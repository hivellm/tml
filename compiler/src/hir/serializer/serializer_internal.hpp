//! # HIR Serializer Internal Types
//!
//! This file defines tag enums and conversion utilities used by the binary
//! serializer to encode HIR nodes. Each HIR variant is assigned a unique
//! tag byte for compact representation.
//!
//! ## Tag Design
//!
//! Tags use `uint8_t` to minimize storage overhead. This limits each
//! category to 256 variants, which is sufficient for HIR:
//!
//! | Category    | Tags  | Description                |
//! |-------------|-------|----------------------------|
//! | TypeTag     | 0-9   | Type representations       |
//! | ExprTag     | 0-28  | Expression variants        |
//! | PatternTag  | 0-8   | Pattern matching variants  |
//! | StmtTag     | 0-1   | Statement variants         |
//! | LiteralTag  | 0-5   | Literal value types        |
//! | BinOpTag    | 0-17  | Binary operators           |
//! | UnaryOpTag  | 0-5   | Unary operators            |
//! | CompoundOpTag| 0-9  | Compound assignment ops    |
//!
//! ## Stability
//!
//! Tag values MUST remain stable across versions for cache compatibility.
//! New tags should be appended at the end, never inserted in the middle.
//! If a tag becomes obsolete, it should be marked deprecated but not removed.
//!
//! ## Conversion Functions
//!
//! The `*_to_tag()` and `tag_to_*()` functions provide safe conversion
//! between HIR enums and serialization tags. They assume HIR enums use
//! the same underlying values, which is enforced by tests.
//!
//! ## See Also
//!
//! - `binary_writer.cpp` - Uses tags for encoding
//! - `binary_reader.cpp` - Uses tags for decoding
//! - `hir_expr.hpp` - HIR enum definitions

#pragma once

#include "hir/hir_expr.hpp"
#include <cstdint>

namespace tml::hir::detail {

// ============================================================================
// Type Tags
// ============================================================================

/// Tags for serializing HirType variants.
///
/// These tags identify the type kind in binary format. The type's
/// string representation is stored separately for reconstruction.
///
/// Layout:
/// - Tags 0-7: Concrete type kinds
/// - Tag 8: Never type (!)
/// - Tag 9: Unknown/null type
enum class TypeTag : uint8_t {
    Primitive = 0,   ///< Built-in types: I32, Bool, etc.
    Named = 1,       ///< User-defined types: structs, enums
    Reference = 2,   ///< Reference type: ref T, mut ref T
    Pointer = 3,     ///< Raw pointer type: *T, *mut T
    Array = 4,       ///< Fixed-size array: [T; N]
    Slice = 5,       ///< Dynamic slice: [T]
    Tuple = 6,       ///< Tuple type: (A, B, C)
    Function = 7,    ///< Function type: func(A) -> B
    Never = 8,       ///< Never type: ! (diverges)
    Unknown = 9,     ///< Null/unresolved type
};

/// Tags for primitive type encoding.
/// These mirror the TML built-in type system.
enum class PrimitiveTag : uint8_t {
    Unit = 0,    ///< () - zero-sized type
    Bool = 1,    ///< Boolean (true/false)
    I8 = 2,      ///< 8-bit signed integer
    I16 = 3,     ///< 16-bit signed integer
    I32 = 4,     ///< 32-bit signed integer (default int)
    I64 = 5,     ///< 64-bit signed integer
    I128 = 6,    ///< 128-bit signed integer
    U8 = 7,      ///< 8-bit unsigned integer
    U16 = 8,     ///< 16-bit unsigned integer
    U32 = 9,     ///< 32-bit unsigned integer
    U64 = 10,    ///< 64-bit unsigned integer
    U128 = 11,   ///< 128-bit unsigned integer
    F32 = 12,    ///< 32-bit float
    F64 = 13,    ///< 64-bit float (default float)
    Char = 14,   ///< Unicode scalar value
    Str = 15,    ///< String slice (&str)
};

// ============================================================================
// Expression Tags
// ============================================================================

/// Tags for serializing HirExpr variants.
///
/// Each tag identifies a specific expression kind. The reader uses
/// these to dispatch to the appropriate deserialization logic.
///
/// Expression groups:
/// - 0-3: Atoms (literal, var, binary, unary)
/// - 4-7: Access (call, method, field, index)
/// - 8-12: Constructors (tuple, array, struct, enum)
/// - 13-18: Control flow (block, if, when, loops)
/// - 19-21: Jumps (return, break, continue)
/// - 22-28: Special (closure, cast, try, await, assign)
enum class ExprTag : uint8_t {
    // Atoms
    Literal = 0,        ///< Constant value: 42, "hello", true
    Var = 1,            ///< Variable reference: x, my_var
    Binary = 2,         ///< Binary operation: a + b
    Unary = 3,          ///< Unary operation: -x, not y

