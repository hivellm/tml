TML_MODULE("compiler")

//! # HIR Constant Folding and Dead Code Elimination
//!
//! Implements constant folding and dead code elimination passes for HIR.
//!
//! ## Overview
//!
//! HIR passes perform source-level optimizations before lowering to MIR.
//! These passes operate on the type-annotated HIR representation.
//!
//! ## Constant Folding Details
//!
//! Evaluates compile-time constant expressions:
//! - Integer arithmetic: `2 + 3` -> `5`
//! - Float arithmetic: `1.0 / 2.0` -> `0.5`
//! - Boolean logic: `true and false` -> `false`
//! - Short-circuit: `false and expr` -> `false` (skips expr)
//! - Unary: `-5`, `not true`, `~0xFF`
//!
//! Division by zero is not folded to preserve runtime error.
//!
//! ## Dead Code Elimination Details
//!
//! Removes unreachable code:
//! - `if true { a } else { b }` -> `a`
//! - `if false { a } else { b }` -> `b`
//! - Code after `return`, `break`, `continue`
//!
//! ## See Also
//!
//! - `hir_pass_inline.cpp` - Inlining, closure optimization, pass manager
//! - `hir_pass.hpp` - Pass class declarations
//! - `mir/mir_pass.cpp` - MIR-level optimizations

#include "hir/hir_pass.hpp"

#include "hir/hir_expr.hpp"
#include "hir/hir_stmt.hpp"
#include "types/type.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace tml::hir {

// ============================================================================
// Helper: Create Literal Expression
// ============================================================================
//
// Factory functions for creating constant literals during folding.
// These create fresh HirLiteralExpr nodes with the computed value.

static auto make_int_literal(int64_t value, const HirType& type, const SourceSpan& span)
    -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    HirLiteralExpr lit;
    lit.id = HirId{0};
    lit.value = value;
    lit.type = type;
    lit.span = span;
    expr->kind = std::move(lit);
    return expr;
}

static auto make_uint_literal(uint64_t value, const HirType& type, const SourceSpan& span)
    -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    HirLiteralExpr lit;
    lit.id = HirId{0};
    lit.value = value;
    lit.type = type;
    lit.span = span;
    expr->kind = std::move(lit);
    return expr;
}

static auto make_float_literal(double value, const HirType& type, const SourceSpan& span)
    -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    HirLiteralExpr lit;
    lit.id = HirId{0};
    lit.value = value;
    lit.type = type;
    lit.span = span;
    expr->kind = std::move(lit);
    return expr;
}

static auto make_bool_literal(bool value, const SourceSpan& span) -> HirExprPtr {
    auto expr = std::make_unique<HirExpr>();
    HirLiteralExpr lit;
    lit.id = HirId{0};
    lit.value = value;
    lit.type = types::make_bool();
    lit.span = span;
    expr->kind = std::move(lit);
    return expr;
}

// ============================================================================
// Constant Folding Implementation
// ============================================================================
//
// Recursively traverses expressions and replaces constant subexpressions
// with their evaluated results. Returns true if any folding occurred.
//
// Algorithm:
// 1. Recursively fold child expressions first (post-order)
// 2. Check if current expression can be folded (both operands are literals)
// 3. Evaluate and replace if possible
// 4. Handle short-circuit logic (false && x -> false, true || x -> true)

auto ConstantFolding::run(HirModule& module) -> bool {
    changed_ = false;

    for (auto& func : module.functions) {
        fold_function(func);
    }

    return changed_;
}

auto ConstantFolding::run_pass(HirModule& module) -> bool {
    ConstantFolding pass;
    return pass.run(module);
}

void ConstantFolding::fold_function(HirFunction& func) {
    if (!func.body)
        return;
    fold_expr(func.body.value());
}

void ConstantFolding::fold_stmt(HirStmt& stmt) {
    std::visit(
        [this](auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, HirLetStmt>) {
                if (s.init) {
                    fold_expr(s.init.value());
                }
            } else if constexpr (std::is_same_v<T, HirExprStmt>) {
                fold_expr(s.expr);
            }
        },
        stmt.kind);
}

