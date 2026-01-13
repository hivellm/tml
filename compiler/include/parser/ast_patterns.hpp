//! # Pattern AST Nodes
//!
//! This module defines the AST nodes for pattern matching constructs.
//! Patterns are used for destructuring values in let bindings, function
//! parameters, when expressions, and for loops.
//!
//! ## Pattern Categories
//!
//! - **Wildcard**: `_` - matches anything, discards value
//! - **Identifier**: `x`, `mut x` - binds value to a name
//! - **Literal**: `42`, `"hello"`, `true` - matches exact values
//! - **Tuple**: `(a, b, c)` - destructures tuples
//! - **Struct**: `Point { x, y }` - destructures named structs
//! - **Enum**: `Just(x)`, `Nothing` - matches enum variants
//! - **Or**: `a | b | c` - matches any of several patterns
//! - **Range**: `0 to 10`, `'a' through 'z'` - matches value ranges
//! - **Array**: `[a, b, c]`, `[head, ..rest]` - destructures arrays/slices
//!
//! ## Usage Contexts
//!
//! ```tml
//! let (x, y) = point                    // Let binding
//! when value { Just(x) => x, ... }      // When expression
//! for (k, v) in map { ... }             // For loop
//! func add((x, y): Point) -> I32        // Function parameter
//! ```

#ifndef TML_PARSER_AST_PATTERNS_HPP
#define TML_PARSER_AST_PATTERNS_HPP

#include "ast_common.hpp"
#include "ast_types.hpp"

