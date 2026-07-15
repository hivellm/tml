TML_MODULE("cc")

//! # C Frontend FFI Bridge — Implementation
//!
//! Ships the handle-type lifecycle for `CcDiagnostics` plus
//! stub implementations for the three pipeline entry points. The stubs
//! populate a single fatal diagnostic and return NULL, so callers can
//! already integrate against the real ABI shape while the wire-up to
//! the TML-compiled lexer / parser / lowerer lands incrementally.
//!
//! Currently implemented:
//! - `cc_bridge_diagnostics_new`
//! - `cc_bridge_diagnostics_count`
//! - `cc_bridge_diagnostics_get`
//! - `cc_bridge_free_diagnostics`
//! - `cc_bridge_free_token_stream` (no-op; nothing to allocate yet)
//! - `cc_bridge_free_translation_unit` (no-op)
//! - `cc_bridge_free_mir_module` (no-op)
//! - `cc_bridge_mir_borrow` (returns NULL; see below)
//!
//! Stubbed (append fatal diagnostic, return NULL):
//! - `cc_bridge_preproc`
//! - `cc_bridge_parse`
//! - `cc_bridge_lower`
//!
//! Filling in the stubs is phase24 Phase 2.2 follow-up — it depends on
//! the TML-compiled cc modules being reachable from this C++ layer.
//! Two candidate paths:
//!
//! 1. **Subprocess dispatch** — mirrors `tml coverage` / `coverage_cli.exe`.
//!    The bridge spawns a pre-compiled `cc_driver.exe` (to be authored
//!    under `compiler-tml/src/cc/bin/`) and streams serialized input
//!    in / MIR bytes out. Simplest and most robust.
//! 2. **Static link** — bake the TML cc modules into `tml.exe` at build
//!    time. Requires a new build hook that runs `tml build` during the
//!    C++ compilation step. Zero subprocess overhead but adds a
//!    bootstrap dependency.
//!
//! Decision deferred until `cmd_cc.cpp` (Phase 3) clarifies the
//! invocation pattern the CLI needs.

#include "cc/cc_bridge.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

/// Owning, null-terminated copy of a C string. Returns nullptr when
/// the input is nullptr.
char* dup_cstr(const char* s) {
    if (s == nullptr) {
        return nullptr;
    }
    const size_t len = std::strlen(s);
    char* out = static_cast<char*>(std::malloc(len + 1));
    if (out == nullptr) {
        std::abort();
    }
    std::memcpy(out, s, len + 1);
    return out;
}

} // namespace

// ============================================================================
// CcDiagnostics
// ============================================================================
//
// The diagnostic sink keeps every string it receives alive for its own
// lifetime. Callers get back `const char*` views that are valid until
// `cc_bridge_free_diagnostics` is called. This matches the ABI the
// header documents (no separate "borrow" lifetime).

struct CcDiagnosticEntry {
    char*   file;
    int32_t line;
    int32_t column;
    int32_t severity;
    char*   message;

    CcDiagnosticEntry(const char* file_, int32_t line_, int32_t column_,
                      int32_t severity_, const char* message_)
        : file(dup_cstr(file_)), line(line_), column(column_),
          severity(severity_), message(dup_cstr(message_)) {}

    CcDiagnosticEntry(const CcDiagnosticEntry&)            = delete;
    CcDiagnosticEntry& operator=(const CcDiagnosticEntry&) = delete;

    CcDiagnosticEntry(CcDiagnosticEntry&& other) noexcept
        : file(other.file), line(other.line), column(other.column),
          severity(other.severity), message(other.message) {
        other.file    = nullptr;
        other.message = nullptr;
    }
    CcDiagnosticEntry& operator=(CcDiagnosticEntry&& other) noexcept {
        if (this != &other) {
            std::free(file);
            std::free(message);
            file          = other.file;
            line          = other.line;
            column        = other.column;
            severity      = other.severity;
            message       = other.message;
            other.file    = nullptr;
            other.message = nullptr;
        }
        return *this;
    }

    ~CcDiagnosticEntry() {
        std::free(file);
        std::free(message);
    }
};

struct CcDiagnostics {
    std::vector<CcDiagnosticEntry> entries;
};

/// Push a fatal diagnostic recording that this bridge entry point is
/// still a stub. Shared between `cc_bridge_preproc`,
/// `cc_bridge_parse`, and `cc_bridge_lower` so the pipeline can make
/// progress while the real dispatch is being wired in.
static void push_stub_diag(CcDiagnostics* diags, const char* stage) {
    if (diags == nullptr) {
        return;
    }
    std::string msg = "cc_bridge ";
    msg += stage;
    msg += " is not yet wired to the TML-compiled cc pipeline — "
           "tracked under phase24 Phase 2.2.";
    diags->entries.emplace_back("", 0, 0, /*severity=*/3, msg.c_str());
}

// ============================================================================
// C ABI surface
// ============================================================================

extern "C" {

CcDiagnostics* cc_bridge_diagnostics_new() {
    return new CcDiagnostics();
}

size_t cc_bridge_diagnostics_count(const CcDiagnostics* diags) {
    if (diags == nullptr) {
        return 0;
    }
    return diags->entries.size();
}

CcDiagnostic cc_bridge_diagnostics_get(const CcDiagnostics* diags, size_t i) {
    CcDiagnostic out{};
    if (diags == nullptr || i >= diags->entries.size()) {
        return out;
    }
    const auto& e = diags->entries[i];
    out.file      = e.file != nullptr ? e.file : "";
    out.line      = e.line;
    out.column    = e.column;
    out.severity  = e.severity;
    out.message   = e.message != nullptr ? e.message : "";
    return out;
}

void cc_bridge_free_diagnostics(CcDiagnostics* diags) {
    delete diags;
}

CcTokenStream* cc_bridge_preproc(const char* /*source*/,
                                 const char* /*filename*/,
                                 const char* const* /*include_paths*/,
                                 const char* const* /*defines*/,
                                 CcDiagnostics* diags) {
    push_stub_diag(diags, "preproc");
    return nullptr;
}

void cc_bridge_free_token_stream(CcTokenStream* /*tokens*/) {
    // No-op until cc_bridge_preproc actually allocates.
}

CcTranslationUnit* cc_bridge_parse(CcTokenStream* /*tokens*/,
                                   CcDiagnostics* diags) {
    push_stub_diag(diags, "parse");
    return nullptr;
}

void cc_bridge_free_translation_unit(CcTranslationUnit* /*tu*/) {
    // No-op until cc_bridge_parse actually allocates.
}

CcMirModule* cc_bridge_lower(CcTranslationUnit* /*tu*/,
                             CcAbiTarget /*target*/,
                             const char* /*module_name*/,
                             CcDiagnostics* diags) {
    push_stub_diag(diags, "lower");
    return nullptr;
}

void cc_bridge_free_mir_module(CcMirModule* /*mir*/) {
    // No-op until cc_bridge_lower actually allocates.
}

const tml::mir::Module* cc_bridge_mir_borrow(const CcMirModule* /*mir*/) {
    return nullptr;
}

} // extern "C"
