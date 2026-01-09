//! # HIR Optimization Passes Implementation
//!
//! Implements constant folding, dead code elimination, inlining,
//! and closure optimization passes for HIR.
//!
//! ## Overview
//!
//! HIR passes perform source-level optimizations before lowering to MIR.
//! These passes operate on the type-annotated HIR representation.
//!
//! ## Available Passes
//!
//! | Pass                  | Level | Description                              |
//! |-----------------------|-------|------------------------------------------|
//! | `ConstantFolding`     | O1+   | Evaluate constant expressions at compile |
//! | `DeadCodeElimination` | O2+   | Remove unreachable code branches         |
//! | `ClosureOptimization` | O3    | Optimize closure captures and escape     |
//! | `Inlining`            | O3    | Inline small non-recursive functions     |
//!
//! ## Pass Execution
//!
//! Passes can be run individually or via `HirPassManager`:
//!
//! ```cpp
//! // Individual pass
//! ConstantFolding::run_pass(module);
//!
//! // Pass manager (runs to fixpoint at O3)
//! optimize_hir_level(module, 3);
//! ```
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
    if (!func.body) return;
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
    if (!expr) return false;

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
                if (e.else_branch) fold_expr(e.else_branch.value());
            } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                for (auto& stmt : e.stmts) {
                    fold_stmt(*stmt);
                }
                if (e.expr) fold_expr(e.expr.value());
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
                if (e.base) fold_expr(e.base.value());
            } else if constexpr (std::is_same_v<T, HirEnumExpr>) {
                for (auto& payload : e.payload) {
                    fold_expr(payload);
                }
            } else if constexpr (std::is_same_v<T, HirLoopExpr>) {
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
                    if (arm.guard) fold_expr(arm.guard.value());
                    fold_expr(arm.body);
                }
            } else if constexpr (std::is_same_v<T, HirClosureExpr>) {
                fold_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirTryExpr>) {
                fold_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirAwaitExpr>) {
                fold_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirReturnExpr>) {
                if (e.value) fold_expr(e.value.value());
            } else if constexpr (std::is_same_v<T, HirBreakExpr>) {
                if (e.value) fold_expr(e.value.value());
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
                if (e.expr) fold_expr(e.expr.value());
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
    if (!operand_lit) return std::nullopt;

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
        if (right == 0) return std::nullopt;
        return make_int_literal(left / right, type, span);
    case HirBinOp::Mod:
        if (right == 0) return std::nullopt;
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
        if (right == 0) return std::nullopt;
        return make_uint_literal(left / right, type, span);
    case HirBinOp::Mod:
        if (right == 0) return std::nullopt;
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
    if (!func.body) return;
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
                if (s.init) eliminate_in_expr(s.init.value());
            } else if constexpr (std::is_same_v<T, HirExprStmt>) {
                eliminate_in_expr(s.expr);
            }
        },
        stmt.kind);
}

auto DeadCodeElimination::eliminate_in_expr(HirExprPtr& expr) -> bool {
    if (!expr) return false;

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
                if (e.else_branch) eliminate_in_expr(e.else_branch.value());
            } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                for (auto& stmt : e.stmts) {
                    eliminate_in_expr_stmt(*stmt);
                }
                if (e.expr) eliminate_in_expr(e.expr.value());
            } else if constexpr (std::is_same_v<T, HirLoopExpr>) {
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
                    if (arm.guard) eliminate_in_expr(arm.guard.value());
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
                if (e.base) eliminate_in_expr(e.base.value());
            } else if constexpr (std::is_same_v<T, HirEnumExpr>) {
                for (auto& payload : e.payload) {
                    eliminate_in_expr(payload);
                }
            } else if constexpr (std::is_same_v<T, HirTryExpr>) {
                eliminate_in_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirAwaitExpr>) {
                eliminate_in_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirReturnExpr>) {
                if (e.value) eliminate_in_expr(e.value.value());
            } else if constexpr (std::is_same_v<T, HirBreakExpr>) {
                if (e.value) eliminate_in_expr(e.value.value());
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
                if (e.expr) eliminate_in_expr(e.expr.value());
            }
            // HirLiteralExpr, HirVarExpr, HirContinueExpr - no subexpressions
        },
        expr->kind);

    return false;
}

