#pragma once

//! # CGValue — Typed Codegen Value Wrapper
//!
//! Wraps an LLVM SSA register with the metadata needed to make correct
//! ABI and memory decisions throughout MIR codegen. Replaces the loose
//! `(string reg, string type)` pairs that were scattered through the
//! codegen pass with a single type that knows:
//!
//!   - The LLVM register name (%t42, %alloca.3, etc.)
//!   - The LLVM IR type string (i64, ptr, %struct.Foo, etc.)
//!   - Whether it is an immediate value or an address (pointer)
//!   - The original MIR type, for ABI decisions at call sites

#include "mir/mir.hpp"

#include <string>

namespace tml::codegen {

/// How a codegen value is represented in LLVM IR.
enum class CGValueKind : uint8_t {
    Immediate,  ///< SSA register holding the value directly (e.g. %t1 of type i64)
    Address,    ///< SSA register holding a pointer to the value (alloca, GEP result)
    FatPointer, ///< Two-part value: {data_ptr, metadata} (slices, dyn objects)
    ZeroSized,  ///< Unit/void — no register, no memory
};

/// A typed codegen value — wraps an LLVM SSA register with type info.
///
/// Replaces the loose `(string reg, string type)` pairs scattered throughout
/// the MIR codegen. Every CGValue knows:
///   - Its LLVM register name (%t42, %alloca.3, etc.)
///   - Its LLVM type string (i64, ptr, %struct.Foo, etc.)
///   - Whether it is an immediate value or an address
///   - Its original MIR type (for ABI decisions at call sites)
struct CGValue {
    std::string reg;       ///< LLVM SSA register (e.g. "%t42")
    std::string llvm_type; ///< LLVM IR type (e.g. "i64", "ptr", "%struct.Foo")
    CGValueKind kind = CGValueKind::Immediate;
    mir::MirTypePtr mir_type; ///< Original MIR type (nullable)

    // ── Queries ────────────────────────────────────────────────────────────────

    /// True if this value is an aggregate (struct, enum, tuple, array).
    [[nodiscard]] bool is_aggregate() const;

    /// True if this value is held as a pointer (Address or FatPointer).
    [[nodiscard]] bool is_pointer() const {
        return kind == CGValueKind::Address || kind == CGValueKind::FatPointer;
    }

    /// True if this value has no runtime representation (Unit).
    [[nodiscard]] bool is_zero_sized() const {
        return kind == CGValueKind::ZeroSized;
    }

    /// True if this is an immediate scalar value.
    [[nodiscard]] bool is_immediate() const {
        return kind == CGValueKind::Immediate;
    }

    // ── Factory methods ────────────────────────────────────────────────────────

    /// Create a CGValue representing an LLVM SSA register holding a scalar.
    static CGValue immediate(std::string reg, std::string llvm_type,
                             mir::MirTypePtr mir_type = nullptr);

    /// Create a CGValue representing a pointer to a value in memory.
    /// @param pointee_type  The type that the pointer points to (NOT "ptr").
    static CGValue address(std::string reg, std::string pointee_type,
                           mir::MirTypePtr mir_type = nullptr);

    /// Create a CGValue representing a zero-sized (Unit) value.
    static CGValue zero_sized();

    // Conversion methods are declared after the result structs below.
};

// ── Conversion result types ────────────────────────────────────────────────────
// These are defined after CGValue so they can hold a CGValue by value.

/// Result of spilling an Immediate value to an alloca.
/// Returned by CGValue::to_address().
struct CGSpillResult {
    CGValue value;         ///< The address-mode CGValue (kind=Address)
    std::string alloca_ir; ///< "  %spill.N = alloca <type>"
    std::string store_ir;  ///< "  store <type> %reg, ptr %spill.N"
};

/// Result of loading an Address value to an immediate register.
/// Returned by CGValue::to_immediate().
struct CGLoadResult {
    CGValue value;       ///< The immediate-mode CGValue (kind=Immediate)
    std::string load_ir; ///< "  %load.N = load <type>, ptr %reg"
};

// ── Conversion methods (declared here, defined in cg_value.cpp) ───────────────
// These are free functions so they avoid the circular dependency between
// CGValue and its result types.

/// If value is Immediate, spill it to an alloca and return the alloca's
/// CGValue (kind=Address) plus the IR to emit.
/// If value is already Address (or ZeroSized/FatPointer), returns value
/// unchanged with empty IR strings — no new instructions needed.
/// @param counter  A per-function integer counter; incremented for each
///                 alloca generated so names are unique (%spill.0, etc.).
[[nodiscard]] CGSpillResult cg_to_address(const CGValue& value, int& counter);

/// If value is Address, load it and return an Immediate CGValue plus the
/// IR to emit.
/// If value is already Immediate (or ZeroSized/FatPointer), returns value
/// unchanged with an empty IR string — no new instructions needed.
/// @param counter  A per-function integer counter; incremented for each
///                 load generated so names are unique (%load.0, etc.).
[[nodiscard]] CGLoadResult cg_to_immediate(const CGValue& value, int& counter);

} // namespace tml::codegen