auto ConstantFolding::fold_expr(HirExprPtr& expr) -> bool {
    if (!expr)
        return false;

    bool folded = false;

    std::visit(
        [this, &expr, &folded](auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                fold_expr(e.left);
                fold_expr(e.right);

                if (auto result = try_fold_binary(e)) {
                    expr = std::move(*result);
                    changed_ = true;
                    folded = true;
                }
            } else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                fold_expr(e.operand);

                if (auto result = try_fold_unary(e)) {
                    expr = std::move(*result);
                    changed_ = true;
                    folded = true;
                }
            } else if constexpr (std::is_same_v<T, HirIfExpr>) {
                fold_expr(e.condition);
                fold_expr(e.then_branch);
                if (e.else_branch)
                    fold_expr(e.else_branch.value());
            } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                for (auto& stmt : e.stmts) {
                    fold_stmt(*stmt);
                }
                if (e.expr)
                    fold_expr(e.expr.value());
            } else if constexpr (std::is_same_v<T, HirCallExpr>) {
                // func_name is a string, not an expression - no need to fold
                for (auto& arg : e.args) {
                    fold_expr(arg);
                }
            } else if constexpr (std::is_same_v<T, HirMethodCallExpr>) {
                fold_expr(e.receiver);
                for (auto& arg : e.args) {
                    fold_expr(arg);
                }
            } else if constexpr (std::is_same_v<T, HirFieldExpr>) {
                fold_expr(e.object);
            } else if constexpr (std::is_same_v<T, HirIndexExpr>) {
                fold_expr(e.object);
                fold_expr(e.index);
            } else if constexpr (std::is_same_v<T, HirTupleExpr>) {
                for (auto& elem : e.elements) {
                    fold_expr(elem);
                }
            } else if constexpr (std::is_same_v<T, HirArrayExpr>) {
                for (auto& elem : e.elements) {
                    fold_expr(elem);
                }
            } else if constexpr (std::is_same_v<T, HirArrayRepeatExpr>) {
                fold_expr(e.value);
            } else if constexpr (std::is_same_v<T, HirStructExpr>) {
                for (auto& [name, val] : e.fields) {
                    fold_expr(val);
                }
                if (e.base)
                    fold_expr(e.base.value());
            } else if constexpr (std::is_same_v<T, HirEnumExpr>) {
                for (auto& payload : e.payload) {
                    fold_expr(payload);
                }
            } else if constexpr (std::is_same_v<T, HirLoopExpr>) {
                fold_expr(e.condition);
                fold_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirWhileExpr>) {
                fold_expr(e.condition);
                fold_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirForExpr>) {
                fold_expr(e.iter);
                fold_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirWhenExpr>) {
                fold_expr(e.scrutinee);
                for (auto& arm : e.arms) {
                    if (arm.guard)
                        fold_expr(arm.guard.value());
                    fold_expr(arm.body);
                }
            } else if constexpr (std::is_same_v<T, HirClosureExpr>) {
                fold_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirTryExpr>) {
                fold_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirAwaitExpr>) {
                fold_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirReturnExpr>) {
                if (e.value)
                    fold_expr(e.value.value());
            } else if constexpr (std::is_same_v<T, HirBreakExpr>) {
                if (e.value)
                    fold_expr(e.value.value());
            } else if constexpr (std::is_same_v<T, HirCastExpr>) {
                fold_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirAssignExpr>) {
                fold_expr(e.target);
                fold_expr(e.value);
            } else if constexpr (std::is_same_v<T, HirCompoundAssignExpr>) {
                fold_expr(e.target);
                fold_expr(e.value);
            } else if constexpr (std::is_same_v<T, HirLowlevelExpr>) {
                for (auto& stmt : e.stmts) {
                    fold_stmt(*stmt);
                }
                if (e.expr)
                    fold_expr(e.expr.value());
            }
            // HirLiteralExpr, HirVarExpr, HirContinueExpr - no subexpressions
        },
        expr->kind);

    return folded;
}