auto DeadCodeElimination::is_terminating(const HirStmt& stmt) -> bool {
    if (auto* expr_stmt = std::get_if<HirExprStmt>(&stmt.kind)) {
        if (std::holds_alternative<HirReturnExpr>(expr_stmt->expr->kind)) return true;
        if (std::holds_alternative<HirBreakExpr>(expr_stmt->expr->kind)) return true;
        if (std::holds_alternative<HirContinueExpr>(expr_stmt->expr->kind)) return true;
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
                if (e.op == HirUnaryOp::Deref) return false;
                return is_pure_expr(*e.operand);
            } else if constexpr (std::is_same_v<T, HirTupleExpr>) {
                for (const auto& elem : e.elements) {
                    if (!is_pure_expr(*elem)) return false;
                }
                return true;
            } else if constexpr (std::is_same_v<T, HirArrayExpr>) {
                for (const auto& elem : e.elements) {
                    if (!is_pure_expr(*elem)) return false;
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

// ============================================================================
// Inlining Implementation
// ============================================================================
//
// Function inlining replaces call sites with the function body, eliminating
// call overhead for small functions. This is most effective for:
// - Small leaf functions (no calls)
// - Functions marked @inline
// - Pure functions called in hot paths
//
// Algorithm:
// 1. Build map of inlinable functions
// 2. Walk all expressions looking for call sites
// 3. For eligible calls, substitute parameters and inline body
// 4. Repeat until no more inlining opportunities

// Helper to count statements in a function body
static auto count_statements(const HirExpr& expr) -> size_t {
    size_t count = 0;

    std::visit(
        [&count](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, HirBlockExpr>) {
                count += e.stmts.size();
                if (e.expr) {
                    count += 1; // Final expression counts as one
                }
            } else {
                count = 1; // Non-block body counts as one statement
            }
        },
        expr.kind);

    return count;
}

// Helper to check if function is recursive (calls itself)
static auto is_recursive(const HirFunction& func) -> bool {
    if (!func.body) return false;

    bool found_self_call = false;
    std::function<void(const HirExpr&)> check_expr = [&](const HirExpr& expr) {
        if (found_self_call) return;

        std::visit(
            [&](const auto& e) {
                using T = std::decay_t<decltype(e)>;

                if constexpr (std::is_same_v<T, HirCallExpr>) {
                    if (e.func_name == func.name || e.func_name == func.mangled_name) {
                        found_self_call = true;
                        return;
                    }
                    for (const auto& arg : e.args) {
                        check_expr(*arg);
                    }
                } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                    for (const auto& stmt : e.stmts) {
                        if (auto* expr_stmt = std::get_if<HirExprStmt>(&stmt->kind)) {
                            check_expr(*expr_stmt->expr);
                        } else if (auto* let_stmt = std::get_if<HirLetStmt>(&stmt->kind)) {
                            if (let_stmt->init) {
                                check_expr(**let_stmt->init);
                            }
                        }
                    }
                    if (e.expr) check_expr(**e.expr);
                } else if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                    check_expr(*e.left);
                    check_expr(*e.right);
                } else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                    check_expr(*e.operand);
                } else if constexpr (std::is_same_v<T, HirIfExpr>) {
                    check_expr(*e.condition);
                    check_expr(*e.then_branch);
                    if (e.else_branch) check_expr(**e.else_branch);
                } else if constexpr (std::is_same_v<T, HirMethodCallExpr>) {
                    check_expr(*e.receiver);
                    for (const auto& arg : e.args) {
                        check_expr(*arg);
                    }
                }
            },
            expr.kind);
    };

    check_expr(**func.body);
    return found_self_call;
}

// Helper to deep clone an expression (for inlining)
static auto clone_expr(const HirExpr& expr) -> HirExprPtr;

// Forward declaration
static auto clone_pattern(const HirPattern& pattern) -> HirPatternPtr;

static auto clone_pattern(const HirPattern& pattern) -> HirPatternPtr {
    auto result = std::make_unique<HirPattern>();

    std::visit(
        [&result](const auto& p) {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, HirWildcardPattern>) {
                result->kind = p;
            } else if constexpr (std::is_same_v<T, HirBindingPattern>) {
                result->kind = p;
            } else if constexpr (std::is_same_v<T, HirLiteralPattern>) {
                result->kind = p;
            } else if constexpr (std::is_same_v<T, HirTuplePattern>) {
                HirTuplePattern new_pattern;
                new_pattern.id = p.id;
                for (const auto& elem : p.elements) {
                    new_pattern.elements.push_back(clone_pattern(*elem));
                }
                new_pattern.type = p.type;
                new_pattern.span = p.span;
                result->kind = std::move(new_pattern);
            } else if constexpr (std::is_same_v<T, HirStructPattern>) {
                HirStructPattern new_pattern;
                new_pattern.id = p.id;
                new_pattern.struct_name = p.struct_name;
                for (const auto& [name, pat] : p.fields) {
                    new_pattern.fields.push_back({name, clone_pattern(*pat)});
                }
                new_pattern.has_rest = p.has_rest;
                new_pattern.type = p.type;
                new_pattern.span = p.span;
                result->kind = std::move(new_pattern);
            } else if constexpr (std::is_same_v<T, HirEnumPattern>) {
                HirEnumPattern new_pattern;
                new_pattern.id = p.id;
                new_pattern.enum_name = p.enum_name;
                new_pattern.variant_name = p.variant_name;
                new_pattern.variant_index = p.variant_index;
                if (p.payload) {
                    std::vector<HirPatternPtr> new_payload;
                    for (const auto& pat : *p.payload) {
                        new_payload.push_back(clone_pattern(*pat));
                    }
                    new_pattern.payload = std::move(new_payload);
                }
                new_pattern.type = p.type;
                new_pattern.span = p.span;
                result->kind = std::move(new_pattern);
            } else if constexpr (std::is_same_v<T, HirOrPattern>) {
                HirOrPattern new_pattern;
                new_pattern.id = p.id;
                for (const auto& alt : p.alternatives) {
                    new_pattern.alternatives.push_back(clone_pattern(*alt));
                }
                new_pattern.type = p.type;
                new_pattern.span = p.span;
                result->kind = std::move(new_pattern);
            } else if constexpr (std::is_same_v<T, HirRangePattern>) {
                result->kind = p;
            } else if constexpr (std::is_same_v<T, HirArrayPattern>) {
                HirArrayPattern new_pattern;
                new_pattern.id = p.id;
                for (const auto& elem : p.elements) {
                    new_pattern.elements.push_back(clone_pattern(*elem));
                }
                if (p.rest) {
                    new_pattern.rest = clone_pattern(**p.rest);
                }
                new_pattern.type = p.type;
                new_pattern.span = p.span;
                result->kind = std::move(new_pattern);
            }
        },
        pattern.kind);

    return result;
}

static auto clone_stmt(const HirStmt& stmt) -> HirStmtPtr {
    auto result = std::make_unique<HirStmt>();

    std::visit(
        [&result](const auto& s) {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, HirLetStmt>) {
                HirLetStmt new_let;
                new_let.id = s.id;
                new_let.pattern = clone_pattern(*s.pattern);
                new_let.type = s.type;
                if (s.init) {
                    new_let.init = clone_expr(**s.init);
                }
                new_let.span = s.span;
                result->kind = std::move(new_let);
            } else if constexpr (std::is_same_v<T, HirExprStmt>) {
                HirExprStmt new_stmt;
                new_stmt.id = s.id;
                new_stmt.expr = clone_expr(*s.expr);
                new_stmt.span = s.span;
                result->kind = std::move(new_stmt);
            }
        },
        stmt.kind);

    return result;
}

