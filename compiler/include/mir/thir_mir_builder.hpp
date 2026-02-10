//! # THIR to MIR Builder
//!
//! Converts THIR (with explicit coercions and resolved dispatch) to MIR.
//! Mirrors `HirMirBuilder` but operates on THIR input and handles
//! `ThirCoercionExpr` nodes.
//!
//! ## Advantages over HirMirBuilder
//!
//! - Coercions are already explicit (no implicit widening rules)
//! - Method dispatch is already resolved (no trait lookup needed)
//! - Operator overloading is already desugared (or marked with operator_method)
//! - Pattern exhaustiveness is already checked
//!
//! ## Pipeline Position
//!
//! ```text
//! HIR → ThirLower → THIR → ThirMirBuilder → MIR → Codegen
//!                           ^^^^^^^^^^^^^^^
//!                           THIS MODULE
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_builder.hpp" // Reuse BuildContext
#include "thir/thir.hpp"
#include "types/env.hpp"

#include <stack>
#include <unordered_map>

namespace tml::mir {

/// Builds MIR from THIR.
///
/// This builder converts THIR to SSA-form MIR. The key difference from
/// HirMirBuilder is that coercions are explicit `ThirCoercionExpr` nodes
/// which map directly to MIR cast/load/store instructions.
class ThirMirBuilder {
public:
    explicit ThirMirBuilder(const types::TypeEnv& env);

    /// Build a MIR module from a THIR module.
    [[nodiscard]] auto build(const thir::ThirModule& thir_module) -> Module;

private:
    const types::TypeEnv& env_;
    Module module_;
    BuildContext ctx_;

    // ============ Type Conversion ============

    [[nodiscard]] auto convert_type(const thir::ThirType& type) -> MirTypePtr;

    // ============ Declaration Building ============

    void build_declarations(const thir::ThirModule& thir_module);
    void build_struct(const thir::ThirStruct& s);
    void build_enum(const thir::ThirEnum& e);
    void build_function(const thir::ThirFunction& func);
    void build_impl(const thir::ThirImpl& impl);

    // ============ Statement Building ============

    [[nodiscard]] auto build_stmt(const thir::ThirStmt& stmt) -> bool;
    void build_let_stmt(const thir::ThirLetStmt& let);
    void build_expr_stmt(const thir::ThirExprStmt& expr);

    // ============ Expression Building ============

    [[nodiscard]] auto build_expr(const thir::ThirExprPtr& expr) -> Value;
    [[nodiscard]] auto build_literal(const thir::ThirLiteralExpr& lit) -> Value;
    [[nodiscard]] auto build_var(const thir::ThirVarExpr& var) -> Value;
    [[nodiscard]] auto build_binary(const thir::ThirBinaryExpr& bin) -> Value;
    [[nodiscard]] auto build_unary(const thir::ThirUnaryExpr& unary) -> Value;
    [[nodiscard]] auto build_call(const thir::ThirCallExpr& call) -> Value;
    [[nodiscard]] auto build_method_call(const thir::ThirMethodCallExpr& call) -> Value;
    [[nodiscard]] auto build_field(const thir::ThirFieldExpr& field) -> Value;
    [[nodiscard]] auto build_index(const thir::ThirIndexExpr& index) -> Value;
    [[nodiscard]] auto build_if(const thir::ThirIfExpr& if_expr) -> Value;
    [[nodiscard]] auto build_block(const thir::ThirBlockExpr& block) -> Value;
    [[nodiscard]] auto build_loop(const thir::ThirLoopExpr& loop) -> Value;
    [[nodiscard]] auto build_while(const thir::ThirWhileExpr& while_expr) -> Value;
    [[nodiscard]] auto build_for(const thir::ThirForExpr& for_expr) -> Value;
    [[nodiscard]] auto build_return(const thir::ThirReturnExpr& ret) -> Value;
    [[nodiscard]] auto build_break(const thir::ThirBreakExpr& brk) -> Value;
    [[nodiscard]] auto build_continue(const thir::ThirContinueExpr& cont) -> Value;
    [[nodiscard]] auto build_when(const thir::ThirWhenExpr& when) -> Value;
    [[nodiscard]] auto build_struct_expr(const thir::ThirStructExpr& s) -> Value;
    [[nodiscard]] auto build_enum_expr(const thir::ThirEnumExpr& e) -> Value;
    [[nodiscard]] auto build_tuple(const thir::ThirTupleExpr& tuple) -> Value;
    [[nodiscard]] auto build_array(const thir::ThirArrayExpr& arr) -> Value;
    [[nodiscard]] auto build_array_repeat(const thir::ThirArrayRepeatExpr& arr) -> Value;
    [[nodiscard]] auto build_cast(const thir::ThirCastExpr& cast) -> Value;
    [[nodiscard]] auto build_closure(const thir::ThirClosureExpr& closure) -> Value;
    [[nodiscard]] auto build_try(const thir::ThirTryExpr& try_expr) -> Value;
    [[nodiscard]] auto build_await(const thir::ThirAwaitExpr& await_expr) -> Value;
    [[nodiscard]] auto build_assign(const thir::ThirAssignExpr& assign) -> Value;
    [[nodiscard]] auto build_compound_assign(const thir::ThirCompoundAssignExpr& assign) -> Value;
    [[nodiscard]] auto build_lowlevel(const thir::ThirLowlevelExpr& lowlevel) -> Value;