namespace tml::parser {

// ============================================================================
// Simple Patterns
// ============================================================================

/// Wildcard pattern: `_`.
///
/// Matches any value and discards it. Useful when you need to match
/// a position but don't care about the value.
///
/// # Example
///
/// ```tml
/// when result {
///     Ok(_) => print("success"),     // Ignore the value
///     Err(e) => print("error: {e}"),
/// }
/// ```
struct WildcardPattern {
    SourceSpan span; ///< Source location.
};

/// Identifier pattern: `x` or `mut x`.
///
/// Binds a value to a name, optionally with a type annotation.
/// Use `mut` to create a mutable binding.
///
/// # Examples
///
/// ```tml
/// let x = 42                  // Immutable binding
/// let mut count = 0           // Mutable binding
/// let value: I32 = 100        // With type annotation
/// ```
struct IdentPattern {
    std::string name;                       ///< The bound name.
    bool is_mut;                            ///< True for mutable binding.
    std::optional<TypePtr> type_annotation; ///< Optional type annotation.
    SourceSpan span;                        ///< Source location.
};

/// Literal pattern: `42`, `"hello"`, `true`.
///
/// Matches a specific literal value. Used in when expressions
/// to match exact values.
///
/// # Example
///
/// ```tml
/// when status_code {
///     200 => "OK",
///     404 => "Not Found",
///     500 => "Server Error",
///     _ => "Unknown",
/// }
/// ```
struct LiteralPattern {
    lexer::Token literal; ///< The literal token (contains the value).
    SourceSpan span;      ///< Source location.
};

// ============================================================================
// Composite Patterns
// ============================================================================

/// Tuple pattern: `(a, b, c)`.
///
/// Destructures a tuple into its components. Each element can be
/// any pattern, allowing nested destructuring.
///
/// # Examples
///
/// ```tml
/// let (x, y) = get_point()
/// let (first, _, last) = (1, 2, 3)      // Ignore middle element
/// let ((a, b), c) = nested_tuple        // Nested destructuring
/// ```
struct TuplePattern {
    std::vector<PatternPtr> elements; ///< Element patterns.
    SourceSpan span;                  ///< Source location.
};

/// Struct pattern: `Point { x, y }` or `Point { x, .. }`.
///
/// Destructures a named struct by its fields. Can use `..` to ignore
/// remaining fields.
///
/// # Examples
///
/// ```tml
/// let Point { x, y } = point
/// let Person { name, .. } = person      // Ignore other fields
/// let Config { debug: is_debug } = cfg  // Rename binding
/// ```
struct StructPattern {
    TypePath path;                                          ///< The struct type.
    std::vector<std::pair<std::string, PatternPtr>> fields; ///< Field bindings (name -> pattern).
    bool has_rest;   ///< True if `..` present (ignore remaining fields).
    SourceSpan span; ///< Source location.
};

/// Enum variant pattern: `Just(x)`, `Nothing`.
///
/// Matches a specific enum variant, optionally destructuring its payload.
///
/// # Examples
///
/// ```tml
/// when maybe {
///     Just(value) => process(value),
///     Nothing => default_value,
/// }
///
/// when result {
///     Ok(data) => use(data),
///     Err(Error::NotFound) => handle_not_found(),
///     Err(e) => panic("unexpected: {e}"),
/// }
/// ```
struct EnumPattern {
    TypePath path; ///< The enum variant path (e.g., `Option::Just`).
    std::optional<std::vector<PatternPtr>> payload; ///< Variant payload patterns (if any).
    SourceSpan span;                                ///< Source location.
};

// ============================================================================
// Advanced Patterns
// ============================================================================

/// Or pattern: `a | b | c`.
///
/// Matches if any of the alternatives match. All alternatives must
/// bind the same names with the same types.
///
/// # Example
///
/// ```tml
/// when key {
///     'a' | 'e' | 'i' | 'o' | 'u' => "vowel",
///     _ => "consonant",
/// }
/// ```
struct OrPattern {
    std::vector<PatternPtr> patterns; ///< Alternative patterns.
    SourceSpan span;                  ///< Source location.
};

/// Range pattern: `0 to 10` or `'a' through 'z'`.
///
/// Matches values within a range. Uses `to` for exclusive end
/// and `through` for inclusive end.
///
/// # Examples
///
/// ```tml
/// when score {
///     0 to 60 => "F",
///     60 to 70 => "D",
///     70 to 80 => "C",
///     80 to 90 => "B",
///     90 through 100 => "A",    // Inclusive: includes 100
/// }
/// ```
struct RangePattern {
    std::optional<ExprPtr> start; ///< Start of range (optional for `..end`).
    std::optional<ExprPtr> end;   ///< End of range (optional for `start..`).
    bool inclusive;               ///< True for `through` (inclusive), false for `to` (exclusive).
    SourceSpan span;              ///< Source location.
};

/// Array/slice pattern: `[a, b, c]` or `[head, ..rest]`.
///
/// Destructures arrays or slices. Can use `..rest` to capture
/// remaining elements into a slice.
///
/// # Examples
///
/// ```tml
/// let [first, second, third] = array
/// let [head, ..tail] = slice            // head: T, tail: [T]
/// let [a, b, ..middle, y, z] = items    // Capture middle
/// ```
struct ArrayPattern {
    std::vector<PatternPtr> elements; ///< Element patterns.
    std::optional<PatternPtr> rest;   ///< Rest pattern for `[head, ..rest]`.
    SourceSpan span;                  ///< Source location.
};

// ============================================================================
// Pattern Variant
// ============================================================================

/// A pattern for destructuring and matching values.
///
/// Patterns are used in:
/// - `let` bindings: `let (x, y) = point`
/// - `when` arms: `when value { Just(x) => ... }`
/// - `for` loops: `for (k, v) in map`
/// - Function parameters: `func add((x, y): Point)`
///
/// Uses `std::variant` for type-safe sum types with helper methods
/// for type checking and casting.
struct Pattern {
    std::variant<WildcardPattern, IdentPattern, LiteralPattern, TuplePattern, StructPattern,
                 EnumPattern, OrPattern, RangePattern, ArrayPattern>
        kind;        ///< The pattern variant.
    SourceSpan span; ///< Source location.

    /// Checks if this pattern is of kind `T`.
    ///
    /// # Example
    /// ```cpp
    /// if (pattern.is<IdentPattern>()) { ... }
    /// ```
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Gets this pattern as kind `T`. Throws `std::bad_variant_access` if wrong kind.
    ///
    /// # Example
    /// ```cpp
    /// auto& ident = pattern.as<IdentPattern>();
    /// ```
    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    /// Gets this pattern as kind `T` (const). Throws `std::bad_variant_access` if wrong kind.
    template <typename T> [[nodiscard]] auto as() const -> const T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }
};

} // namespace tml::parser

#endif // TML_PARSER_AST_PATTERNS_HPP