static auto clone_expr(const HirExpr& expr) -> HirExprPtr {
    auto result = std::make_unique<HirExpr>();

    std::visit(
        [&result](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, HirLiteralExpr>) {
                result->kind = e;
            } else if constexpr (std::is_same_v<T, HirVarExpr>) {
                result->kind = e;
            } else if constexpr (std::is_same_v<T, HirContinueExpr>) {
                result->kind = e;
            } else if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                HirBinaryExpr new_expr;
                new_expr.id = e.id;
                new_expr.op = e.op;
                new_expr.left = clone_expr(*e.left);
                new_expr.right = clone_expr(*e.right);
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                HirUnaryExpr new_expr;
                new_expr.id = e.id;
                new_expr.op = e.op;
                new_expr.operand = clone_expr(*e.operand);
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                HirBlockExpr new_expr;
                new_expr.id = e.id;
                for (const auto& stmt : e.stmts) {
                    new_expr.stmts.push_back(clone_stmt(*stmt));
                }
                if (e.expr) {
                    new_expr.expr = clone_expr(**e.expr);
                }
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirCallExpr>) {
                HirCallExpr new_expr;
                new_expr.id = e.id;
                new_expr.func_name = e.func_name;
                new_expr.type_args = e.type_args;
                for (const auto& arg : e.args) {
                    new_expr.args.push_back(clone_expr(*arg));
                }
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirMethodCallExpr>) {
                HirMethodCallExpr new_expr;
                new_expr.id = e.id;
                new_expr.receiver = clone_expr(*e.receiver);
                new_expr.method_name = e.method_name;
                for (const auto& arg : e.args) {
                    new_expr.args.push_back(clone_expr(*arg));
                }
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirFieldExpr>) {
                HirFieldExpr new_expr;
                new_expr.id = e.id;
                new_expr.object = clone_expr(*e.object);
                new_expr.field_name = e.field_name;
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirIndexExpr>) {
                HirIndexExpr new_expr;
                new_expr.id = e.id;
                new_expr.object = clone_expr(*e.object);
                new_expr.index = clone_expr(*e.index);
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirTupleExpr>) {
                HirTupleExpr new_expr;
                new_expr.id = e.id;
                for (const auto& elem : e.elements) {
                    new_expr.elements.push_back(clone_expr(*elem));
                }
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirArrayExpr>) {
                HirArrayExpr new_expr;
                new_expr.id = e.id;
                for (const auto& elem : e.elements) {
                    new_expr.elements.push_back(clone_expr(*elem));
                }
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirArrayRepeatExpr>) {
                HirArrayRepeatExpr new_expr;
                new_expr.id = e.id;
                new_expr.value = clone_expr(*e.value);
                new_expr.count = e.count;
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirStructExpr>) {
                HirStructExpr new_expr;
                new_expr.id = e.id;
                new_expr.struct_name = e.struct_name;
                for (const auto& [name, val] : e.fields) {
                    new_expr.fields.push_back({name, clone_expr(*val)});
                }
                if (e.base) {
                    new_expr.base = clone_expr(**e.base);
                }
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirEnumExpr>) {
                HirEnumExpr new_expr;
                new_expr.id = e.id;
                new_expr.enum_name = e.enum_name;
                new_expr.variant_name = e.variant_name;
                for (const auto& payload : e.payload) {
                    new_expr.payload.push_back(clone_expr(*payload));
                }
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirIfExpr>) {
                HirIfExpr new_expr;
                new_expr.id = e.id;
                new_expr.condition = clone_expr(*e.condition);
                new_expr.then_branch = clone_expr(*e.then_branch);
                if (e.else_branch) {
                    new_expr.else_branch = clone_expr(**e.else_branch);
                }
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirWhenExpr>) {
                HirWhenExpr new_expr;
                new_expr.id = e.id;
                new_expr.scrutinee = clone_expr(*e.scrutinee);
                for (const auto& arm : e.arms) {
                    HirWhenArm new_arm;
                    new_arm.pattern = clone_pattern(*arm.pattern);
                    if (arm.guard) {
                        new_arm.guard = clone_expr(**arm.guard);
                    }
                    new_arm.body = clone_expr(*arm.body);
                    new_expr.arms.push_back(std::move(new_arm));
                }
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirLoopExpr>) {
                HirLoopExpr new_expr;
                new_expr.id = e.id;
                new_expr.label = e.label;
                new_expr.body = clone_expr(*e.body);
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirWhileExpr>) {
                HirWhileExpr new_expr;
                new_expr.id = e.id;
                new_expr.label = e.label;
                new_expr.condition = clone_expr(*e.condition);
                new_expr.body = clone_expr(*e.body);
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirForExpr>) {
                HirForExpr new_expr;
                new_expr.id = e.id;
                new_expr.label = e.label;
                new_expr.pattern = clone_pattern(*e.pattern);
                new_expr.iter = clone_expr(*e.iter);
                new_expr.body = clone_expr(*e.body);
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirReturnExpr>) {
                HirReturnExpr new_expr;
                new_expr.id = e.id;
                if (e.value) {
                    new_expr.value = clone_expr(**e.value);
                }
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirBreakExpr>) {
                HirBreakExpr new_expr;
                new_expr.id = e.id;
                new_expr.label = e.label;
                if (e.value) {
                    new_expr.value = clone_expr(**e.value);
                }
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirClosureExpr>) {
                HirClosureExpr new_expr;
                new_expr.id = e.id;
                new_expr.params = e.params;
                new_expr.body = clone_expr(*e.body);
                new_expr.captures = e.captures;
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirCastExpr>) {
                HirCastExpr new_expr;
                new_expr.id = e.id;
                new_expr.expr = clone_expr(*e.expr);
                new_expr.target_type = e.target_type;
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirTryExpr>) {
                HirTryExpr new_expr;
                new_expr.id = e.id;
                new_expr.expr = clone_expr(*e.expr);
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirAwaitExpr>) {
                HirAwaitExpr new_expr;
                new_expr.id = e.id;
                new_expr.expr = clone_expr(*e.expr);
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirAssignExpr>) {
                HirAssignExpr new_expr;
                new_expr.id = e.id;
                new_expr.target = clone_expr(*e.target);
                new_expr.value = clone_expr(*e.value);
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirCompoundAssignExpr>) {
                HirCompoundAssignExpr new_expr;
                new_expr.id = e.id;
                new_expr.op = e.op;
                new_expr.target = clone_expr(*e.target);
                new_expr.value = clone_expr(*e.value);
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            } else if constexpr (std::is_same_v<T, HirLowlevelExpr>) {
                HirLowlevelExpr new_expr;
                new_expr.id = e.id;
                for (const auto& stmt : e.stmts) {
                    new_expr.stmts.push_back(clone_stmt(*stmt));
                }
                if (e.expr) {
                    new_expr.expr = clone_expr(**e.expr);
                }
                new_expr.type = e.type;
                new_expr.span = e.span;
                result->kind = std::move(new_expr);
            }
        },
        expr.kind);

    return result;
}

