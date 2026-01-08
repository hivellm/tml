//! # HIR Patterns
//!
//! This module defines pattern types for the HIR. Patterns are used for
//! destructuring and matching values in various contexts.
//!
//! ## Where Patterns Appear
//!
//! Patterns appear in several TML constructs:
//! - **`let` bindings**: `let (x, y) = point` - destructure into variables
//! - **`when` arms**: `when value { Just(x) => ... }` - match and destructure
//! - **`for` loops**: `for (k, v) in map` - iterate with destructuring
//! - **Function parameters**: `func add((x, y): Point)` - parameter patterns
//!
//! ## Pattern Kinds
//!
//! | Pattern | Syntax | Description |
//! |---------|--------|-------------|
//! | Wildcard | `_` | Matches any value, discards it |
//! | Binding | `x`, `mut x` | Binds matched value to a variable |
//! | Literal | `42`, `true`, `"hello"` | Matches exact compile-time value |
//! | Tuple | `(a, b, c)` | Destructures tuple by position |
//! | Struct | `Point { x, y, .. }` | Destructures struct by field name |
//! | Enum | `Just(v)`, `Nothing` | Matches enum variant, destructures payload |
//! | Or | `a \| b \| c` | Matches if any alternative matches |
//! | Range | `0 to 10`, `'a' through 'z'` | Matches value within range |
//! | Array | `[a, b, ..rest]` | Destructures array/slice |
//!
//! ## Pattern Exhaustiveness
//!
//! HIR does not verify pattern exhaustiveness - that is done during type
//! checking before HIR lowering. HIR patterns are always well-typed and
//! exhaustiveness-checked.
//!
//! ## Type Information
//!
//! Every pattern carries its resolved type (`HirType`). For binding patterns,
//! this is the type of the variable being bound. For other patterns, this
//! is the type of the value being matched.
//!
//! ## Example: Pattern Lowering
//!
//! TML source:
//! ```tml
//! let Point { x, y } = get_point()
//! ```
//!
//! HIR (conceptual):
//! ```
//! HirLetStmt {
//!     pattern: HirStructPattern {
//!         struct_name: "Point",
//!         fields: [("x", HirBindingPattern("x")), ("y", HirBindingPattern("y"))],
//!         type: Point
//!     },
//!     init: HirCallExpr("get_point", ...)
//! }
//! ```
//!
//! ## See Also
//!
//! - `docs/specs/31-HIR.md` - Complete HIR documentation
//! - `hir_expr.hpp` - Expressions that use patterns (when, for)
//! - `hir_stmt.hpp` - Let statements that use patterns

#pragma once

#include "hir/hir_id.hpp"