auto ConstantFolding::try_fold_binary(HirBinaryExpr& binary) -> std::optional<HirExprPtr> {
    auto* left_lit = std::get_if<HirLiteralExpr>(&binary.left->kind);
    auto* right_lit = std::get_if<HirLiteralExpr>(&binary.right->kind);

    if (!left_lit || !right_lit) {
        // Short-circuit evaluation for logical operators
        if (left_lit) {
            if (auto* left_bool = std::get_if<bool>(&left_lit->value)) {
                if (binary.op == HirBinOp::And && !*left_bool) {
                    return make_bool_literal(false, binary.span);
                }
                if (binary.op == HirBinOp::Or && *left_bool) {
                    return make_bool_literal(true, binary.span);
                }
            }
        }
        return std::nullopt;
    }

    // Both operands are literals
    if (auto* left_int = std::get_if<int64_t>(&left_lit->value)) {
        if (auto* right_int = std::get_if<int64_t>(&right_lit->value)) {
            return eval_int_binary(binary.op, *left_int, *right_int, binary.type, binary.span);
        }
    }

    if (auto* left_uint = std::get_if<uint64_t>(&left_lit->value)) {
        if (auto* right_uint = std::get_if<uint64_t>(&right_lit->value)) {
            return eval_uint_binary(binary.op, *left_uint, *right_uint, binary.type, binary.span);
        }
    }

    if (auto* left_float = std::get_if<double>(&left_lit->value)) {
        if (auto* right_float = std::get_if<double>(&right_lit->value)) {
            return eval_float_binary(binary.op, *left_float, *right_float, binary.type,
                                     binary.span);
        }
    }

    if (auto* left_bool = std::get_if<bool>(&left_lit->value)) {
        if (auto* right_bool = std::get_if<bool>(&right_lit->value)) {
            return eval_bool_binary(binary.op, *left_bool, *right_bool, binary.span);
        }
    }

    return std::nullopt;
}

auto ConstantFolding::try_fold_unary(HirUnaryExpr& unary) -> std::optional<HirExprPtr> {
    auto* operand_lit = std::get_if<HirLiteralExpr>(&unary.operand->kind);
    if (!operand_lit)
        return std::nullopt;

    switch (unary.op) {
    case HirUnaryOp::Neg:
        if (auto* val = std::get_if<int64_t>(&operand_lit->value)) {
            return make_int_literal(-(*val), unary.type, unary.span);
        }
        if (auto* val = std::get_if<double>(&operand_lit->value)) {
            return make_float_literal(-(*val), unary.type, unary.span);
        }
        break;

    case HirUnaryOp::Not:
        if (auto* val = std::get_if<bool>(&operand_lit->value)) {
            return make_bool_literal(!(*val), unary.span);
        }
        break;

    case HirUnaryOp::BitNot:
        if (auto* val = std::get_if<int64_t>(&operand_lit->value)) {
            return make_int_literal(~(*val), unary.type, unary.span);
        }
        if (auto* val = std::get_if<uint64_t>(&operand_lit->value)) {
            return make_uint_literal(~(*val), unary.type, unary.span);
        }
        break;

    default:
        break;
    }

    return std::nullopt;
}

auto ConstantFolding::eval_int_binary(HirBinOp op, int64_t left, int64_t right, const HirType& type,
                                      const SourceSpan& span) -> std::optional<HirExprPtr> {
    switch (op) {
    case HirBinOp::Add:
        return make_int_literal(left + right, type, span);
    case HirBinOp::Sub:
        return make_int_literal(left - right, type, span);
    case HirBinOp::Mul:
        return make_int_literal(left * right, type, span);
    case HirBinOp::Div:
        if (right == 0)
            return std::nullopt;
        return make_int_literal(left / right, type, span);
    case HirBinOp::Mod:
        if (right == 0)
            return std::nullopt;
        return make_int_literal(left % right, type, span);
    case HirBinOp::Eq:
        return make_bool_literal(left == right, span);
    case HirBinOp::Ne:
        return make_bool_literal(left != right, span);
    case HirBinOp::Lt:
        return make_bool_literal(left < right, span);
    case HirBinOp::Le:
        return make_bool_literal(left <= right, span);
    case HirBinOp::Gt:
        return make_bool_literal(left > right, span);
    case HirBinOp::Ge:
        return make_bool_literal(left >= right, span);
    case HirBinOp::BitAnd:
        return make_int_literal(left & right, type, span);
    case HirBinOp::BitOr:
        return make_int_literal(left | right, type, span);
    case HirBinOp::BitXor:
        return make_int_literal(left ^ right, type, span);
    case HirBinOp::Shl:
        return make_int_literal(left << right, type, span);
    case HirBinOp::Shr:
        return make_int_literal(left >> right, type, span);
    default:
        return std::nullopt;
    }
}

