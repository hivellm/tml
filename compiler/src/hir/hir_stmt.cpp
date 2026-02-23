TML_MODULE("compiler")

//! # HIR Statement Implementation
//!
//! This file implements the HirStmt accessor methods and factory functions.
//!
//! ## Overview
//!
//! HIR statements represent side-effectful operations that don't produce values.
//! TML is expression-oriented, so the statement set is minimal:
//!
//! | Statement    | Purpose                                    |
//! |--------------|--------------------------------------------|
//! | `HirLetStmt` | Variable binding with pattern destructure  |
//! | `HirExprStmt`| Expression evaluated for side effects      |
//!
//! ## Design Notes
//!
//! - `var x = ...` is desugared to `let mut x = ...` during AST lowering
//! - Assignment expressions (=, +=, etc.) are expressions, not statements
//! - Loop/control flow constructs are expressions that return values
//!
//! ## Factory Functions
//!
//! - `make_hir_let` - Creates let statements with optional initializer
//! - `make_hir_expr_stmt` - Wraps expressions as statements
//!
//! ## See Also
//!
//! - `hir_stmt.hpp` - Statement type definitions
//! - `hir_builder_stmt.cpp` - Statement lowering from AST

#include "hir/hir_stmt.hpp"

#include "hir/hir_expr.hpp"

namespace tml::hir {

// ============================================================================
// HirStmt Accessors
// ============================================================================
//
// Uniform accessors for statement properties. Uses std::visit to extract
// common fields from whichever statement variant is active.

auto HirStmt::id() const -> HirId {
    return std::visit([](const auto& s) { return s.id; }, kind);
}

auto HirStmt::span() const -> SourceSpan {
    return std::visit([](const auto& s) { return s.span; }, kind);
}

// ============================================================================
// Statement Factory Functions
// ============================================================================
//
// Factory functions for creating HIR statement nodes.
// - make_hir_let: Binds a pattern to an optional initializer
// - make_hir_expr_stmt: Wraps an expression for side-effect evaluation

auto make_hir_let(HirId id, HirPatternPtr pattern, HirType type, std::optional<HirExprPtr> init,
                  SourceSpan span, bool is_volatile) -> HirStmtPtr {
    auto stmt = std::make_unique<HirStmt>();
    stmt->kind =
        HirLetStmt{id, std::move(pattern), std::move(type), std::move(init), span, is_volatile};
    return stmt;
}

auto make_hir_expr_stmt(HirId id, HirExprPtr expr, SourceSpan span) -> HirStmtPtr {
    auto stmt = std::make_unique<HirStmt>();
    stmt->kind = HirExprStmt{id, std::move(expr), span};
    return stmt;
}

} // namespace tml::hir
