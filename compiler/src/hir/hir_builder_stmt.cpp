//! # HIR Builder - Statement Lowering
//!
//! This file implements statement lowering from AST to HIR.

#include "hir/hir_builder.hpp"

namespace tml::hir {

// ============================================================================
// Statement Lowering Dispatch
// ============================================================================

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

    // Lower initializer
    auto init = lower_expr(*var_stmt.init);

    return make_hir_let(fresh_id(), std::move(pattern), type, std::move(init), var_stmt.span);
}

// ============================================================================
// Expression Statement
// ============================================================================

auto HirBuilder::lower_expr_stmt(const parser::ExprStmt& expr_stmt) -> HirStmtPtr {
    auto expr = lower_expr(*expr_stmt.expr);
    return make_hir_expr_stmt(fresh_id(), std::move(expr), expr_stmt.span);
}

} // namespace tml::hir
