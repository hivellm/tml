//! # Pattern Exhaustiveness Checker
//!
//! Implements the usefulness-based exhaustiveness checking algorithm
//! (Maranget 2007) for `when` expressions in THIR.
//!
//! ## Algorithm Overview
//!
//! 1. Build a pattern matrix from `when` arms
//! 2. For each type constructor not covered, report it as missing
//! 3. Recursively check sub-patterns for nested types
//! 4. Handle wildcards, bindings, literals, enum variants, ranges
//!
//! ## Usage
//!
//! ```cpp
//! ExhaustivenessChecker checker(type_env);
//! auto missing = checker.check_when(when_expr, scrutinee_type);
//! if (!missing.empty()) {
//!     // Emit non-exhaustive pattern warning/error
//! }
//! ```

#pragma once

#include "thir/thir_expr.hpp"

#include <string>
#include <vector>

namespace tml::types {
class TypeEnv;
} // namespace tml::types

namespace tml::thir {

// ============================================================================
// Pattern Deep-Clone Utility
// ============================================================================

/// Deep-clone a ThirPattern (patterns contain unique_ptr and are non-copyable).
[[nodiscard]] ThirPatternPtr clone_pattern(const ThirPattern& pattern);

/// Deep-clone a ThirPattern from a pointer.
[[nodiscard]] inline ThirPatternPtr clone_pattern(const ThirPatternPtr& p) {
    if (!p)
        return nullptr;
    return clone_pattern(*p);
}

// ============================================================================
// Constructor Representation
// ============================================================================

/// Represents a type constructor for exhaustiveness checking.
///
/// Constructors are the "building blocks" of patterns:
/// - Enum variants are constructors of their enum type
/// - `true` and `false` are constructors of Bool
/// - Integer literals are constructors of integer types
/// - Tuple/struct constructors represent their respective types
struct Constructor {
    enum class Kind {
        EnumVariant, ///< Enum variant (e.g., Just, Nothing)
        BoolTrue,    ///< Boolean true
        BoolFalse,   ///< Boolean false
        IntLiteral,  ///< Integer literal (for exhaustive integer ranges)
        CharLiteral, ///< Character literal
        StrLiteral,  ///< String literal (never exhaustive alone)
        Wildcard,    ///< Matches everything (for wildcard/binding patterns)
        Tuple,       ///< Tuple constructor
        Struct,      ///< Struct constructor (always single constructor)
        Range,       ///< Range of values
        Missing,     ///< Represents "everything not yet covered"
    };

    Kind kind;
    std::string name;       ///< Constructor name (variant name, literal value, etc.)
    int arity = 0;          ///< Number of sub-patterns this constructor takes
    int variant_index = -1; ///< For enum variants: the variant index
};

// ============================================================================
// Pattern Row / Matrix
// ============================================================================

/// A single row in the pattern matrix.
///
/// Each row corresponds to one when arm. The row contains one pattern per
/// column (initially just one column for the scrutinee, but specialization
/// can expand nested patterns into multiple columns).
struct PatternRow {
    std::vector<ThirPatternPtr> columns;
};

/// A pattern matrix for exhaustiveness checking.
///
/// The matrix has one row per when arm and one column per scrutinee position.
/// Specialization operations transform the matrix by "splitting" on a
/// specific constructor.
struct PatternMatrix {
    std::vector<PatternRow> rows;

    /// Number of columns in the matrix.
    [[nodiscard]] auto width() const -> size_t {
        return rows.empty() ? 0 : rows[0].columns.size();
    }

    /// Number of rows in the matrix.
    [[nodiscard]] auto height() const -> size_t {
        return rows.size();
    }

    /// Check if the matrix is empty (no rows).
    [[nodiscard]] auto empty() const -> bool {
        return rows.empty();
    }
};

// ============================================================================
// Exhaustiveness Checker
// ============================================================================

/// Checks pattern exhaustiveness for `when` expressions.
///
/// Uses the usefulness algorithm from "Warnings for pattern matching"
/// (Maranget 2007). A set of patterns is exhaustive if no possible value
/// of the scrutinee type can fail to match at least one pattern.
class ExhaustivenessChecker {
public:
    explicit ExhaustivenessChecker(const types::TypeEnv& env);

    /// Check a when expression for exhaustiveness.
    ///
    /// @param when The when expression to check
    /// @param scrutinee_type Type of the scrutinee being matched
    /// @return List of human-readable descriptions of missing patterns.
    ///         Empty if the patterns are exhaustive.
    auto check_when(const ThirWhenExpr& when, ThirType scrutinee_type) -> std::vector<std::string>;

    /// Check if a pattern row is "useful" with respect to a pattern matrix.
    ///
    /// A row is useful if there exists some value that matches the row but
    /// does not match any row in the matrix. This is used both for:
    /// - Exhaustiveness: check if the "missing" row `[_, _, ...]` is useful
    /// - Unreachability: check if each arm is useful w.r.t. previous arms
    ///
    /// @param matrix The existing pattern matrix (previous arms)
    /// @param row The new row to check
    /// @param types Column types
    /// @return true if the row is useful (matches something the matrix doesn't)
    auto is_useful(const PatternMatrix& matrix, const PatternRow& row,
                   const std::vector<ThirType>& types) -> bool;

private:
    const types::TypeEnv* env_;

    /// Get all constructors for a type.
    ///
    /// For enum types, returns one constructor per variant.
    /// For Bool, returns {true, false}.
    /// For other types, returns empty (meaning infinite constructors).
    auto type_constructors(ThirType type) -> std::vector<Constructor>;

    /// Specialize a pattern matrix for a specific constructor.
    ///
    /// "Specialization" filters the matrix to only rows whose first column
    /// matches the given constructor, then expands sub-patterns into new columns.
    auto specialize_matrix(const PatternMatrix& matrix, const Constructor& ctor,
                           const std::vector<ThirType>& types) -> PatternMatrix;

    /// Specialize a single row for a constructor.
    auto specialize_row(const PatternRow& row, const Constructor& ctor,
                        const std::vector<ThirType>& types) -> std::optional<PatternRow>;

    /// Compute the "default" matrix (rows whose first column is a wildcard).
    auto default_matrix(const PatternMatrix& matrix, const std::vector<ThirType>& types)
        -> PatternMatrix;

    /// Extract the constructor from a pattern.
    auto pattern_constructor(const ThirPattern& pattern) -> Constructor;

    /// Get sub-pattern types for a constructor applied to a type.
    auto constructor_sub_types(const Constructor& ctor, ThirType type) -> std::vector<ThirType>;

    /// Get sub-patterns from a pattern for a given constructor.
    auto pattern_sub_patterns(const ThirPattern& pattern, const Constructor& ctor)
        -> std::vector<ThirPatternPtr>;

    /// Make a wildcard pattern for filling in sub-patterns.
    auto make_wildcard() -> ThirPatternPtr;

    /// Format a constructor as a human-readable pattern string.
    auto format_constructor(const Constructor& ctor, ThirType type) -> std::string;
};

} // namespace tml::thir