// Helper to substitute parameter references with argument expressions
static void substitute_params(HirExpr& expr, const std::vector<std::string>& param_names,
                              const std::vector<HirExprPtr>& args) {
    std::visit(
        [&param_names, &args](auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, HirVarExpr>) {
                // Check if this variable is a parameter
                for (size_t i = 0; i < param_names.size(); ++i) {
                    if (e.name == param_names[i] && i < args.size()) {
                        // Can't replace in-place, this is handled at call site
                        return;
                    }
                }
            } else if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                substitute_params(*e.left, param_names, args);
                substitute_params(*e.right, param_names, args);
            } else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                substitute_params(*e.operand, param_names, args);
            } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                for (auto& stmt : e.stmts) {
                    if (auto* expr_stmt = std::get_if<HirExprStmt>(&stmt->kind)) {
                        substitute_params(*expr_stmt->expr, param_names, args);
                    } else if (auto* let_stmt = std::get_if<HirLetStmt>(&stmt->kind)) {
                        if (let_stmt->init) {
                            substitute_params(**let_stmt->init, param_names, args);
                        }
                    }
                }
                if (e.expr) substitute_params(**e.expr, param_names, args);
            } else if constexpr (std::is_same_v<T, HirCallExpr>) {
                for (auto& arg : e.args) {
                    substitute_params(*arg, param_names, args);
                }
            } else if constexpr (std::is_same_v<T, HirIfExpr>) {
                substitute_params(*e.condition, param_names, args);
                substitute_params(*e.then_branch, param_names, args);
                if (e.else_branch) substitute_params(**e.else_branch, param_names, args);
            }
        },
        expr.kind);
}

auto Inlining::run(HirModule& module) -> bool {
    changed_ = false;

    // Build map of inlinable functions
    std::unordered_map<std::string, const HirFunction*> inlinable;
    for (const auto& func : module.functions) {
        if (should_inline(func)) {
            inlinable[func.name] = &func;
            if (func.mangled_name != func.name) {
                inlinable[func.mangled_name] = &func;
            }
        }
    }

    if (inlinable.empty()) {
        return false;
    }

    // Process each function looking for inline opportunities
    for (auto& func : module.functions) {
        if (!func.body) continue;

        // Don't inline into functions that are themselves inlinable (avoid bloat)
        if (inlinable.count(func.name) > 0) continue;

        inline_calls_in_expr(func.body.value(), inlinable);
    }

    return changed_;
}

auto Inlining::run_pass(HirModule& module, size_t max_statements) -> bool {
    Inlining pass(max_statements);
    return pass.run(module);
}

auto Inlining::should_inline(const HirFunction& func) -> bool {
    // Don't inline extern functions
    if (func.is_extern) return false;

    // Don't inline functions without bodies
    if (!func.body) return false;

    // Check for @noinline attribute
    for (const auto& attr : func.attributes) {
        if (attr == "noinline") return false;
    }

    // Check for @inline attribute (always inline if present)
    bool has_inline_attr = false;
    for (const auto& attr : func.attributes) {
        if (attr == "inline") {
            has_inline_attr = true;
            break;
        }
    }

    // Check statement count
    size_t stmt_count = count_statements(**func.body);
    if (!has_inline_attr && stmt_count > max_statements_) {
        return false;
    }

    // Don't inline recursive functions
    if (is_recursive(func)) return false;

    return true;
}

auto Inlining::inline_call(HirCallExpr& call, const HirFunction& func)
    -> std::optional<HirExprPtr> {
    if (!func.body) return std::nullopt;

    // Clone the function body
    auto inlined_body = clone_expr(**func.body);

    // Build parameter name list
    std::vector<std::string> param_names;
    for (const auto& param : func.params) {
        param_names.push_back(param.name);
    }

    // Create let bindings for each argument and wrap in a block
    HirBlockExpr block;
    block.id = HirId{0};
    block.span = call.span;

    for (size_t i = 0; i < func.params.size() && i < call.args.size(); ++i) {
        // Create let statement: let param_name = arg
        auto pattern = std::make_unique<HirPattern>();
        pattern->kind =
            HirBindingPattern{HirId{0}, func.params[i].name, func.params[i].is_mut,
                              func.params[i].type, call.span};

        auto let_stmt = std::make_unique<HirStmt>();
        let_stmt->kind = HirLetStmt{HirId{0}, std::move(pattern), func.params[i].type,
                                    clone_expr(*call.args[i]), call.span};

        block.stmts.push_back(std::move(let_stmt));
    }

    // Add the inlined body as the final expression
    block.expr = std::move(inlined_body);
    block.type = func.return_type;

    auto result = std::make_unique<HirExpr>();
    result->kind = std::move(block);

    changed_ = true;
    return result;
}

