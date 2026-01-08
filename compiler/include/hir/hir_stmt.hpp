//! # HIR Statements
//!
//! This module defines statement types for the HIR. Statements are side-effecting
//! constructs that do not produce values directly.
//!
//! ## Overview
//!
//! HIR has a minimal set of statement types, keeping the representation simple:
//!
//! | Statement | TML Syntax | Description |
//! |-----------|------------|-------------|
//! | `HirLetStmt` | `let x = 5` | Variable binding with pattern |
//! | `HirExprStmt` | `foo();` | Expression evaluated for side effects |
//!
//! ## Desugaring
//!
//! The HIR statement set is reduced from AST through desugaring:
//! - `var x = 5` becomes `let mut x = 5` (handled in `HirLetStmt`)
//! - Assignment is an expression (`HirAssignExpr`), not a statement
//!
//! ## Statement vs Expression
//!
//! In TML (and HIR), the distinction is:
//! - **Expressions** produce values and can appear anywhere a value is needed
//! - **Statements** are sequenced in blocks and don't produce values
//!
//! An expression can become a statement by adding a semicolon:
//! ```tml
//! foo(x);     // Expression statement - value discarded
//! let y = foo(x);  // Let statement - value bound to y
//! ```
//!
//! ## Patterns in Let Statements
//!
//! Let statements use patterns for binding, enabling destructuring:
//! ```tml
//! let (x, y) = get_pair()     // Tuple destructuring
//! let Point { x, y } = point  // Struct destructuring
//! let [a, b, ..rest] = array  // Array destructuring
//! ```
//!
//! ## See Also
//!
//! - `docs/specs/31-HIR.md` - Complete HIR documentation
//! - `hir_expr.hpp` - Expression types (including assignment)
//! - `hir_pattern.hpp` - Patterns used in let statements

#pragma once

#include "hir/hir_id.hpp"
#include "hir/hir_pattern.hpp"

namespace tml::hir {

// Forward declaration
struct HirExpr;
using HirExprPtr = Box<HirExpr>;

// ============================================================================
// Statement Definitions
// ============================================================================

/// Let statement: `let x = expr` or `let x: T = expr`
///
/// Binds a value to a pattern, introducing new variables into scope.
/// This is the primary way to create local variables in TML.
///
/// ## Fields
/// - `id`: Unique identifier for this statement node
/// - `pattern`: Pattern to bind (may introduce multiple variables)
/// - `type`: Type of the bound value (explicit or inferred)
/// - `init`: Optional initializer expression (None for uninitialized)
/// - `span`: Source location
///
/// ## Mutability
///
/// Mutability is encoded in the pattern, not the let statement itself:
/// ```tml
/// let x = 5       // Immutable binding
/// let mut y = 10  // Mutable binding (HirBindingPattern with is_mut=true)
/// ```
///
/// ## Desugaring from `var`
///
/// The `var` keyword is syntactic sugar for `let mut`:
/// ```tml
/// var count = 0   // Desugared to: let mut count = 0
/// ```
///
/// ## Uninitialized Variables
///
/// Variables can be declared without initialization:
/// ```tml
/// let x: I32      // Declared but not initialized
/// x = compute()   // Must be assigned before use
/// ```
/// In this case, `init` is None and the compiler tracks initialization.
struct HirLetStmt {
    HirId id;
    HirPatternPtr pattern;
    HirType type;
    std::optional<HirExprPtr> init;
    SourceSpan span;
};

/// Expression statement: `expr;`
///
/// Evaluates an expression for its side effects, discarding the result.
///
/// ## Fields
/// - `id`: Unique identifier for this statement node
/// - `expr`: The expression to evaluate
/// - `span`: Source location
///
/// ## Common Uses
///
/// - Function calls: `print("hello");`
/// - Method calls: `list.push(item);`
/// - Assignments: `x = 10;` (assignment is an expression in HIR)
/// - Compound assignments: `count += 1;`
///
/// ## Value Discarding
///
/// The expression's value is discarded. For expressions with useful values,
/// the compiler may warn if the value is unused (for types marked with
/// `@must_use`).
struct HirExprStmt {
    HirId id;
    HirExprPtr expr;
    SourceSpan span;
};

// ============================================================================
// HirStmt Container
// ============================================================================

/// A statement in HIR.
///
/// `HirStmt` is a variant container that can hold either `HirLetStmt` or
/// `HirExprStmt`. It provides common accessors for ID and span.
///
/// ## Type Checking
///
/// Use `is<T>()` to check the statement kind before accessing with `as<T>()`:
/// ```cpp
/// if (stmt.is<HirLetStmt>()) {
///     const auto& let_stmt = stmt.as<HirLetStmt>();
///     // Process the binding pattern
/// } else if (stmt.is<HirExprStmt>()) {
///     const auto& expr_stmt = stmt.as<HirExprStmt>();
///     // Process the expression
/// }
/// ```
///
/// ## Note on Statement Count
///
/// HIR has only 2 statement types because most control flow is represented
/// as expressions (if, when, loops, return, break, continue).
struct HirStmt {
    std::variant<HirLetStmt, HirExprStmt> kind;

    /// Get the HIR ID for this statement.
    [[nodiscard]] auto id() const -> HirId;

    /// Get the source span.
    [[nodiscard]] auto span() const -> SourceSpan;

    /// Check if this statement is of kind `T`.
    /// @tparam T Either `HirLetStmt` or `HirExprStmt`
    /// @return true if this statement holds a T
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Get this statement as kind `T` (mutable).
    /// @tparam T Either `HirLetStmt` or `HirExprStmt`
    /// @return Reference to the contained statement
    /// @throws std::bad_variant_access if statement is not of type T
    template <typename T> [[nodiscard]] auto as() -> T& {
        return std::get<T>(kind);
    }

    /// Get this statement as kind `T` (const).
    /// @tparam T Either `HirLetStmt` or `HirExprStmt`
    /// @return Const reference to the contained statement
    /// @throws std::bad_variant_access if statement is not of type T
    template <typename T> [[nodiscard]] auto as() const -> const T& {
        return std::get<T>(kind);
    }
};

// ============================================================================
// Statement Factory Functions
// ============================================================================

/// Create a let statement.
///
/// @param id Unique identifier for this node
/// @param pattern Pattern to bind (determines variable names and mutability)
/// @param type Type of the bound value
/// @param init Optional initializer expression
/// @param span Source location
/// @return Heap-allocated statement
///
/// ## Example
/// ```cpp
/// auto pattern = make_hir_binding_pattern(id, "x", false, i32_type, span);
/// auto init = make_hir_literal(id, 42, i32_type, span);
/// auto stmt = make_hir_let(id, std::move(pattern), i32_type, std::move(init), span);
/// ```
auto make_hir_let(HirId id, HirPatternPtr pattern, HirType type, std::optional<HirExprPtr> init,
                  SourceSpan span) -> HirStmtPtr;

/// Create an expression statement.
///
/// @param id Unique identifier for this node
/// @param expr The expression to evaluate
/// @param span Source location
/// @return Heap-allocated statement
///
/// ## Example
/// ```cpp
/// auto call = make_hir_call(id, "print", {}, {arg}, void_type, span);
/// auto stmt = make_hir_expr_stmt(id, std::move(call), span);
/// ```
auto make_hir_expr_stmt(HirId id, HirExprPtr expr, SourceSpan span) -> HirStmtPtr;

} // namespace tml::hir
