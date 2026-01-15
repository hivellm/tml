//! # HIR Builder - Statement Lowering
//!
//! This file implements statement lowering from AST to HIR.
//!
//! ## Overview
//!
//! Statement lowering transforms parser AST statements into HIR statements.
//! TML has a small set of statement types since most constructs are expressions.
//!
//! ## Statement Types
//!
//! | AST Statement | HIR Statement | Description                           |
//! |---------------|---------------|---------------------------------------|
//! | `LetStmt`     | `HirLetStmt`  | Immutable binding: `let x = 1`        |
//! | `VarStmt`     | `HirLetStmt`  | Mutable binding: `var x = 1` (sugar)  |
//! | `ExprStmt`    | `HirExprStmt` | Expression evaluated for side-effects |
//! | `DeclPtr`     | (placeholder) | Nested declarations in blocks         |
//!
//! ## Key Transformations
//!
//! - `var x = value` desugars to `let mut x = value`
//! - Pattern bindings add variables to the current scope
//! - Type inference from initializers when no annotation provided
//!
//! ## Scope Management
//!
//! Each `let`/`var` binding adds names to the current scope for variable
//! resolution. Block expressions create new scopes (handled in hir_builder_expr.cpp).
//!
//! ## See Also
//!
//! - `hir_builder.cpp` - Main builder and scope infrastructure
//! - `hir_builder_expr.cpp` - Expression lowering
//! - `hir_builder_pattern.cpp` - Pattern lowering for let bindings

#include "hir/hir_builder.hpp"

namespace tml::hir {

// ============================================================================
// Statement Lowering Dispatch
// ============================================================================
//
// Main entry point for statement lowering. Uses std::visit to dispatch
// to type-specific lowering functions based on the statement variant.
// Nested declarations (const, type aliases) become placeholder statements
// since they're already registered in the module's symbol tables.

auto HirBuilder::lower_stmt(const parser::Stmt& stmt) -> HirStmtPtr {
    return std::visit(
        [&](const auto& s) -> HirStmtPtr {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, parser::LetStmt>) {
                return lower_let(s);
            } else if constexpr (std::is_same_v<T, parser::VarStmt>) {
                return lower_var(s);
            } else if constexpr (std::is_same_v<T, parser::ExprStmt>) {
                return lower_expr_stmt(s);
            } else if constexpr (std::is_same_v<T, parser::DeclPtr>) {
                // Nested declarations become expression statements
                // (mainly for const/type declarations inside blocks)
                auto placeholder =
                    make_hir_literal(fresh_id(), int64_t(0), types::make_unit(), stmt.span);
                return make_hir_expr_stmt(fresh_id(), std::move(placeholder), stmt.span);
            } else {
                // Fallback
                auto placeholder =
                    make_hir_literal(fresh_id(), int64_t(0), types::make_unit(), stmt.span);
                return make_hir_expr_stmt(fresh_id(), std::move(placeholder), stmt.span);
            }
        },
        stmt.kind);
}

// ============================================================================
// Let Statement
// ============================================================================
//
// `let` creates an immutable binding. The type is determined by:
// 1. Explicit type annotation: `let x: I32 = ...`
// 2. Initializer expression type: `let x = 42` infers I32
// 3. Default to unit if neither present (rare edge case)
//
// Pattern bindings extract names and register them in the current scope
// for later variable resolution in expressions.

auto HirBuilder::lower_let(const parser::LetStmt& let_stmt) -> HirStmtPtr {
    // Resolve type
    HirType type;
    if (let_stmt.type_annotation) {
        type = resolve_type(**let_stmt.type_annotation);
    } else if (let_stmt.init) {
        type = get_expr_type(**let_stmt.init);
    } else {
        type = types::make_unit();
    }

    // Lower pattern
    auto pattern = lower_pattern(*let_stmt.pattern, type);

    // Add bindings to current scope
    if (let_stmt.pattern->is<parser::IdentPattern>()) {
        const auto& ident = let_stmt.pattern->as<parser::IdentPattern>();
        if (!scopes_.empty()) {
            scopes_.back().insert(ident.name);
        }
        // Also define in type environment scope so we can look up the type later
        if (auto scope = type_env_.current_scope()) {
            scope->define(ident.name, type, ident.is_mut, let_stmt.span);
        }
    }

    // Lower initializer
    std::optional<HirExprPtr> init;
    if (let_stmt.init) {
        init = lower_expr(**let_stmt.init);
    }

    return make_hir_let(fresh_id(), std::move(pattern), type, std::move(init), let_stmt.span);
}

// ============================================================================
// Var Statement (desugars to let mut)
// ============================================================================
//
// TML's `var x = value` is syntactic sugar for `let mut x = value`.
// This provides a familiar, more concise syntax for mutable variables.
//
// Unlike `let`, `var` always requires an initializer and uses simple
// identifier binding (not full pattern syntax).

auto HirBuilder::lower_var(const parser::VarStmt& var_stmt) -> HirStmtPtr {
    // Resolve type
    HirType type;
    if (var_stmt.type_annotation) {
        type = resolve_type(**var_stmt.type_annotation);
    } else {
        type = get_expr_type(*var_stmt.init);
    }

    // Create mutable binding pattern
    auto pattern = make_hir_binding_pattern(fresh_id(), var_stmt.name, true, type, var_stmt.span);

    // Add to current scope
    if (!scopes_.empty()) {
        scopes_.back().insert(var_stmt.name);
    }
    // Also define in type environment scope so we can look up the type later
    if (auto scope = type_env_.current_scope()) {
        scope->define(var_stmt.name, type, true, var_stmt.span);
    }

    // Lower initializer
    auto init = lower_expr(*var_stmt.init);

    return make_hir_let(fresh_id(), std::move(pattern), type, std::move(init), var_stmt.span);
}

// ============================================================================
// Expression Statement
// ============================================================================
//
// Expression statements evaluate an expression for its side effects,
// discarding the result. Common examples:
// - Function calls: `print("hello")`
// - Assignments: `x = 42`
// - Method calls: `list.push(item)`

auto HirBuilder::lower_expr_stmt(const parser::ExprStmt& expr_stmt) -> HirStmtPtr {
    auto expr = lower_expr(*expr_stmt.expr);
    return make_hir_expr_stmt(fresh_id(), std::move(expr), expr_stmt.span);
}

} // namespace tml::hir