void Inlining::inline_calls_in_expr(
    HirExprPtr& expr, const std::unordered_map<std::string, const HirFunction*>& inlinable) {
    if (!expr) return;

    std::visit(
        [this, &expr, &inlinable](auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, HirCallExpr>) {
                // First recurse into arguments
                for (auto& arg : e.args) {
                    inline_calls_in_expr(arg, inlinable);
                }

                // Check if this call can be inlined
                auto it = inlinable.find(e.func_name);
                if (it != inlinable.end()) {
                    if (auto inlined = inline_call(e, *it->second)) {
                        expr = std::move(*inlined);
                    }
                }
            } else if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                inline_calls_in_expr(e.left, inlinable);
                inline_calls_in_expr(e.right, inlinable);
            } else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                inline_calls_in_expr(e.operand, inlinable);
            } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                for (auto& stmt : e.stmts) {
                    if (auto* expr_stmt = std::get_if<HirExprStmt>(&stmt->kind)) {
                        inline_calls_in_expr(expr_stmt->expr, inlinable);
                    } else if (auto* let_stmt = std::get_if<HirLetStmt>(&stmt->kind)) {
                        if (let_stmt->init) {
                            inline_calls_in_expr(let_stmt->init.value(), inlinable);
                        }
                    }
                }
                if (e.expr) inline_calls_in_expr(e.expr.value(), inlinable);
            } else if constexpr (std::is_same_v<T, HirIfExpr>) {
                inline_calls_in_expr(e.condition, inlinable);
                inline_calls_in_expr(e.then_branch, inlinable);
                if (e.else_branch) inline_calls_in_expr(e.else_branch.value(), inlinable);
            } else if constexpr (std::is_same_v<T, HirMethodCallExpr>) {
                inline_calls_in_expr(e.receiver, inlinable);
                for (auto& arg : e.args) {
                    inline_calls_in_expr(arg, inlinable);
                }
            } else if constexpr (std::is_same_v<T, HirLoopExpr>) {
                inline_calls_in_expr(e.body, inlinable);
            } else if constexpr (std::is_same_v<T, HirWhileExpr>) {
                inline_calls_in_expr(e.condition, inlinable);
                inline_calls_in_expr(e.body, inlinable);
            } else if constexpr (std::is_same_v<T, HirForExpr>) {
                inline_calls_in_expr(e.iter, inlinable);
                inline_calls_in_expr(e.body, inlinable);
            } else if constexpr (std::is_same_v<T, HirWhenExpr>) {
                inline_calls_in_expr(e.scrutinee, inlinable);
                for (auto& arm : e.arms) {
                    if (arm.guard) inline_calls_in_expr(arm.guard.value(), inlinable);
                    inline_calls_in_expr(arm.body, inlinable);
                }
            } else if constexpr (std::is_same_v<T, HirClosureExpr>) {
                inline_calls_in_expr(e.body, inlinable);
            } else if constexpr (std::is_same_v<T, HirTryExpr>) {
                inline_calls_in_expr(e.expr, inlinable);
            } else if constexpr (std::is_same_v<T, HirAwaitExpr>) {
                inline_calls_in_expr(e.expr, inlinable);
            } else if constexpr (std::is_same_v<T, HirReturnExpr>) {
                if (e.value) inline_calls_in_expr(e.value.value(), inlinable);
            } else if constexpr (std::is_same_v<T, HirBreakExpr>) {
                if (e.value) inline_calls_in_expr(e.value.value(), inlinable);
            } else if constexpr (std::is_same_v<T, HirCastExpr>) {
                inline_calls_in_expr(e.expr, inlinable);
            } else if constexpr (std::is_same_v<T, HirFieldExpr>) {
                inline_calls_in_expr(e.object, inlinable);
            } else if constexpr (std::is_same_v<T, HirIndexExpr>) {
                inline_calls_in_expr(e.object, inlinable);
                inline_calls_in_expr(e.index, inlinable);
            } else if constexpr (std::is_same_v<T, HirTupleExpr>) {
                for (auto& elem : e.elements) {
                    inline_calls_in_expr(elem, inlinable);
                }
            } else if constexpr (std::is_same_v<T, HirArrayExpr>) {
                for (auto& elem : e.elements) {
                    inline_calls_in_expr(elem, inlinable);
                }
            } else if constexpr (std::is_same_v<T, HirArrayRepeatExpr>) {
                inline_calls_in_expr(e.value, inlinable);
            } else if constexpr (std::is_same_v<T, HirStructExpr>) {
                for (auto& [name, val] : e.fields) {
                    inline_calls_in_expr(val, inlinable);
                }
                if (e.base) inline_calls_in_expr(e.base.value(), inlinable);
            } else if constexpr (std::is_same_v<T, HirEnumExpr>) {
                for (auto& payload : e.payload) {
                    inline_calls_in_expr(payload, inlinable);
                }
            } else if constexpr (std::is_same_v<T, HirAssignExpr>) {
                inline_calls_in_expr(e.target, inlinable);
                inline_calls_in_expr(e.value, inlinable);
            } else if constexpr (std::is_same_v<T, HirCompoundAssignExpr>) {
                inline_calls_in_expr(e.target, inlinable);
                inline_calls_in_expr(e.value, inlinable);
            } else if constexpr (std::is_same_v<T, HirLowlevelExpr>) {
                for (auto& stmt : e.stmts) {
                    if (auto* expr_stmt = std::get_if<HirExprStmt>(&stmt->kind)) {
                        inline_calls_in_expr(expr_stmt->expr, inlinable);
                    } else if (auto* let_stmt = std::get_if<HirLetStmt>(&stmt->kind)) {
                        if (let_stmt->init) {
                            inline_calls_in_expr(let_stmt->init.value(), inlinable);
                        }
                    }
                }
                if (e.expr) inline_calls_in_expr(e.expr.value(), inlinable);
            }
            // HirLiteralExpr, HirVarExpr, HirContinueExpr - no subexpressions
        },
        expr->kind);
}

