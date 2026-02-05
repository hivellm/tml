//! # Statement AST Nodes
//!
//! This module defines the AST nodes for statements (side-effecting constructs).
//! Statements appear inside blocks and function bodies. They execute sequentially
//! and don't produce values (unlike expressions).
//!
//! ## Statement Types
//!
//! - **Let**: `let x = expr` - immutable binding
//! - **Var**: `var x = expr` - mutable binding (equivalent to `let mut`)
//! - **Expression**: `expr;` - evaluate expression for side effects
//! - **Declaration**: nested functions, types, etc.
//!
//! ## Let vs Var
//!
//! TML provides two binding syntaxes:
//! - `let x = 42` - immutable binding (cannot be reassigned)
//! - `var x = 42` - mutable binding (can be reassigned)
//!
//! The `var` keyword is syntactic sugar for `let mut`, providing a more
//! familiar syntax for developers coming from other languages.

#ifndef TML_PARSER_AST_STMTS_HPP
#define TML_PARSER_AST_STMTS_HPP

#include "ast_common.hpp"
#include "ast_exprs.hpp"
#include "ast_patterns.hpp"
#include "ast_types.hpp"

namespace tml::parser {

// ============================================================================
// Variable Bindings
// ============================================================================

/// Let statement: `let x = expr` or `let x: T = expr`.
///
/// Creates an immutable binding. The pattern can be simple (identifier)
/// or complex (destructuring).
///
/// # Examples
///
/// ```tml
/// let x = 42
/// let (a, b) = get_pair()
/// let Point { x, y } = point
/// let value: I32 = compute()
/// ```
struct LetStmt {
    PatternPtr pattern;                     ///< Binding pattern.
    std::optional<TypePtr> type_annotation; ///< Optional type annotation.
    std::optional<ExprPtr> init;            ///< Initializer expression.
    SourceSpan span;                        ///< Source location.
    bool is_volatile = false;               ///< Volatile modifier (prevents optimization).
};

/// Var statement: `var x = expr` (mutable binding).
///
/// Creates a mutable binding. Equivalent to `let mut x = expr`.
/// The value can be reassigned later.
///
/// # Examples
///
/// ```tml
/// var count = 0
/// count = count + 1     // OK: can reassign
///
/// var total: F64 = 0.0
/// total = total + value
/// ```
struct VarStmt {
    std::string name;                       ///< Variable name.
    std::optional<TypePtr> type_annotation; ///< Optional type annotation.
    ExprPtr init;                           ///< Initializer expression (required).
    SourceSpan span;                        ///< Source location.
    bool is_volatile = false;               ///< Volatile modifier (prevents optimization).
};

/// Let-else statement: `let Pattern: T = expr else { diverging_block }`.
///
/// Attempts to match a refutable pattern and either binds the matched values
/// or executes a diverging else block.
///
/// # Examples
///
/// ```tml
/// let Ok(value): Outcome[I32, Str] = result else {
///     return Err("failed")
/// }
///
/// let Just(x): Maybe[I32] = maybe else {
///     panic("expected value")
/// }
/// ```
///
/// # Requirements
///
/// - The else block MUST diverge (return, panic, break, continue)
/// - The pattern must be refutable (can fail to match)
struct LetElseStmt {
    PatternPtr pattern;                     ///< Pattern to match (refutable).
    std::optional<TypePtr> type_annotation; ///< Type annotation (required in TML).
    ExprPtr init;                           ///< Expression to match against.
    ExprPtr else_block;                     ///< Diverging block if pattern doesn't match.
    SourceSpan span;                        ///< Source location.
};

// ============================================================================
// Expression Statement
// ============================================================================

/// Expression statement: `expr;`.
///
/// Evaluates an expression for its side effects, discarding the result.
/// Common for function calls, assignments, and method calls.
///
/// # Examples
///
/// ```tml
/// print("hello");
/// vec.push(item);
/// x = x + 1;
/// ```
struct ExprStmt {
    ExprPtr expr;    ///< The expression.
    SourceSpan span; ///< Source location.
};

// ============================================================================
// Statement Variant
// ============================================================================

/// A statement (side-effecting construct).
///
/// Statements include variable bindings, expression statements,
/// and nested declarations. They appear inside blocks and function bodies.
///
/// Uses `std::variant` for type-safe sum types with helper methods
/// for type checking and casting.
struct Stmt {
    std::variant<LetStmt, VarStmt, LetElseStmt, ExprStmt,
                 DeclPtr ///< Nested declaration (func, type, etc.).
                 >
        kind;        ///< The statement variant.
    SourceSpan span; ///< Source location.

    /// Checks if this statement is of kind `T`.
    ///
    /// # Example
    /// ```cpp
    /// if (stmt.is<LetStmt>()) { ... }
    /// ```
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Gets this statement as kind `T`. Throws `std::bad_variant_access` if wrong kind.
    ///
    /// # Example
    /// ```cpp
    /// auto& let_stmt = stmt.as<LetStmt>();
    /// ```
    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    /// Gets this statement as kind `T` (const). Throws `std::bad_variant_access` if wrong kind.
    template <typename T> [[nodiscard]] auto as() const -> const T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }
};

} // namespace tml::parser

#endif // TML_PARSER_AST_STMTS_HPP