namespace tml::hir {

// ============================================================================
// Pattern Definitions
// ============================================================================

/// Wildcard pattern: `_`
///
/// Matches any value and discards it. Useful for:
/// - Ignoring values in tuple/struct destructuring: `let (x, _) = pair`
/// - Catch-all case in `when`: `when x { _ => default_value }`
///
/// ## Fields
/// - `id`: Unique identifier for this pattern node
/// - `span`: Source location of the `_` token
struct HirWildcardPattern {
    HirId id;
    SourceSpan span;
};

/// Binding pattern: `x` or `mut x`
///
/// Binds the matched value to a new variable in the current scope.
///
/// ## Fields
/// - `id`: Unique identifier for this pattern node
/// - `name`: The variable name to bind
/// - `is_mut`: Whether the binding is mutable (`mut x` vs `x`)
/// - `type`: The type of the bound variable
/// - `span`: Source location
///
/// ## Examples
/// - `let x = 5` - immutable binding
/// - `let mut count = 0` - mutable binding
/// - `when opt { Just(value) => ... }` - binding in enum pattern
struct HirBindingPattern {
    HirId id;
    std::string name;
    bool is_mut;
    HirType type;
    SourceSpan span;
};

/// Literal pattern: `42`, `"hello"`, `true`
///
/// Matches a specific compile-time constant value. Used in `when` expressions
/// for matching exact values.
///
/// ## Fields
/// - `id`: Unique identifier for this pattern node
/// - `value`: The literal value to match (integer, float, bool, char, or string)
/// - `type`: The type of the literal
/// - `span`: Source location
///
/// ## Example
/// ```tml
/// when status_code {
///     200 => "OK",
///     404 => "Not Found",
///     _ => "Unknown"
/// }
/// ```
struct HirLiteralPattern {
    HirId id;
    std::variant<int64_t, uint64_t, double, bool, char, std::string> value;
    HirType type;
    SourceSpan span;
};

/// Tuple pattern: `(a, b, c)`
///
/// Destructures a tuple by position. The number of elements must match
/// the tuple being destructured.
///
/// ## Fields
/// - `id`: Unique identifier for this pattern node
/// - `elements`: Sub-patterns for each tuple element
/// - `type`: The tuple type being matched
/// - `span`: Source location
///
/// ## Example
/// ```tml
/// let (x, y, z) = get_coordinates()
/// for (key, value) in map.entries()
/// ```
struct HirTuplePattern {
    HirId id;
    std::vector<HirPatternPtr> elements;
    HirType type;
    SourceSpan span;
};

/// Struct pattern: `Point { x, y }` or `Point { x, .. }`
///
/// Destructures a struct by field name. Fields can be:
/// - Named bindings: `Point { x: px, y: py }` - bind to different names
/// - Shorthand: `Point { x, y }` - bind to same name as field
/// - Partial with rest: `Point { x, .. }` - ignore remaining fields
///
/// ## Fields
/// - `id`: Unique identifier for this pattern node
/// - `struct_name`: Name of the struct being matched
/// - `fields`: List of (field_name, sub_pattern) pairs
/// - `has_rest`: Whether `..` is present (ignores unmatched fields)
/// - `type`: The struct type being matched
/// - `span`: Source location
struct HirStructPattern {
    HirId id;
    std::string struct_name;
    std::vector<std::pair<std::string, HirPatternPtr>> fields;
    bool has_rest; // `..` present
    HirType type;
    SourceSpan span;
};

/// Enum variant pattern: `Just(x)`, `Nothing`, `Color::Red`
///
/// Matches a specific enum variant and optionally destructures its payload.
///
/// ## Fields
/// - `id`: Unique identifier for this pattern node
/// - `enum_name`: Name of the enum type (e.g., "Maybe", "Color")
/// - `variant_name`: Name of the variant (e.g., "Just", "Nothing", "Red")
/// - `variant_index`: Numeric index of the variant (resolved during lowering)
/// - `payload`: Sub-patterns for variant payload (if any)
/// - `type`: The enum type being matched
/// - `span`: Source location
///
/// ## Variant Index
///
/// The `variant_index` is resolved during HIR lowering and corresponds to
/// the declaration order in the enum definition:
/// ```tml
/// type Maybe[T] { Just(T), Nothing }  // Just=0, Nothing=1
/// ```
struct HirEnumPattern {
    HirId id;
    std::string enum_name;
    std::string variant_name;
    int variant_index;
    std::optional<std::vector<HirPatternPtr>> payload;
    HirType type;
    SourceSpan span;
};

/// Or pattern: `a | b | c`
///
/// Matches if any of the alternative patterns match. All alternatives
/// must bind the same variables with the same types.
///
/// ## Fields
/// - `id`: Unique identifier for this pattern node
/// - `alternatives`: List of alternative patterns (at least 2)
/// - `type`: The type being matched (same for all alternatives)
/// - `span`: Source location
///
/// ## Example
/// ```tml
/// when direction {
///     North | South => "vertical",
///     East | West => "horizontal"
/// }
/// ```
///
/// ## Binding Requirements
///
/// When alternatives contain bindings, all alternatives must bind
/// the same names with compatible types:
/// ```tml
/// when result {
///     Ok(x) | Err(x) => use(x)  // x bound in both
/// }
/// ```
struct HirOrPattern {
    HirId id;
    std::vector<HirPatternPtr> alternatives;
    HirType type;
    SourceSpan span;
};

/// Range pattern: `0 to 10`, `'a' through 'z'`
///
/// Matches values within a numeric or character range.
///
/// ## Fields
/// - `id`: Unique identifier for this pattern node
/// - `start`: Lower bound (inclusive), None for unbounded start
/// - `end`: Upper bound, None for unbounded end
/// - `inclusive`: If true, end is inclusive (`through`); if false, exclusive (`to`)
/// - `type`: The numeric/char type being matched
/// - `span`: Source location
///
/// ## Range Syntax
/// - `0 to 10` - matches 0..9 (exclusive end)
/// - `0 through 10` - matches 0..10 (inclusive end)
/// - `'a' through 'z'` - matches lowercase letters
///
/// ## Compile-Time Bounds
///
/// Range bounds must be compile-time constants, not arbitrary expressions.
/// This is enforced during lowering.
struct HirRangePattern {
    HirId id;
    std::optional<int64_t> start;
    std::optional<int64_t> end;
    bool inclusive;
    HirType type;
    SourceSpan span;
};

/// Array/slice pattern: `[a, b, c]` or `[head, ..rest]`
///
/// Destructures an array or slice by position, optionally capturing
/// remaining elements with a rest pattern.
///
/// ## Fields
/// - `id`: Unique identifier for this pattern node
/// - `elements`: Sub-patterns for positional elements
/// - `rest`: Optional pattern for remaining elements (captures as slice)
/// - `type`: The array/slice type being matched
/// - `span`: Source location
///
/// ## Examples
/// ```tml
/// let [first, second, ..rest] = array   // Capture first two, rest in slice
/// let [a, b, c] = triple                 // Exact match, 3 elements
/// let [head, ..] = list                  // Just get first, ignore rest
/// ```
struct HirArrayPattern {
    HirId id;
    std::vector<HirPatternPtr> elements;
    std::optional<HirPatternPtr> rest;
    HirType type;
    SourceSpan span;
};

// ============================================================================
// HirPattern Container
// ============================================================================

/// A pattern for destructuring values in let bindings, when arms, etc.
///
/// HirPattern is a variant type that can hold any of the pattern kinds
/// defined above. It provides common accessors for ID, type, and span
/// that work regardless of the underlying pattern kind.
///
/// ## Type Checking
///
/// Use `is<T>()` to check the pattern kind before accessing with `as<T>()`:
/// ```cpp
/// if (pattern.is<HirBindingPattern>()) {
///     const auto& binding = pattern.as<HirBindingPattern>();
///     std::cout << "Binds to: " << binding.name << std::endl;
/// }
/// ```
struct HirPattern {
    std::variant<HirWildcardPattern, HirBindingPattern, HirLiteralPattern, HirTuplePattern,
                 HirStructPattern, HirEnumPattern, HirOrPattern, HirRangePattern, HirArrayPattern>
        kind;