// ============================================================================
// Closure Optimization Implementation
// ============================================================================
//
// Optimizes closure captures to reduce overhead:
// 1. Remove unused captures - variables captured but never referenced
// 2. Convert ref-to-value - captures that don't escape can be copied
// 3. Identify trivial closures - closures with no captures can become fn ptrs
//
// Algorithm:
// 1. Walk all expressions looking for closures
// 2. For each closure, analyze which captures are used
// 3. Remove unused captures and optimize capture modes

auto ClosureOptimization::run(HirModule& module) -> bool {
    changed_ = false;

    for (auto& func : module.functions) {
        optimize_function(func);
    }

    return changed_;
}

auto ClosureOptimization::run_pass(HirModule& module) -> bool {
    ClosureOptimization pass;
    return pass.run(module);
}

void ClosureOptimization::optimize_function(HirFunction& func) {
    if (!func.body) return;
    optimize_in_expr(func.body.value());
}

void ClosureOptimization::optimize_in_expr(HirExprPtr& expr) {
    if (!expr) return;

    std::visit(
        [this](auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, HirClosureExpr>) {
                // Optimize this closure
                optimize_closure(e);
                // Also recurse into the body
                optimize_in_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                optimize_in_expr(e.left);
                optimize_in_expr(e.right);
            } else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                optimize_in_expr(e.operand);
            } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                for (auto& stmt : e.stmts) {
                    if (auto* expr_stmt = std::get_if<HirExprStmt>(&stmt->kind)) {
                        optimize_in_expr(expr_stmt->expr);
                    } else if (auto* let_stmt = std::get_if<HirLetStmt>(&stmt->kind)) {
                        if (let_stmt->init) {
                            optimize_in_expr(let_stmt->init.value());
                        }
                    }
                }
                if (e.expr) optimize_in_expr(e.expr.value());
            } else if constexpr (std::is_same_v<T, HirCallExpr>) {
                for (auto& arg : e.args) {
                    optimize_in_expr(arg);
                }
            } else if constexpr (std::is_same_v<T, HirMethodCallExpr>) {
                optimize_in_expr(e.receiver);
                for (auto& arg : e.args) {
                    optimize_in_expr(arg);
                }
            } else if constexpr (std::is_same_v<T, HirIfExpr>) {
                optimize_in_expr(e.condition);
                optimize_in_expr(e.then_branch);
                if (e.else_branch) optimize_in_expr(e.else_branch.value());
            } else if constexpr (std::is_same_v<T, HirLoopExpr>) {
                optimize_in_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirWhileExpr>) {
                optimize_in_expr(e.condition);
                optimize_in_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirForExpr>) {
                optimize_in_expr(e.iter);
                optimize_in_expr(e.body);
            } else if constexpr (std::is_same_v<T, HirWhenExpr>) {
                optimize_in_expr(e.scrutinee);
                for (auto& arm : e.arms) {
                    if (arm.guard) optimize_in_expr(arm.guard.value());
                    optimize_in_expr(arm.body);
                }
            } else if constexpr (std::is_same_v<T, HirTryExpr>) {
                optimize_in_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirAwaitExpr>) {
                optimize_in_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirReturnExpr>) {
                if (e.value) optimize_in_expr(e.value.value());
            } else if constexpr (std::is_same_v<T, HirBreakExpr>) {
                if (e.value) optimize_in_expr(e.value.value());
            } else if constexpr (std::is_same_v<T, HirCastExpr>) {
                optimize_in_expr(e.expr);
            } else if constexpr (std::is_same_v<T, HirFieldExpr>) {
                optimize_in_expr(e.object);
            } else if constexpr (std::is_same_v<T, HirIndexExpr>) {
                optimize_in_expr(e.object);
                optimize_in_expr(e.index);
            } else if constexpr (std::is_same_v<T, HirTupleExpr>) {
                for (auto& elem : e.elements) {
                    optimize_in_expr(elem);
                }
            } else if constexpr (std::is_same_v<T, HirArrayExpr>) {
                for (auto& elem : e.elements) {
                    optimize_in_expr(elem);
                }
            } else if constexpr (std::is_same_v<T, HirArrayRepeatExpr>) {
                optimize_in_expr(e.value);
            } else if constexpr (std::is_same_v<T, HirStructExpr>) {
                for (auto& [name, val] : e.fields) {
                    optimize_in_expr(val);
                }
                if (e.base) optimize_in_expr(e.base.value());
            } else if constexpr (std::is_same_v<T, HirEnumExpr>) {
                for (auto& payload : e.payload) {
                    optimize_in_expr(payload);
                }
            } else if constexpr (std::is_same_v<T, HirAssignExpr>) {
                optimize_in_expr(e.target);
                optimize_in_expr(e.value);
            } else if constexpr (std::is_same_v<T, HirCompoundAssignExpr>) {
                optimize_in_expr(e.target);
                optimize_in_expr(e.value);
            } else if constexpr (std::is_same_v<T, HirLowlevelExpr>) {
                for (auto& stmt : e.stmts) {
                    if (auto* expr_stmt = std::get_if<HirExprStmt>(&stmt->kind)) {
                        optimize_in_expr(expr_stmt->expr);
                    } else if (auto* let_stmt = std::get_if<HirLetStmt>(&stmt->kind)) {
                        if (let_stmt->init) {
                            optimize_in_expr(let_stmt->init.value());
                        }
                    }
                }
                if (e.expr) optimize_in_expr(e.expr.value());
            }
            // HirLiteralExpr, HirVarExpr, HirContinueExpr - no subexpressions
        },
        expr->kind);
}

