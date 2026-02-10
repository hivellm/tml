//! # THIR Statements
//!
//! Statement types for THIR. Mirrors HIR statements exactly — the same two
//! statement kinds (let and expression statement) are used.

#pragma once

#include "thir/thir_expr.hpp"

namespace tml::thir {

// ============================================================================
// Statement Definitions
// ============================================================================

/// Let statement: `let x = expr` or `let x: T = expr`
struct ThirLetStmt {
    ThirId id;
    ThirPatternPtr pattern;
    ThirType type;
    std::optional<ThirExprPtr> init;
    SourceSpan span;
    bool is_volatile = false;
};

/// Expression statement: `expr;`
struct ThirExprStmt {
    ThirId id;
    ThirExprPtr expr;
    SourceSpan span;
};

// ============================================================================
// ThirStmt Container
// ============================================================================

/// A THIR statement.
struct ThirStmt {
    std::variant<ThirLetStmt, ThirExprStmt> kind;

    /// Get the THIR ID for this statement.
    [[nodiscard]] auto id() const -> ThirId;

    /// Get the source span.
    [[nodiscard]] auto span() const -> SourceSpan;

    /// Check if this statement is of kind `T`.
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Get this statement as kind `T` (mutable).
    template <typename T> [[nodiscard]] auto as() -> T& {
        return std::get<T>(kind);
    }

    /// Get this statement as kind `T` (const).
    template <typename T> [[nodiscard]] auto as() const -> const T& {
        return std::get<T>(kind);
    }
};

} // namespace tml::thir