    // Access
    Call = 4,           ///< Function call: foo(x, y)
    MethodCall = 5,     ///< Method call: obj.method(x)
    Field = 6,          ///< Field access: obj.field
    Index = 7,          ///< Index access: arr[i]

    // Constructors
    Tuple = 8,          ///< Tuple: (a, b, c)
    Array = 9,          ///< Array literal: [1, 2, 3]
    ArrayRepeat = 10,   ///< Array repeat: [0; 100]
    Struct = 11,        ///< Struct construction: Point { x, y }
    Enum = 12,          ///< Enum construction: Some(x)

    // Control flow
    Block = 13,         ///< Block expression: { stmts; expr }
    If = 14,            ///< If expression: if cond { } else { }
    When = 15,          ///< Pattern match: when x { ... }
    Loop = 16,          ///< Infinite loop: loop { }
    While = 17,         ///< While loop: while cond { }
    For = 18,           ///< For loop: for x in iter { }

    // Jumps
    Return = 19,        ///< Return: return x
    Break = 20,         ///< Break: break 'label x
    Continue = 21,      ///< Continue: continue 'label

    // Special
    Closure = 22,       ///< Closure: do(x) x + 1
    Cast = 23,          ///< Type cast: x as I64
    Try = 24,           ///< Try operator: expr?
    Await = 25,         ///< Await: expr.await
    Assign = 26,        ///< Assignment: x = y
    CompoundAssign = 27,///< Compound: x += y
    Lowlevel = 28,      ///< Lowlevel block: lowlevel { }
};

// ============================================================================
// Pattern Tags
// ============================================================================

/// Tags for serializing HirPattern variants.
///
/// Patterns are used in `let` bindings and `when` expressions.
enum class PatternTag : uint8_t {
    Wildcard = 0,   ///< Wildcard: _
    Binding = 1,    ///< Variable binding: x, mut x
    Literal = 2,    ///< Literal match: 42, "hi"
    Tuple = 3,      ///< Tuple destructuring: (a, b)
    Struct = 4,     ///< Struct destructuring: Point { x, y }
    Enum = 5,       ///< Enum matching: Some(x), None
    Or = 6,         ///< Alternative: a | b | c
    Range = 7,      ///< Range: 1..10, 1..=10
    Array = 8,      ///< Array: [a, b, ..rest]
};

// ============================================================================
// Statement Tags
// ============================================================================

/// Tags for serializing HirStmt variants.
///
/// HIR has only two statement kinds after lowering.
enum class StmtTag : uint8_t {
    Let = 0,    ///< Variable binding: let x = expr
    Expr = 1,   ///< Expression statement: expr;
};

// ============================================================================
// Literal Value Tags
// ============================================================================

/// Tags for serializing literal values.
///
/// These identify the runtime type of a literal constant.
enum class LiteralTag : uint8_t {
    Int64 = 0,    ///< Signed integer (default)
    UInt64 = 1,   ///< Unsigned integer (u suffix)
    Float64 = 2,  ///< Floating point
    Bool = 3,     ///< Boolean (true/false)
    Char = 4,     ///< Character literal ('x')
    String = 5,   ///< String literal ("hello")
};

// ============================================================================
// Operator Tags
// ============================================================================

/// Tags for binary operators.
///
/// Ordered by precedence groups:
/// - 0-4: Arithmetic
/// - 5-10: Comparison
/// - 11-12: Logical
/// - 13-17: Bitwise
enum class BinOpTag : uint8_t {
    // Arithmetic
    Add = 0,      ///< Addition: +
    Sub = 1,      ///< Subtraction: -
    Mul = 2,      ///< Multiplication: *
    Div = 3,      ///< Division: /
    Mod = 4,      ///< Modulo: %

