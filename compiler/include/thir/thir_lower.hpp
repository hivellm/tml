//! # HIR -> THIR Lowering
//!
//! Transforms HIR to THIR by:
//! - Materializing implicit type coercions
//! - Resolving method calls via the trait solver
//! - Desugaring operator overloading to method calls
//! - Checking pattern exhaustiveness on `when` expressions
//! - Normalizing associated types

#pragma once

#include "hir/hir_module.hpp"
#include "thir/exhaustiveness.hpp"
#include "thir/thir.hpp"
#include "traits/solver.hpp"

namespace tml::thir {

/// Lowers HIR to THIR.
///
/// This is the main entry point for the THIR pass. It walks the HIR module,
/// transforming each expression and statement into its THIR equivalent while
/// inserting explicit coercion nodes, resolving method dispatch, and checking
/// pattern exhaustiveness.
class ThirLower {
public:
    ThirLower(const types::TypeEnv& env, traits::TraitSolver& solver);

    /// Lower an entire HIR module to THIR.
    auto lower_module(const hir::HirModule& hir) -> ThirModule;

    /// Get diagnostics emitted during lowering.
    auto diagnostics() const -> const std::vector<std::string>& {
        return diagnostics_;
    }

private:
    const types::TypeEnv* env_;
    traits::TraitSolver* solver_;
    traits::AssociatedTypeNormalizer normalizer_;
    ExhaustivenessChecker exhaustiveness_;
    hir::HirIdGenerator id_gen_;
    std::vector<std::string> diagnostics_;

    // --- Module-level lowering ---
    auto lower_function(const hir::HirFunction& func) -> ThirFunction;
    auto lower_struct(const hir::HirStruct& s) -> ThirStruct;
    auto lower_enum(const hir::HirEnum& e) -> ThirEnum;
    auto lower_behavior(const hir::HirBehavior& b) -> ThirBehavior;
    auto lower_impl(const hir::HirImpl& impl) -> ThirImpl;
    auto lower_const(const hir::HirConst& c) -> ThirConst;

    // --- Expression lowering ---
    auto lower_expr(const hir::HirExprPtr& expr) -> ThirExprPtr;
    auto lower_literal(const hir::HirLiteralExpr& lit) -> ThirExprPtr;
    auto lower_var(const hir::HirVarExpr& var) -> ThirExprPtr;
    auto lower_binary(const hir::HirBinaryExpr& bin) -> ThirExprPtr;
    auto lower_unary(const hir::HirUnaryExpr& un) -> ThirExprPtr;
    auto lower_call(const hir::HirCallExpr& call) -> ThirExprPtr;
    auto lower_method_call(const hir::HirMethodCallExpr& call) -> ThirExprPtr;
    auto lower_field(const hir::HirFieldExpr& field) -> ThirExprPtr;
    auto lower_index(const hir::HirIndexExpr& idx) -> ThirExprPtr;
    auto lower_tuple(const hir::HirTupleExpr& tuple) -> ThirExprPtr;
    auto lower_array(const hir::HirArrayExpr& arr) -> ThirExprPtr;
    auto lower_array_repeat(const hir::HirArrayRepeatExpr& arr) -> ThirExprPtr;
    auto lower_struct_expr(const hir::HirStructExpr& s) -> ThirExprPtr;
    auto lower_enum_expr(const hir::HirEnumExpr& e) -> ThirExprPtr;
    auto lower_block(const hir::HirBlockExpr& block) -> ThirExprPtr;
    auto lower_if(const hir::HirIfExpr& if_expr) -> ThirExprPtr;
    auto lower_when(const hir::HirWhenExpr& when) -> ThirExprPtr;
    auto lower_loop(const hir::HirLoopExpr& loop) -> ThirExprPtr;
    auto lower_while(const hir::HirWhileExpr& wh) -> ThirExprPtr;
    auto lower_for(const hir::HirForExpr& f) -> ThirExprPtr;
    auto lower_return(const hir::HirReturnExpr& ret) -> ThirExprPtr;
    auto lower_break(const hir::HirBreakExpr& brk) -> ThirExprPtr;
    auto lower_continue(const hir::HirContinueExpr& cont) -> ThirExprPtr;
    auto lower_closure(const hir::HirClosureExpr& clos) -> ThirExprPtr;
    auto lower_cast(const hir::HirCastExpr& cast) -> ThirExprPtr;
    auto lower_try(const hir::HirTryExpr& try_expr) -> ThirExprPtr;
    auto lower_await(const hir::HirAwaitExpr& await_expr) -> ThirExprPtr;
    auto lower_assign(const hir::HirAssignExpr& assign) -> ThirExprPtr;
    auto lower_compound_assign(const hir::HirCompoundAssignExpr& assign) -> ThirExprPtr;
    auto lower_lowlevel(const hir::HirLowlevelExpr& ll) -> ThirExprPtr;

    // --- Statement lowering ---
    auto lower_stmt(const hir::HirStmtPtr& stmt) -> ThirStmtPtr;

    // --- Pattern lowering ---
    auto lower_pattern(const hir::HirPatternPtr& pattern) -> ThirPatternPtr;

    // --- Coercion insertion ---

    /// Wrap `expr` in a coercion node if `expr->type != target`.
    auto coerce(ThirExprPtr expr, ThirType target) -> ThirExprPtr;

    /// Determine what coercion (if any) is needed from `from` to `to`.
    auto needs_coercion(ThirType from, ThirType to) -> std::optional<CoercionKind>;

    // --- Method resolution ---

    /// Resolve a method call via the trait solver.
    auto resolve_method(const hir::HirMethodCallExpr& call) -> ResolvedMethod;

    // --- Operator desugaring ---

    /// Map a binary operator to its corresponding behavior method name.
    auto op_behavior_method(hir::HirBinOp op) -> std::optional<std::pair<std::string, std::string>>;

    /// Map a compound operator to its corresponding behavior method name.
    auto compound_op_behavior_method(hir::HirCompoundOp op)
        -> std::optional<std::pair<std::string, std::string>>;

    // --- Helpers ---
    auto fresh_id() -> ThirId;
    auto is_primitive_numeric(ThirType type) -> bool;
    auto is_integer_type(ThirType type) -> bool;
    auto is_float_type(ThirType type) -> bool;
    auto primitive_kind(ThirType type) -> std::optional<types::PrimitiveKind>;
};

} // namespace tml::thir
