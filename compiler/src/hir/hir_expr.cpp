TML_MODULE("compiler")

//! # HIR Expression Implementation
//!
//! This file implements the HirExpr accessor methods and factory functions.
//!
//! ## Overview
//!
//! HIR expressions are the building blocks of computation in the High-level
//! Intermediate Representation. Each expression has:
//! - A unique `HirId` for identification
//! - A `HirType` representing its result type
//! - A `SourceSpan` for error reporting
//!
//! ## Expression Kinds
//!
//! | Category     | Types                                     | Description           |
//! |--------------|-------------------------------------------|-----------------------|
//! | Atoms        | Literal, Var, Enum                        | Basic values          |
//! | Operations   | Binary, Unary, Cast                       | Computations          |
//! | Access       | Call, MethodCall, Field, Index            | Data access           |
//! | Constructors | Tuple, Array, Struct                      | Composite values      |
//! | Control      | If, When, Loop, While, For, Block         | Control flow          |
//! | Jumps        | Return, Break, Continue                   | Non-local control     |
//! | Special      | Closure, Try, Await, Lowlevel             | Advanced features     |
//!
//! ## Factory Functions
//!
//! Factory functions (`make_hir_*`) provide a consistent API for creating
//! HIR expression nodes. They handle:
//! - Memory allocation via `std::make_unique`
//! - Proper initialization of the variant kind
//! - Type and span propagation
//!
//! ## See Also
//!
//! - `hir_expr.hpp` - Expression type definitions
//! - `hir_builder_expr.cpp` - Expression lowering from AST
//! - `hir_printer.cpp` - Expression pretty-printing

#include "hir/hir_expr.hpp"

#include "hir/hir_stmt.hpp" // Required for HirStmtPtr destructor in HirBlockExpr

namespace tml::hir {

// ============================================================================
// HirExpr Accessors
// ============================================================================
//
// These methods provide uniform access to expression properties regardless
// of the specific expression kind. The variant visitor pattern extracts
// the common fields (id, type, span) from whichever variant is active.
//
// Note: Return, Break, Continue, Assign, and CompoundAssign don't produce
// values, so type() returns nullptr for these "statement-like" expressions.

auto HirExpr::id() const -> HirId {
    return std::visit([](const auto& e) { return e.id; }, kind);
}

auto HirExpr::type() const -> HirType {
    return std::visit(
        [](const auto& e) -> HirType {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, HirReturnExpr> || std::is_same_v<T, HirBreakExpr> ||
                          std::is_same_v<T, HirContinueExpr> || std::is_same_v<T, HirAssignExpr> ||
                          std::is_same_v<T, HirCompoundAssignExpr>) {
                return nullptr; // These don't produce values
            } else {
                return e.type;
            }
        },
        kind);
}

auto HirExpr::span() const -> SourceSpan {
    return std::visit([](const auto& e) { return e.span; }, kind);
}

// ============================================================================
// Expression Factory Functions
// ============================================================================
//
// Factory functions create properly-initialized HIR expression nodes.
// Each function:
// 1. Allocates a new HirExpr via make_unique
// 2. Sets the appropriate variant in the `kind` field
// 3. Returns ownership via unique_ptr
//
// Overloads for make_hir_literal handle different value types:
// int64_t, uint64_t, double, bool, char, string

auto make_hir_literal(HirId id, int64_t value, HirType type, SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirLiteralExpr{id, value, std::move(type), span};
    return expr;
}

auto make_hir_literal(HirId id, uint64_t value, HirType type, SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirLiteralExpr{id, value, std::move(type), span};
    return expr;
}

auto make_hir_literal(HirId id, double value, HirType type, SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirLiteralExpr{id, value, std::move(type), span};
    return expr;
}

auto make_hir_literal(HirId id, bool value, HirType type, SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirLiteralExpr{id, value, std::move(type), span};
    return expr;
}

auto make_hir_literal(HirId id, char value, HirType type, SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirLiteralExpr{id, value, std::move(type), span};
    return expr;
}

auto make_hir_literal(HirId id, const std::string& value, HirType type, SourceSpan span)
    -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirLiteralExpr{id, value, std::move(type), span};
    return expr;
}

auto make_hir_var(HirId id, const std::string& name, HirType type, SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirVarExpr{id, name, std::move(type), span};
    return expr;
}

auto make_hir_binary(HirId id, HirBinOp op, HirExprPtr left, HirExprPtr right, HirType type,
                     SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirBinaryExpr{id, op, std::move(left), std::move(right), std::move(type), span};
    return expr;
}

auto make_hir_unary(HirId id, HirUnaryOp op, HirExprPtr operand, HirType type, SourceSpan span)
    -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirUnaryExpr{id, op, std::move(operand), std::move(type), span};
    return expr;
}

auto make_hir_call(HirId id, const std::string& func_name, std::vector<HirType> type_args,
                   std::vector<HirExprPtr> args, HirType type, SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind =
        HirCallExpr{id, func_name, std::move(type_args), std::move(args), std::move(type), span};
    return expr;
}

auto make_hir_method_call(HirId id, HirExprPtr receiver, const std::string& method_name,
                          std::vector<HirType> type_args, std::vector<HirExprPtr> args,
                          HirType receiver_type, HirType type, SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirMethodCallExpr{id,
                                   std::move(receiver),
                                   method_name,
                                   std::move(type_args),
                                   std::move(args),
                                   std::move(receiver_type),
                                   std::move(type),
                                   span};
    return expr;
}

auto make_hir_field(HirId id, HirExprPtr object, const std::string& field_name, int field_index,
                    HirType type, SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind =
        HirFieldExpr{id, std::move(object), field_name, field_index, std::move(type), span};
    return expr;
}

auto make_hir_index(HirId id, HirExprPtr object, HirExprPtr index, HirType type, SourceSpan span)
    -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirIndexExpr{id, std::move(object), std::move(index), std::move(type), span};
    return expr;
}

auto make_hir_block(HirId id, std::vector<HirStmtPtr> stmts, std::optional<HirExprPtr> expr,
                    HirType type, SourceSpan span) -> HirExprPtr {
    auto result = std::make_unique<HirExpr>();
    result->kind = HirBlockExpr{id, std::move(stmts), std::move(expr), std::move(type), span};
    return result;
}

auto make_hir_if(HirId id, HirExprPtr condition, HirExprPtr then_branch,
                 std::optional<HirExprPtr> else_branch, HirType type, SourceSpan span)
    -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirIfExpr{
        id,  std::move(condition), std::move(then_branch), std::move(else_branch), std::move(type),
        span};
    return expr;
}

auto make_hir_return(HirId id, std::optional<HirExprPtr> value, SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirReturnExpr{id, std::move(value), span};
    return expr;
}

auto make_hir_break(HirId id, std::optional<std::string> label, std::optional<HirExprPtr> value,
                    SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirBreakExpr{id, std::move(label), std::move(value), span};
    return expr;
}

auto make_hir_continue(HirId id, std::optional<std::string> label, SourceSpan span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    expr->kind = HirContinueExpr{id, std::move(label), span};
    return expr;
}

} // namespace tml::hir