    // Comparison
    Eq = 5,       ///< Equal: ==
    Ne = 6,       ///< Not equal: !=
    Lt = 7,       ///< Less than: <
    Le = 8,       ///< Less or equal: <=
    Gt = 9,       ///< Greater than: >
    Ge = 10,      ///< Greater or equal: >=

    // Logical
    And = 11,     ///< Logical and: and
    Or = 12,      ///< Logical or: or

    // Bitwise
    BitAnd = 13,  ///< Bitwise and: &
    BitOr = 14,   ///< Bitwise or: |
    BitXor = 15,  ///< Bitwise xor: ^
    Shl = 16,     ///< Shift left: <<
    Shr = 17,     ///< Shift right: >>
};

/// Tags for unary operators.
enum class UnaryOpTag : uint8_t {
    Neg = 0,      ///< Numeric negation: -x
    Not = 1,      ///< Logical not: not x
    BitNot = 2,   ///< Bitwise not: ~x
    Ref = 3,      ///< Reference: ref x
    RefMut = 4,   ///< Mutable reference: mut ref x
    Deref = 5,    ///< Dereference: *x
};

/// Tags for compound assignment operators.
///
/// These correspond to `x op= y` forms.
enum class CompoundOpTag : uint8_t {
    Add = 0,      ///< x += y
    Sub = 1,      ///< x -= y
    Mul = 2,      ///< x *= y
    Div = 3,      ///< x /= y
    Mod = 4,      ///< x %= y
    BitAnd = 5,   ///< x &= y
    BitOr = 6,    ///< x |= y
    BitXor = 7,   ///< x ^= y
    Shl = 8,      ///< x <<= y
    Shr = 9,      ///< x >>= y
};

// ============================================================================
// Tag Conversion Utilities
// ============================================================================
//
// These functions convert between HIR enums and serialization tags.
// They assume the enum values match the tag values (verified by tests).
// Using direct casts is safe because HIR enums use the same underlying type.

/// Convert HirBinOp to BinOpTag for serialization.
inline auto binop_to_tag(HirBinOp op) -> BinOpTag {
    return static_cast<BinOpTag>(static_cast<uint8_t>(op));
}

/// Convert BinOpTag back to HirBinOp for deserialization.
inline auto tag_to_binop(BinOpTag tag) -> HirBinOp {
    return static_cast<HirBinOp>(static_cast<uint8_t>(tag));
}

/// Convert HirUnaryOp to UnaryOpTag for serialization.
inline auto unaryop_to_tag(HirUnaryOp op) -> UnaryOpTag {
    return static_cast<UnaryOpTag>(static_cast<uint8_t>(op));
}

/// Convert UnaryOpTag back to HirUnaryOp for deserialization.
inline auto tag_to_unaryop(UnaryOpTag tag) -> HirUnaryOp {
    return static_cast<HirUnaryOp>(static_cast<uint8_t>(tag));
}

/// Convert HirCompoundOp to CompoundOpTag for serialization.
inline auto compoundop_to_tag(HirCompoundOp op) -> CompoundOpTag {
    return static_cast<CompoundOpTag>(static_cast<uint8_t>(op));
}

/// Convert CompoundOpTag back to HirCompoundOp for deserialization.
inline auto tag_to_compoundop(CompoundOpTag tag) -> HirCompoundOp {
    return static_cast<HirCompoundOp>(static_cast<uint8_t>(tag));
}

} // namespace tml::hir::detail