void ClosureOptimization::optimize_closure(HirClosureExpr& closure) {
    // Check each capture and remove unused ones
    std::vector<HirCapture> optimized_captures;

    for (const auto& capture : closure.captures) {
        if (is_capture_used(closure, capture.name)) {
            HirCapture new_capture = capture;

            // If capture doesn't escape, we can potentially optimize it
            if (!capture_escapes(closure, capture.name)) {
                // For now, mark non-escaping captures as not mutable
                // (more aggressive optimization would convert ref to value)
                new_capture.is_mut = false;
            }

            optimized_captures.push_back(std::move(new_capture));
        } else {
            // Capture is unused - don't include it
            changed_ = true;
        }
    }

    if (optimized_captures.size() != closure.captures.size()) {
        closure.captures = std::move(optimized_captures);

        // Update the closure type to reflect removed captures
        if (closure.type && closure.type->is<types::ClosureType>()) {
            auto& closure_type = closure.type->as<types::ClosureType>();
            std::vector<types::CapturedVar> new_captured;
            for (const auto& cap : closure.captures) {
                new_captured.push_back({cap.name, cap.type, cap.is_mut});
            }
            // Create updated closure type
            closure.type = types::make_closure(closure_type.params, closure_type.return_type,
                                               std::move(new_captured));
        }
    }
}

auto ClosureOptimization::is_capture_used(const HirClosureExpr& closure, const std::string& name)
    -> bool {
    // Check if the captured variable is referenced in the closure body
    return check_var_usage(*closure.body, name);
}

auto ClosureOptimization::capture_escapes(const HirClosureExpr& closure, const std::string& name)
    -> bool {
    // Check if a reference to the captured variable could escape
    // (e.g., stored in a structure, passed to a function, returned)
    return check_var_escapes(*closure.body, name);
}

auto ClosureOptimization::check_var_usage(const HirExpr& expr, const std::string& name) -> bool {
    bool found = false;

    std::visit(
        [&found, &name, this](const auto& e) {
            if (found) return;
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, HirVarExpr>) {
                if (e.name == name) {
                    found = true;
                }
            } else if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                found = check_var_usage(*e.left, name) || check_var_usage(*e.right, name);
            } else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                found = check_var_usage(*e.operand, name);
            } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                for (const auto& stmt : e.stmts) {
                    if (auto* expr_stmt = std::get_if<HirExprStmt>(&stmt->kind)) {
                        if (check_var_usage(*expr_stmt->expr, name)) {
                            found = true;
                            return;
                        }
                    } else if (auto* let_stmt = std::get_if<HirLetStmt>(&stmt->kind)) {
                        if (let_stmt->init && check_var_usage(**let_stmt->init, name)) {
                            found = true;
                            return;
                        }
                    }
                }
                if (e.expr && check_var_usage(**e.expr, name)) {
                    found = true;
                }
            } else if constexpr (std::is_same_v<T, HirCallExpr>) {
                for (const auto& arg : e.args) {
                    if (check_var_usage(*arg, name)) {
                        found = true;
                        return;
                    }
                }
            } else if constexpr (std::is_same_v<T, HirMethodCallExpr>) {
                if (check_var_usage(*e.receiver, name)) {
                    found = true;
                    return;
                }
                for (const auto& arg : e.args) {
                    if (check_var_usage(*arg, name)) {
                        found = true;
                        return;
                    }
                }
            } else if constexpr (std::is_same_v<T, HirIfExpr>) {
                found = check_var_usage(*e.condition, name) || check_var_usage(*e.then_branch, name);
                if (!found && e.else_branch) {
                    found = check_var_usage(**e.else_branch, name);
                }
            } else if constexpr (std::is_same_v<T, HirFieldExpr>) {
                found = check_var_usage(*e.object, name);
            } else if constexpr (std::is_same_v<T, HirIndexExpr>) {
                found = check_var_usage(*e.object, name) || check_var_usage(*e.index, name);
            } else if constexpr (std::is_same_v<T, HirLoopExpr>) {
                found = check_var_usage(*e.body, name);
            } else if constexpr (std::is_same_v<T, HirWhileExpr>) {
                found =
                    check_var_usage(*e.condition, name) || check_var_usage(*e.body, name);
            } else if constexpr (std::is_same_v<T, HirForExpr>) {
                found = check_var_usage(*e.iter, name) || check_var_usage(*e.body, name);
            } else if constexpr (std::is_same_v<T, HirReturnExpr>) {
                if (e.value) found = check_var_usage(**e.value, name);
            } else if constexpr (std::is_same_v<T, HirClosureExpr>) {
                // Check if inner closure uses the variable
                found = check_var_usage(*e.body, name);
            }
        },
        expr.kind);

    return found;
}

