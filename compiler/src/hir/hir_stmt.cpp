//! # HIR Statement Implementation
//!
//! This file implements the HirStmt accessor methods and factory functions.

#include "hir/hir_stmt.hpp"

#include "hir/hir_expr.hpp"

namespace tml::hir {

// ============================================================================
// HirStmt Accessors
// ============================================================================

auto HirStmt::id() const -> HirId {
    return std::visit([](const auto& s) { return s.id; }, kind);
}

auto HirStmt::span() const -> SourceSpan {
    return std::visit([](const auto& s) { return s.span; }, kind);
}

// ============================================================================
// Statement Factory Functions
// ============================================================================

auto make_hir_let(HirId id, HirPatternPtr pattern, HirType type, std::optional<HirExprPtr> init,
                  SourceSpan span) -> HirStmtPtr {
    auto stmt = std::make_unique<HirStmt>();
    stmt->kind = HirLetStmt{id, std::move(pattern), std::move(type), std::move(init), span};
    return stmt;
}

auto make_hir_expr_stmt(HirId id, HirExprPtr expr, SourceSpan span) -> HirStmtPtr {
    auto stmt = std::make_unique<HirStmt>();
    stmt->kind = HirExprStmt{id, std::move(expr), span};
    return stmt;
}

} // namespace tml::hir
