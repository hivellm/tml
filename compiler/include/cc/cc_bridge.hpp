//! # C Frontend FFI Bridge
//!
//! Plain-C ABI surface between the C++ `tml cc` CLI driver
//! (`compiler/src/cli/commands/cmd_cc.cpp`) and the TML-authored C17
//! frontend (`compiler-tml/src/cc/`). The C++ side knows how to read
//! files, discover include paths, invoke the phase23a preprocessor
//! (already C++), and hand the resulting token stream to the TML-side
//! lexer / parser / lowerer. The TML side's types (`CToken`,
//! `CTranslationUnit`, `MirModule`, …) are opaque to C++; this bridge
//! wraps every pointer in a handle type and exposes a small set of
//! entry points callable through `@extern("c")` on the TML side.
//!
//! ## Pipeline overview
//!
//! ```text
//!   cmd_cc.cpp
//!        |
//!        +------- file bytes, include-paths, -D defines -----------+
//!        |                                                         |
//!        v                                                         |
//!   cc_bridge_preproc  --->  CcTokenStream  (opaque PpToken list)  |
//!                                 |                                |
//!                                 v                                |
//!                    cc_bridge_parse  --->  CcTranslationUnit      |
//!                                               |                  |
//!                                               v                  |
//!                              cc_bridge_lower  --->  CcMirModule  |
//!                                                          |       |
//!                                                          v       v
//!                            existing MIR -> LLVM backend (llvm_backend.cpp)
//!                                                          |
//!                                                          v
//!                                                     .obj / .ll
//! ```
//!
//! ## Ownership contract
//!
//! Every non-NULL handle returned by one of the `cc_bridge_*`
//! constructors MUST eventually be freed with the matching
//! `cc_bridge_free_*` function. Handles are single-owner — passing one
//! into the next pipeline stage (e.g. the token stream into
//! `cc_bridge_parse`) transfers ownership; after a successful transfer
//! the caller MUST NOT free the input handle. On error, the constructor
//! returns NULL and does NOT consume the input; the caller remains
//! responsible for freeing it.
//!
//! Error objects live inside the returned diagnostic buffer and are
//! cleaned up with `cc_bridge_free_diagnostics`. They do not alias any
//! input buffer.
//!
//! ## Threading
//!
//! Handles are NOT thread-safe. Pipe one file through one thread end to
//! end; if `tml cc -j N` is ever added, each worker gets its own set of
//! handles.
//!
//! ## Target ABIs
//!
//! `cc_bridge_lower` takes an explicit `CcAbiTarget` so the caller can
//! select Windows x64 LLP64 vs System V AMD64 LP64 vs i686 vs aarch64.
//! The default (`CC_ABI_TARGET_HOST`) matches the host triple — useful
//! for bootstrap / self-compile scenarios where the produced `.obj`
//! links into the same `tml.exe`.
//!
//! ## Why a handle-based ABI?
//!
//! The TML types hold `Heap[T]` payloads whose layout is
//! codegen-dependent. Exposing raw pointers across the FFI seam would
//! make the ABI fragile every time the TML compiler changes its memory
//! representation. Handles are opaque, version-stable, and cheap (one
//! pointer).

#pragma once

#include <cstddef>
#include <cstdint>

// ============================================================================
// C ABI
// ============================================================================
//
// The full API is declared with C linkage so the TML side can resolve
// every symbol through `@extern("c")` without any name mangling.