auto ClosureOptimization::check_var_escapes(const HirExpr& expr, const std::string& name) -> bool {
    // Conservative escape analysis:
    // A variable escapes if:
    // 1. It's returned from the closure
    // 2. It's passed to a function call
    // 3. It's stored in a structure/tuple
    // 4. A reference to it is taken

    bool escapes = false;

    std::visit(
        [&escapes, &name, this](const auto& e) {
            if (escapes) return;
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, HirReturnExpr>) {
                // Variable returned = escapes
                if (e.value && check_var_usage(**e.value, name)) {
                    escapes = true;
                }
            } else if constexpr (std::is_same_v<T, HirCallExpr>) {
                // Variable passed to function = might escape
                for (const auto& arg : e.args) {
                    if (check_var_usage(*arg, name)) {
                        escapes = true;
                        return;
                    }
                }
            } else if constexpr (std::is_same_v<T, HirMethodCallExpr>) {
                // Variable passed as method argument = might escape
                for (const auto& arg : e.args) {
                    if (check_var_usage(*arg, name)) {
                        escapes = true;
                        return;
                    }
                }
                // Receiver usage doesn't escape (method is called on it)
            } else if constexpr (std::is_same_v<T, HirUnaryExpr>) {
                // Reference taken = escapes
                if (e.op == HirUnaryOp::Ref || e.op == HirUnaryOp::RefMut) {
                    if (check_var_usage(*e.operand, name)) {
                        escapes = true;
                        return;
                    }
                }
                escapes = check_var_escapes(*e.operand, name);
            } else if constexpr (std::is_same_v<T, HirStructExpr>) {
                // Stored in struct = escapes
                for (const auto& [field_name, val] : e.fields) {
                    if (check_var_usage(*val, name)) {
                        escapes = true;
                        return;
                    }
                }
            } else if constexpr (std::is_same_v<T, HirTupleExpr>) {
                // Stored in tuple = escapes
                for (const auto& elem : e.elements) {
                    if (check_var_usage(*elem, name)) {
                        escapes = true;
                        return;
                    }
                }
            } else if constexpr (std::is_same_v<T, HirArrayExpr>) {
                // Stored in array = escapes
                for (const auto& elem : e.elements) {
                    if (check_var_usage(*elem, name)) {
                        escapes = true;
                        return;
                    }
                }
            } else if constexpr (std::is_same_v<T, HirBinaryExpr>) {
                escapes =
                    check_var_escapes(*e.left, name) || check_var_escapes(*e.right, name);
            } else if constexpr (std::is_same_v<T, HirBlockExpr>) {
                for (const auto& stmt : e.stmts) {
                    if (auto* expr_stmt = std::get_if<HirExprStmt>(&stmt->kind)) {
                        if (check_var_escapes(*expr_stmt->expr, name)) {
                            escapes = true;
                            return;
                        }
                    } else if (auto* let_stmt = std::get_if<HirLetStmt>(&stmt->kind)) {
                        if (let_stmt->init && check_var_escapes(**let_stmt->init, name)) {
                            escapes = true;
                            return;
                        }
                    }
                }
                if (e.expr) {
                    escapes = check_var_escapes(**e.expr, name);
                }
            } else if constexpr (std::is_same_v<T, HirIfExpr>) {
                escapes = check_var_escapes(*e.condition, name) ||
                          check_var_escapes(*e.then_branch, name);
                if (!escapes && e.else_branch) {
                    escapes = check_var_escapes(**e.else_branch, name);
                }
            } else if constexpr (std::is_same_v<T, HirLoopExpr>) {
                escapes = check_var_escapes(*e.body, name);
            } else if constexpr (std::is_same_v<T, HirWhileExpr>) {
                escapes = check_var_escapes(*e.condition, name) ||
                          check_var_escapes(*e.body, name);
            } else if constexpr (std::is_same_v<T, HirForExpr>) {
                escapes = check_var_escapes(*e.iter, name) || check_var_escapes(*e.body, name);
            } else if constexpr (std::is_same_v<T, HirClosureExpr>) {
                // If inner closure captures this variable, it escapes
                for (const auto& cap : e.captures) {
                    if (cap.name == name) {
                        escapes = true;
                        return;
                    }
                }
            }
            // HirVarExpr, HirLiteralExpr, etc. - don't cause escapes
        },
        expr.kind);

    return escapes;
}

// ============================================================================
// Pass Manager Implementation
// ============================================================================
//
// Manages a pipeline of HIR passes. Passes are registered with add_pass<T>()
// and executed in order with run(). For aggressive optimization, use
// run_to_fixpoint() which repeats until no pass reports changes.

auto HirPassManager::run(HirModule& module) -> bool {
    bool changed = false;
    for (auto& pass : passes_) {
        if (pass->run(module)) {
            changed = true;
        }
    }
    return changed;
}

auto HirPassManager::run_to_fixpoint(HirModule& module, size_t max_iterations) -> size_t {
    size_t iterations = 0;
    while (iterations < max_iterations) {
        bool changed = run(module);
        ++iterations;
        if (!changed) break;
    }
    return iterations;
}

// ============================================================================
// Convenience Functions
// ============================================================================
//
// optimize_hir: Default optimization (constant folding + DCE + closures)
// optimize_hir_level: Level-based optimization matching CLI flags:
//   - O0: No optimization
//   - O1: Constant folding only
//   - O2: O1 + dead code elimination
//   - O3: O2 + closure optimization + inlining (runs to fixpoint)

auto optimize_hir(HirModule& module) -> bool {
    HirPassManager pm;
    pm.add_pass<ConstantFolding>();
    pm.add_pass<DeadCodeElimination>();
    pm.add_pass<ClosureOptimization>();
    return pm.run(module);
}

auto optimize_hir_level(HirModule& module, int level) -> bool {
    if (level <= 0) return false;

    HirPassManager pm;

    if (level >= 1) {
        pm.add_pass<ConstantFolding>();
    }

    if (level >= 2) {
        pm.add_pass<DeadCodeElimination>();
    }

    if (level >= 3) {
        pm.add_pass<ClosureOptimization>();
        pm.add_pass<Inlining>();
        return pm.run_to_fixpoint(module) > 1;
    }

    return pm.run(module);
}

} // namespace tml::hir