auto ConstantFolding::eval_uint_binary(HirBinOp op, uint64_t left, uint64_t right,
                                       const HirType& type, const SourceSpan& span)
    -> std::optional<HirExprPtr> {
    switch (op) {
    case HirBinOp::Add:
        return make_uint_literal(left + right, type, span);
    case HirBinOp::Sub:
        return make_uint_literal(left - right, type, span);
    case HirBinOp::Mul:
        return make_uint_literal(left * right, type, span);
    case HirBinOp::Div:
        if (right == 0)
            return std::nullopt;
        return make_uint_literal(left / right, type, span);
    case HirBinOp::Mod:
        if (right == 0)
            return std::nullopt;
        return make_uint_literal(left % right, type, span);
    case HirBinOp::Eq:
        return make_bool_literal(left == right, span);
    case HirBinOp::Ne:
        return make_bool_literal(left != right, span);
    case HirBinOp::Lt:
        return make_bool_literal(left < right, span);
    case HirBinOp::Le:
        return make_bool_literal(left <= right, span);
    case HirBinOp::Gt:
        return make_bool_literal(left > right, span);
    case HirBinOp::Ge:
        return make_bool_literal(left >= right, span);
    case HirBinOp::BitAnd:
        return make_uint_literal(left & right, type, span);
    case HirBinOp::BitOr:
        return make_uint_literal(left | right, type, span);
    case HirBinOp::BitXor:
        return make_uint_literal(left ^ right, type, span);
    case HirBinOp::Shl:
        return make_uint_literal(left << right, type, span);
    case HirBinOp::Shr:
        return make_uint_literal(left >> right, type, span);
    default:
        return std::nullopt;
    }
}

auto ConstantFolding::eval_float_binary(HirBinOp op, double left, double right, const HirType& type,
                                        const SourceSpan& span) -> std::optional<HirExprPtr> {
    switch (op) {
    case HirBinOp::Add:
        return make_float_literal(left + right, type, span);
    case HirBinOp::Sub:
        return make_float_literal(left - right, type, span);
    case HirBinOp::Mul:
        return make_float_literal(left * right, type, span);
    case HirBinOp::Div:
        return make_float_literal(left / right, type, span);
    case HirBinOp::Mod:
        return make_float_literal(std::fmod(left, right), type, span);
    case HirBinOp::Eq:
        return make_bool_literal(left == right, span);
    case HirBinOp::Ne:
        return make_bool_literal(left != right, span);
    case HirBinOp::Lt:
        return make_bool_literal(left < right, span);
    case HirBinOp::Le:
        return make_bool_literal(left <= right, span);
    case HirBinOp::Gt:
        return make_bool_literal(left > right, span);
    case HirBinOp::Ge:
        return make_bool_literal(left >= right, span);
    default:
        return std::nullopt;
    }
}

auto ConstantFolding::eval_bool_binary(HirBinOp op, bool left, bool right, const SourceSpan& span)
    -> std::optional<HirExprPtr> {
    switch (op) {
    case HirBinOp::And:
        return make_bool_literal(left && right, span);
    case HirBinOp::Or:
        return make_bool_literal(left || right, span);
    case HirBinOp::Eq:
        return make_bool_literal(left == right, span);
    case HirBinOp::Ne:
        return make_bool_literal(left != right, span);
    default:
        return std::nullopt;
    }
}