    /// Build a THIR coercion expression.
    /// This is the key new method that doesn't exist in HirMirBuilder.
    [[nodiscard]] auto build_coercion(const thir::ThirCoercionExpr& coerce) -> Value;

    // ============ Pattern Building ============

    void build_pattern_binding(const thir::ThirPatternPtr& pattern, Value value);
    [[nodiscard]] auto build_pattern_match(const thir::ThirPatternPtr& pattern, Value scrutinee)
        -> Value;

    // ============ Helper Methods ============

    [[nodiscard]] auto create_block(const std::string& name = "") -> uint32_t;
    void switch_to_block(uint32_t block_id);
    [[nodiscard]] auto is_terminated() const -> bool;
    [[nodiscard]] auto emit(Instruction inst, MirTypePtr type, SourceSpan span = SourceSpan{})
        -> Value;
    void emit_void(Instruction inst, SourceSpan span = SourceSpan{});
    [[nodiscard]] auto emit_at_entry(Instruction inst, MirTypePtr type) -> Value;
    void emit_return(std::optional<Value> value = std::nullopt);
    void emit_branch(uint32_t target);
    void emit_cond_branch(Value cond, uint32_t true_block, uint32_t false_block);
    void emit_unreachable();

    [[nodiscard]] auto const_int(int64_t value, int bit_width = 32, bool is_signed = true) -> Value;
    [[nodiscard]] auto const_float(double value, bool is_f64 = false) -> Value;
    [[nodiscard]] auto const_bool(bool value) -> Value;
    [[nodiscard]] auto const_string(const std::string& value) -> Value;
    [[nodiscard]] auto const_unit() -> Value;

    [[nodiscard]] auto get_variable(const std::string& name) -> Value;
    void set_variable(const std::string& name, Value value);

    // Binary operation conversion (reuses HIR op enums since THIR uses the same)
    [[nodiscard]] auto convert_binop(hir::HirBinOp op) -> BinOp;
    [[nodiscard]] auto convert_compound_op(hir::HirCompoundOp op) -> BinOp;
    [[nodiscard]] auto is_comparison_op(hir::HirBinOp op) -> bool;
    [[nodiscard]] auto convert_unaryop(hir::HirUnaryOp op) -> UnaryOp;

    void emit_drop_calls(const std::vector<BuildContext::DropInfo>& drops);
    void emit_drop_for_value(Value value, const MirTypePtr& type, const std::string& type_name);
    void emit_scope_drops();
    void emit_all_drops();
    [[nodiscard]] auto get_type_name(const MirTypePtr& type) const -> std::string;
};

} // namespace tml::mir
