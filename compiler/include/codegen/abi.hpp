#pragma once

//! Centralized ABI Classification Module
//!
//! Replaces scattered `starts_with("%struct.")` checks in MIR codegen with
//! proper type-based ABI classification. Follows Win64 calling convention rules:
//!
//!   - Scalars (integers, floats, pointers, booleans) -> Direct (register)
//!   - Aggregates <= 8 bytes with power-of-2 size -> Direct (register)
//!   - Aggregates > 8 bytes -> Indirect (passed/returned by pointer)
//!   - Unit/void -> Ignore (not passed)
//!   - Fat pointers (slice, dyn, closure) -> Pair (data + vtable/len)
//!
//! Reference: Microsoft x64 ABI, rustc_target::abi::call::PassMode

#include "mir/mir.hpp"

#include <string>
#include <vector>

namespace tml::codegen {

/// How a value is passed in the calling convention.
/// Matches Rust's PassMode (rustc_target::abi::call::PassMode).
enum class PassMode {
    Direct,   ///< Passed directly as a scalar (i32, i64, ptr, float, etc.)
    Indirect, ///< Passed by pointer (large aggregates, sret returns)
    Ignore,   ///< Not passed at all (Unit/void)
    Pair,     ///< Two-part: fat pointers (data + vtable/len), closures, dyn traits
};

/// ABI info for a single argument or return value.
struct ArgABI {
    PassMode mode = PassMode::Direct;
    std::string llvm_type; ///< The LLVM IR type string (e.g., "i64", "ptr", "%struct.Foo")
    bool sret = false;     ///< True if this is a hidden sret return parameter
};

/// ABI info for an entire function signature.
struct FnABI {
    ArgABI ret;
    std::vector<ArgABI> args;
};

/// Classify a MIR type for Win64 ABI passing.
///
/// Rules (from Win64 convention):
/// - Scalars (i8-i128, f32, f64, ptr, bool) -> Direct
/// - Unit/void -> Ignore
/// - Fat pointers (slice, dyn, closure/function) -> Pair
/// - Aggregates <= 8 bytes with power-of-2 size -> Direct (passed in register)
/// - Aggregates > 8 bytes -> Indirect (passed by pointer)
/// - Arrays -> Indirect (always by pointer)
/// - SIMD vectors -> Direct (passed in vector register)
ArgABI classify_type(const mir::MirTypePtr& type);

/// Classify a return type. Large aggregates use sret (hidden first parameter).
/// Same rules as classify_type but sets `sret = true` for Indirect returns.
ArgABI classify_return(const mir::MirTypePtr& type);

/// Compute the full FnABI for a MIR function.
///
/// - Classifies return type (with sret if needed)
/// - Classifies each parameter type
/// - Respects existing sret annotations on the function
FnABI compute_fn_abi(const mir::Function& func);

/// Returns true if an LLVM IR type string represents an aggregate type
/// (struct, enum, class, union, tuple) that should be passed indirectly on Win64.
///
/// This is a transitional helper that replaces scattered `starts_with("%struct.")`
/// checks throughout MIR codegen. Prefer `classify_type(MirTypePtr)` in new code
/// where the MIR type is available.
bool is_aggregate_llvm_type(const std::string& llvm_type);

/// Compute the size in bytes of a MIR type for ABI classification purposes.
///
/// Returns 0 for types that cannot be sized at the MIR level (opaque structs/enums
/// whose field layout is not available from MIR type info alone).
/// For unsized types, callers should fall back to Indirect passing.
size_t compute_type_size(const mir::MirTypePtr& type);

/// Returns true if the given size is a power of two.
/// Used for Win64 ABI: aggregates <= 8 bytes must also be power-of-2 for Direct.
bool is_power_of_two(size_t n);

} // namespace tml::codegen