// ============================================================================
// Dead Code Elimination Implementation
// ============================================================================
//
// Removes provably unreachable code. Currently handles:
// - Constant condition if expressions (if true/false)
// - Tracks terminating statements (return, break, continue)
// - Identifies pure expressions (no side effects)
//
// Pure expressions that are unused could be eliminated (future work).

auto DeadCodeElimination::run(HirModule& module) -> bool {
    changed_ = false;

    for (auto& func : module.functions) {
        eliminate_in_function(func);
    }

    return changed_;
}

auto DeadCodeElimination::run_pass(HirModule& module) -> bool {
    DeadCodeElimination pass;
    return pass.run(module);
}

void DeadCodeElimination::eliminate_in_function(HirFunction& func) {
    if (!func.body)
        return;
    eliminate_in_expr(func.body.value());
}

void DeadCodeElimination::eliminate_in_block(std::vector<HirStmt>& stmts) {
    // Currently a no-op - we work with HirStmtPtr vectors
    (void)stmts;
}

void DeadCodeElimination::eliminate_in_expr_stmt(HirStmt& stmt) {
    std::visit(
        [this](auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, HirLetStmt>) {
                if (s.init)
                    eliminate_in_expr(s.init.value());
            } else if constexpr (std::is_same_v<T, HirExprStmt>) {
                eliminate_in_expr(s.expr);
            }
        },
        stmt.kind);
}

auto DeadCodeElimination::eliminate_in_expr(HirExprPtr& expr) -> bool {
    if (!expr)
        return false;

    std::visit(
        [this, &expr](auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, HirIfExpr>) {
                eliminate_in_expr(e.condition);

                // Check if condition is constant
                if (auto* cond_lit = std::get_if<HirLiteralExpr>(&e.condition->kind)) {
                    if (auto* cond_bool = std::get_if<bool>(&cond_lit->value)) {
                        if (*cond_bool) {
                            // Condition is true, replace with then branch
                            expr = std::move(e.then_branch);
                            changed_ = true;
                            return;
                        } else if (e.else_branch) {
                            // Condition is false, replace with else branch
                            expr = std::move(e.else_branch.value());
                            changed_ = true;
                            return;
                        }
                        // If condition is false and no else, keep as-is (produces unit)
                    }
                }

                eliminate_in_expr(e.then_branch);
                if (e.else_branch)
                    eliminate_in_expr(e.else_branch.value());
            } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                for (auto& stmt : e.stmts) {
                    eliminate_in_expr_stmt(*stmt);
                }
                if (e.expr)
                    eliminate_in_expr(e.expr.value());
            } else if constexpr (std::is_same_v<T, HirLoopExpr>) {
                eliminate_in_expr(e.condition);
                eliminate_in_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirWhileExpr>) {
                eliminate_in_expr(e.condition);
                eliminate_in_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirForExpr>) {
                eliminate_in_expr(e.iter);
                eliminate_in_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirWhenExpr>) {
                eliminate_in_expr(e.scrutinee);
                for (auto& arm : e.arms) {
                    if (arm.guard)
                        eliminate_in_expr(arm.guard.value());
                    eliminate_in_expr(arm.body);
                }
            } else if constexpr (std::is_same_v<T, HirClosureExpr>) {
                eliminate_in_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirCallExpr>) {
                // func_name is a string, no expression to eliminate
                for (auto& arg : e.args) {
                    eliminate_in_expr(arg);
                }
            } else if constexpr (std::is_same_v<T, HirMethodCallExpr>) {
                eliminate_in_expr(e.receiver);
                for (auto& arg : e.args) {
                    eliminate_in_expr(arg);
                }
            } else if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                eliminate_in_expr(e.left);
                eliminate_in_expr(e.right);
            } else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                eliminate_in_expr(e.operand);
            } else if constexpr (std::is_same_v<T, HirFieldExpr>) {
                eliminate_in_expr(e.object);
            } else if constexpr (std::is_same_v<T, HirIndexExpr>) {
                eliminate_in_expr(e.object);
                eliminate_in_expr(e.index);
            } else if constexpr (std::is_same_v<T, HirTupleExpr>) {
                for (auto& elem : e.elements) {
                    eliminate_in_expr(elem);
                }
            } else if constexpr (std::is_same_v<T, HirArrayExpr>) {
                for (auto& elem : e.elements) {
                    eliminate_in_expr(elem);
                }
            } else if constexpr (std::is_same_v<T, HirArrayRepeatExpr>) {
                eliminate_in_expr(e.value);
            } else if constexpr (std::is_same_v<T, HirStructExpr>) {
                for (auto& [name, val] : e.fields) {
                    eliminate_in_expr(val);
                }
                if (e.base)
                    eliminate_in_expr(e.base.value());
            } else if constexpr (std::is_same_v<T, HirEnumExpr>) {
                for (auto& payload : e.payload) {
                    eliminate_in_expr(payload);
                }
            } else if constexpr (std::is_same_v<T, HirTryExpr>) {
                eliminate_in_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirAwaitExpr>) {
                eliminate_in_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirReturnExpr>) {
                if (e.value)
                    eliminate_in_expr(e.value.value());
            } else if constexpr (std::is_same_v<T, HirBreakExpr>) {
                if (e.value)
                    eliminate_in_expr(e.value.value());
            } else if constexpr (std::is_same_v<T, HirCastExpr>) {
                eliminate_in_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirAssignExpr>) {
                eliminate_in_expr(e.target);
                eliminate_in_expr(e.value);
            } else if constexpr (std::is_same_v<T, HirCompoundAssignExpr>) {
                eliminate_in_expr(e.target);
                eliminate_in_expr(e.value);
            } else if constexpr (std::is_same_v<T, HirLowlevelExpr>) {
                for (auto& stmt : e.stmts) {
                    eliminate_in_expr_stmt(*stmt);
                }
                if (e.expr)
                    eliminate_in_expr(e.expr.value());
            }
            // HirLiteralExpr, HirVarExpr, HirContinueExpr - no subexpressions
        },
        expr->kind);

    return false;
}