extern "C" {

// ============================================================================
// Target selection
// ============================================================================

/// Target ABI for the produced MIR / object file.
///
/// Matches the TML-side `CAbiTarget` enum in
/// `compiler-tml/src/cc/types.tml` — the wire format is a single
/// `int32_t` so callers can pass these across the FFI seam without
/// dealing with struct layout.
enum CcAbiTarget : int32_t {
    CC_ABI_TARGET_HOST             = 0,
    CC_ABI_TARGET_WINDOWS_X64_LLP64 = 1,
    CC_ABI_TARGET_SYSV_AMD64_LP64   = 2,
    CC_ABI_TARGET_I686              = 3,
    CC_ABI_TARGET_AARCH64           = 4,
};

// ============================================================================
// Handle types
// ============================================================================
//
// Forward declarations only; callers see these as opaque pointers.

/// Stream of preprocessor tokens produced by the phase23a preprocessor.
/// Owned by the caller of `cc_bridge_preproc`; transferred into
/// `cc_bridge_parse`.
struct CcTokenStream;

/// A parsed C translation unit (pre-lowering). Owned by the caller of
/// `cc_bridge_parse`; transferred into `cc_bridge_lower`.
struct CcTranslationUnit;

/// The lowered MIR module, ready to be handed to the existing
/// `compiler/src/backend/llvm_backend.cpp` pipeline.
struct CcMirModule;

/// A diagnostic sink that accumulates errors across all bridge calls
/// for a given compile unit. Owned by the caller; populated by each
/// bridge entry point that encounters a problem.
struct CcDiagnostics;

// ============================================================================
// Diagnostics
// ============================================================================

/// A single message surfaced by any bridge entry point.
struct CcDiagnostic {
    const char* file;    ///< Source file (null-terminated); may be "" for lex-time diags.
    int32_t     line;    ///< 1-based; 0 when unknown.
    int32_t     column;  ///< 1-based; 0 when unknown.
    int32_t     severity; ///< 0=note, 1=warning, 2=error, 3=fatal.
    const char* message; ///< Null-terminated UTF-8.
};

/// Allocate a fresh, empty diagnostic sink. Never returns NULL on
/// success (abort on out-of-memory — the only failure mode).
CcDiagnostics* cc_bridge_diagnostics_new();

/// Return the number of diagnostics accumulated so far.
size_t cc_bridge_diagnostics_count(const CcDiagnostics* diags);

/// Fetch the i-th diagnostic (0-based). Returns a zero-initialised
/// record when `i >= count`; the returned struct's `const char*` fields
/// are valid until `cc_bridge_free_diagnostics(diags)` is called.
CcDiagnostic cc_bridge_diagnostics_get(const CcDiagnostics* diags, size_t i);

/// Free the diagnostic sink. Safe to call with NULL (no-op).
void cc_bridge_free_diagnostics(CcDiagnostics* diags);

// ============================================================================
// Pipeline entry points
// ============================================================================

/// Run the phase23a preprocessor over a C source buffer.
///
/// @param source         Full file bytes, NUL-terminated UTF-8.
/// @param filename       Null-terminated file path (used in diagnostics).
/// @param include_paths  NULL-terminated array of include directory
///                       paths (each element null-terminated).
///                       May be NULL for no includes.
/// @param defines        NULL-terminated array of `-D NAME[=VALUE]`
///                       strings; each element null-terminated.
///                       May be NULL for no predefines.
/// @param diags          Diagnostic sink; receives preproc errors.
/// @return An owned `CcTokenStream` on success, or NULL on fatal error
///         (one or more diagnostics with severity >= 3 will be
///         appended to `diags`).
CcTokenStream* cc_bridge_preproc(const char*        source,
                                 const char*        filename,
                                 const char* const* include_paths,
                                 const char* const* defines,
                                 CcDiagnostics*     diags);

/// Free a token stream. Safe to call with NULL.
void cc_bridge_free_token_stream(CcTokenStream* tokens);

/// Parse a preprocessor token stream into a C translation unit.
///
/// Consumes `tokens` on success: the caller MUST NOT call
/// `cc_bridge_free_token_stream` on it afterwards. On failure (returns
/// NULL), the caller retains ownership of `tokens` and must free it
/// itself.
///
/// @param tokens  Owned input (consumed on success).
/// @param diags   Diagnostic sink; receives parse errors.
CcTranslationUnit* cc_bridge_parse(CcTokenStream* tokens, CcDiagnostics* diags);

/// Free a translation unit. Safe to call with NULL.
void cc_bridge_free_translation_unit(CcTranslationUnit* tu);

/// Lower a parsed translation unit to MIR.
///
/// Consumes `tu` on success (same ownership rules as
/// `cc_bridge_parse`).
///
/// @param tu           Owned input (consumed on success).
/// @param target       Target ABI for size/alignment/calling convention
///                     decisions.
/// @param module_name  Null-terminated module identifier embedded in
///                     the MIR output.
/// @param diags        Diagnostic sink; receives lowering errors.
CcMirModule* cc_bridge_lower(CcTranslationUnit* tu,
                             CcAbiTarget        target,
                             const char*        module_name,
                             CcDiagnostics*     diags);

/// Free a MIR module. Safe to call with NULL.
///
/// Note: once the MIR has been handed into the LLVM backend for
/// codegen, the backend makes its own internal copy; freeing here is
/// always safe.
void cc_bridge_free_mir_module(CcMirModule* mir);

// ============================================================================
// MIR extraction
// ============================================================================
//
// The C++ LLVM backend consumes `mir::Module` by const-reference. The
// bridge wraps a TML-owned `MirModule` value; `cc_bridge_mir_borrow`
// surfaces a const pointer to the underlying C++-compatible view so
// the caller can hand it into `compiler/src/backend/llvm_backend.cpp`
// without allocating a second copy.
//
// The returned pointer is valid until `cc_bridge_free_mir_module`
// is called on the parent handle.

} // extern "C"

// The `mir::Module` type is exposed only on the C++ side; keep the
// accessor outside the `extern "C"` block so it can return a C++ type.
namespace tml::mir {
struct Module;
}

extern "C" {

/// Borrow the underlying `mir::Module` pointer for consumption by the
/// existing LLVM backend. Do NOT free or mutate the returned pointer;
/// lifetime is tied to the parent `CcMirModule`.
const tml::mir::Module* cc_bridge_mir_borrow(const CcMirModule* mir);

} // extern "C"
