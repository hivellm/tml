//! # Intrinsic Dispatch Table
//!
//! Provides O(1) lookup of intrinsic functions by name, replacing the ~700 line
//! if/else chain in emit_call_inst(). Each intrinsic maps to an IntrinsicKind
//! enum value that drives a compact switch dispatch.
//!
//! ## Design
//!
//! The table is a static unordered_map initialized once on first call.
//! Aliases (e.g., "volatile_read" → PtrReadVolatile, "mem_copy" → Memcpy)
//! are separate entries pointing to the same IntrinsicKind.
//!
//! ## Usage
//!
//! ```cpp
//! auto intrinsic = codegen::lookup_intrinsic(base_name);
//! if (intrinsic) {
//!     switch (intrinsic->kind) {
//!     case IntrinsicKind::PtrRead: return emit_intrinsic_ptr_read(i, inst);
//!     // ...
//!     }
//! }
//! ```

#pragma once

#include <optional>
#include <string>

namespace tml::codegen {

enum class IntrinsicKind : uint8_t {
    // Pointer operations
    PtrRead,
    PtrWrite,
    PtrOffset,
    PtrReadVolatile,
    PtrWriteVolatile,
    PtrReadUnaligned,
    PtrWriteUnaligned,

    // Memory allocation / deallocation
    MemFree,

    // Bulk memory operations
    CopyNonOverlapping,
    Memcpy,
    Memmove,
    Memset,
    MemZero,

    // Math — single-arg LLVM intrinsics
    Sqrt,
    Sin,
    Cos,
    Log,
    Exp,
    Floor,
    Ceil,
    Round,
    Trunc,
    Fabs,

    // Math — two-arg LLVM intrinsics
    Pow,
    Minnum,
    Maxnum,
    Copysign,

    // Math — three-arg LLVM intrinsics
    Fma,

    // Conversion
    ToString,
    DebugString,

    // Misc
    BlackBox,
    BlackBoxI64,
    BlackBoxF64,
    StoreByte,
};

struct IntrinsicInfo {
    IntrinsicKind kind;
    int min_args;    // Minimum required arguments (0 = no check)
    bool has_result; // True if the intrinsic produces a result value
};

/// O(1) lookup of intrinsic by base function name (after stripping module path).
/// Returns nullopt for non-intrinsic functions.
std::optional<IntrinsicInfo> lookup_intrinsic(const std::string& func_name);

} // namespace tml::codegen