auto DeadCodeElimination::is_terminating(const HirStmt& stmt) -> bool {
    if (auto* expr_stmt = std::get_if<HirExprStmt>(&stmt.kind)) {
        if (std::holds_alternative<HirReturnExpr>(expr_stmt->expr->kind))
            return true;
        if (std::holds_alternative<HirBreakExpr>(expr_stmt->expr->kind))
            return true;
        if (std::holds_alternative<HirContinueExpr>(expr_stmt->expr->kind))
            return true;
    }
    return false;
}

auto DeadCodeElimination::is_pure_expr(const HirExpr& expr) -> bool {
    return std::visit(
        [this](const auto& e) -> bool {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, HirLiteralExpr>) {
                return true;
            } else if constexpr (std::is_same_v<T, HirVarExpr>) {
                return true;
            } else if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                return is_pure_expr(*e.left) && is_pure_expr(*e.right);
            } else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                if (e.op == HirUnaryOp::Deref)
                    return false;
                return is_pure_expr(*e.operand);
            } else if constexpr (std::is_same_v<T, HirTupleExpr>) {
                for (const auto& elem : e.elements) {
                    if (!is_pure_expr(*elem))
                        return false;
                }
                return true;
            } else if constexpr (std::is_same_v<T, HirArrayExpr>) {
                for (const auto& elem : e.elements) {
                    if (!is_pure_expr(*elem))
                        return false;
                }
                return true;
            } else if constexpr (std::is_same_v<T, HirFieldExpr>) {
                return is_pure_expr(*e.object);
            } else if constexpr (std::is_same_v<T, HirIndexExpr>) {
                return is_pure_expr(*e.object) && is_pure_expr(*e.index);
            } else {
                return false;
            }
        },
        expr.kind);
}

auto DeadCodeElimination::has_side_effects(const HirExpr& expr) -> bool {
    return !is_pure_expr(expr);
}

} // namespace tml::hir