    /// Get the HIR ID for this pattern.
    [[nodiscard]] auto id() const -> HirId;

    /// Get the type of this pattern.
    /// For bindings, this is the type of the bound variable.
    /// For other patterns, this is the type being matched.
    [[nodiscard]] auto type() const -> HirType;

    /// Get the source location of this pattern.
    [[nodiscard]] auto span() const -> SourceSpan;

    /// Check if this pattern is of a specific kind.
    /// @tparam T One of the HirXxxPattern types
    /// @return true if this pattern holds a T
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Get this pattern as a specific kind (mutable).
    /// @tparam T One of the HirXxxPattern types
    /// @return Reference to the contained pattern
    /// @throws std::bad_variant_access if pattern is not of type T
    template <typename T> [[nodiscard]] auto as() -> T& {
        return std::get<T>(kind);
    }

    /// Get this pattern as a specific kind (const).
    /// @tparam T One of the HirXxxPattern types
    /// @return Const reference to the contained pattern
    /// @throws std::bad_variant_access if pattern is not of type T
    template <typename T> [[nodiscard]] auto as() const -> const T& {
        return std::get<T>(kind);
    }
};

// ============================================================================
// Pattern Factory Functions
// ============================================================================

/// Create a wildcard pattern (`_`).
/// @param id Unique identifier for this node
/// @param span Source location
/// @return Heap-allocated pattern
auto make_hir_wildcard_pattern(HirId id, SourceSpan span) -> HirPatternPtr;

/// Create a binding pattern (`x` or `mut x`).
/// @param id Unique identifier for this node
/// @param name Variable name to bind
/// @param is_mut Whether the binding is mutable
/// @param type Type of the bound variable
/// @param span Source location
/// @return Heap-allocated pattern
auto make_hir_binding_pattern(HirId id, const std::string& name, bool is_mut, HirType type,
                              SourceSpan span) -> HirPatternPtr;

/// Create an integer literal pattern.
/// @param id Unique identifier for this node
/// @param value Integer value to match
/// @param type Type of the literal (I32, I64, etc.)
/// @param span Source location
/// @return Heap-allocated pattern
auto make_hir_literal_pattern(HirId id, int64_t value, HirType type, SourceSpan span)
    -> HirPatternPtr;

/// Create a boolean literal pattern.
/// @param id Unique identifier for this node
/// @param value Boolean value to match (true or false)
/// @param type Bool type
/// @param span Source location
/// @return Heap-allocated pattern
auto make_hir_literal_pattern(HirId id, bool value, HirType type, SourceSpan span) -> HirPatternPtr;

/// Create a string literal pattern.
/// @param id Unique identifier for this node
/// @param value String value to match
/// @param type Str type
/// @param span Source location
/// @return Heap-allocated pattern
auto make_hir_literal_pattern(HirId id, const std::string& value, HirType type, SourceSpan span)
    -> HirPatternPtr;

/// Create a tuple pattern (`(a, b, c)`).
/// @param id Unique identifier for this node
/// @param elements Sub-patterns for each tuple element
/// @param type Tuple type being matched
/// @param span Source location
/// @return Heap-allocated pattern
auto make_hir_tuple_pattern(HirId id, std::vector<HirPatternPtr> elements, HirType type,
                            SourceSpan span) -> HirPatternPtr;

/// Create a struct pattern (`Point { x, y }`).
/// @param id Unique identifier for this node
/// @param struct_name Name of the struct type
/// @param fields List of (field_name, sub_pattern) pairs
/// @param has_rest Whether `..` is present
/// @param type Struct type being matched
/// @param span Source location
/// @return Heap-allocated pattern
auto make_hir_struct_pattern(HirId id, const std::string& struct_name,
                             std::vector<std::pair<std::string, HirPatternPtr>> fields,
                             bool has_rest, HirType type, SourceSpan span) -> HirPatternPtr;

/// Create an enum variant pattern (`Just(x)`, `Nothing`).
/// @param id Unique identifier for this node
/// @param enum_name Name of the enum type
/// @param variant_name Name of the variant
/// @param variant_index Numeric index of the variant
/// @param payload Optional sub-patterns for variant payload
/// @param type Enum type being matched
/// @param span Source location
/// @return Heap-allocated pattern
auto make_hir_enum_pattern(HirId id, const std::string& enum_name, const std::string& variant_name,
                           int variant_index, std::optional<std::vector<HirPatternPtr>> payload,
                           HirType type, SourceSpan span) -> HirPatternPtr;

} // namespace tml::hir
