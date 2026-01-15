//! # HIR to MIR Builder
//!
//! Converts type-checked HIR to MIR in SSA form. The builder performs
//! lowering from the High-level IR to the Mid-level IR representation.
//!
//! ## Advantages over AST→MIR
//!
//! Since HIR is already type-resolved and desugared, this builder is simpler:
//! - Types are already fully resolved (no on-the-fly type inference)
//! - Generics are monomorphized (concrete types only)
//! - Syntax sugar is already expanded (var→let mut, for→loop, etc.)
//! - Closure captures are explicitly listed
//! - Field/variant indices are resolved
//!
//! ## Pipeline Position
//!
//! ```text
//! Source → Lexer → Parser → AST → TypeChecker → HIR → HirMirBuilder → MIR → Codegen
//!                                                      ^^^^^^^^^^^^^^
//!                                                      THIS MODULE
//! ```
//!
//! ## Usage
//!
//! ```cpp
//! #include "mir/hir_mir_builder.hpp"
//!
//! // After HIR building
//! hir::HirModule hir_module = /* from HirBuilder */;
//!
//! // Build MIR from HIR
//! mir::HirMirBuilder builder(type_env);
//! mir::Module mir_module = builder.build(hir_module);
//! ```
//!
//! ## See Also
//!
//! - `docs/specs/31-HIR.md` - HIR documentation
//! - `hir/hir_builder.hpp` - AST to HIR lowering
//! - `mir/mir_builder.hpp` - Legacy AST to MIR builder (for comparison)

#pragma once

#include "hir/hir.hpp"
#include "mir/mir.hpp"
#include "mir/mir_builder.hpp" // Reuse BuildContext
#include "types/env.hpp"

#include <stack>
#include <unordered_map>

namespace tml::mir {

/// Builds MIR from HIR.
///
/// This builder converts type-resolved HIR to SSA-form MIR. It is simpler than
/// the AST-based MirBuilder because HIR already has:
/// - Fully resolved types on every expression
/// - Monomorphized generics
/// - Desugared syntax (var→let mut, etc.)
/// - Resolved field/variant indices
/// - Explicit closure captures
class HirMirBuilder {
public:
    /// Constructs a builder with the type environment.
    ///
    /// The type environment is used for type conversion utilities but not
    /// for type inference (HIR types are already resolved).
    explicit HirMirBuilder(const types::TypeEnv& env);

    /// Builds a MIR module from an HIR module.
    ///
    /// This is the main entry point. It processes all declarations in the
    /// HIR module and produces the corresponding MIR.
    [[nodiscard]] auto build(const hir::HirModule& hir_module) -> Module;

private:
    const types::TypeEnv& env_;
    Module module_;
    BuildContext ctx_;

    // ============ Type Conversion ============

    /// Converts HIR type to MIR type.
    ///
    /// HIR uses semantic types (types::TypePtr) which need conversion
    /// to the simplified MIR type representation.
    [[nodiscard]] auto convert_type(const hir::HirType& type) -> MirTypePtr;

    // ============ Declaration Building ============

    /// Builds all declarations from an HIR module.
    void build_declarations(const hir::HirModule& hir_module);

    /// Builds a struct definition from HIR.
    void build_struct(const hir::HirStruct& s);

    /// Builds an enum definition from HIR.
    void build_enum(const hir::HirEnum& e);

    /// Builds a function definition from HIR.
    void build_function(const hir::HirFunction& func);

    /// Builds an impl block from HIR.
    void build_impl(const hir::HirImpl& impl);

    // ============ Statement Building ============

    /// Builds a statement, returns whether the statement terminated the block.
    [[nodiscard]] auto build_stmt(const hir::HirStmt& stmt) -> bool;

    /// Builds a let statement (variable binding).
    void build_let_stmt(const hir::HirLetStmt& let);

    /// Builds an expression statement.
    void build_expr_stmt(const hir::HirExprStmt& expr);

    // ============ Expression Building ============

    /// Builds an expression, returns the SSA value representing the result.
    [[nodiscard]] auto build_expr(const hir::HirExprPtr& expr) -> Value;

    /// Builds a literal expression.
    [[nodiscard]] auto build_literal(const hir::HirLiteralExpr& lit) -> Value;

    /// Builds a variable reference.
    [[nodiscard]] auto build_var(const hir::HirVarExpr& var) -> Value;

    /// Builds a binary expression.
    [[nodiscard]] auto build_binary(const hir::HirBinaryExpr& bin) -> Value;

    /// Builds a unary expression.
    [[nodiscard]] auto build_unary(const hir::HirUnaryExpr& unary) -> Value;

    /// Builds a function call.
    [[nodiscard]] auto build_call(const hir::HirCallExpr& call) -> Value;

    /// Builds a method call.
    [[nodiscard]] auto build_method_call(const hir::HirMethodCallExpr& call) -> Value;

    /// Builds a field access.
    [[nodiscard]] auto build_field(const hir::HirFieldExpr& field) -> Value;

    /// Builds an index expression.
    [[nodiscard]] auto build_index(const hir::HirIndexExpr& index) -> Value;

    /// Builds an if expression.
    [[nodiscard]] auto build_if(const hir::HirIfExpr& if_expr) -> Value;

    /// Builds a block expression.
    [[nodiscard]] auto build_block(const hir::HirBlockExpr& block) -> Value;

    /// Builds a loop expression.
    [[nodiscard]] auto build_loop(const hir::HirLoopExpr& loop) -> Value;

    /// Builds a while expression.
    [[nodiscard]] auto build_while(const hir::HirWhileExpr& while_expr) -> Value;

    /// Builds a for expression.
    [[nodiscard]] auto build_for(const hir::HirForExpr& for_expr) -> Value;

    /// Builds a return expression.
    [[nodiscard]] auto build_return(const hir::HirReturnExpr& ret) -> Value;

    /// Builds a break expression.
    [[nodiscard]] auto build_break(const hir::HirBreakExpr& brk) -> Value;

    /// Builds a continue expression.
    [[nodiscard]] auto build_continue(const hir::HirContinueExpr& cont) -> Value;

    /// Builds a when (pattern match) expression.
    [[nodiscard]] auto build_when(const hir::HirWhenExpr& when) -> Value;

    /// Builds a struct construction expression.
    [[nodiscard]] auto build_struct_expr(const hir::HirStructExpr& s) -> Value;

    /// Builds an enum variant construction expression.
    [[nodiscard]] auto build_enum_expr(const hir::HirEnumExpr& e) -> Value;

    /// Builds a tuple construction expression.
    [[nodiscard]] auto build_tuple(const hir::HirTupleExpr& tuple) -> Value;

    /// Builds an array literal expression.
    [[nodiscard]] auto build_array(const hir::HirArrayExpr& arr) -> Value;

    /// Builds an array repeat expression.
    [[nodiscard]] auto build_array_repeat(const hir::HirArrayRepeatExpr& arr) -> Value;

    /// Builds a cast expression.
    [[nodiscard]] auto build_cast(const hir::HirCastExpr& cast) -> Value;

    /// Builds a closure expression.
    [[nodiscard]] auto build_closure(const hir::HirClosureExpr& closure) -> Value;

    /// Builds a try (!) expression.
    [[nodiscard]] auto build_try(const hir::HirTryExpr& try_expr) -> Value;

    /// Builds an await expression.
    [[nodiscard]] auto build_await(const hir::HirAwaitExpr& await_expr) -> Value;

    /// Builds an assignment expression.
    [[nodiscard]] auto build_assign(const hir::HirAssignExpr& assign) -> Value;

    /// Builds a compound assignment expression.
    [[nodiscard]] auto build_compound_assign(const hir::HirCompoundAssignExpr& assign) -> Value;

    // ============ Pattern Building ============

    /// Builds pattern binding, binding matched values to variables.
    ///
    /// For simple patterns (binding, wildcard), this directly binds the value.
    /// For complex patterns (struct, tuple, enum), it performs destructuring.
    void build_pattern_binding(const hir::HirPatternPtr& pattern, Value value);

    /// Builds pattern matching condition for a when arm.
    ///
    /// Returns a boolean value indicating whether the pattern matches.
    [[nodiscard]] auto build_pattern_match(const hir::HirPatternPtr& pattern, Value scrutinee)
        -> Value;

    // ============ Helper Methods ============

    /// Creates a new basic block and returns its ID.
    [[nodiscard]] auto create_block(const std::string& name = "") -> uint32_t;

    /// Switches to a basic block (sets it as current).
    void switch_to_block(uint32_t block_id);

    /// Checks if the current block is terminated.
    [[nodiscard]] auto is_terminated() const -> bool;

    /// Emits an instruction to the current block, returning the result value.
    [[nodiscard]] auto emit(Instruction inst, MirTypePtr type, SourceSpan span = SourceSpan{})
        -> Value;

    /// Emits a void instruction (no result value).
    void emit_void(Instruction inst, SourceSpan span = SourceSpan{});

    /// Emits a return terminator.
    void emit_return(std::optional<Value> value = std::nullopt);

    /// Emits an unconditional branch terminator.
    void emit_branch(uint32_t target);

    /// Emits a conditional branch terminator.
    void emit_cond_branch(Value cond, uint32_t true_block, uint32_t false_block);

    /// Emits an unreachable terminator.
    void emit_unreachable();

    // Constant creation helpers
    [[nodiscard]] auto const_int(int64_t value, int bit_width = 32, bool is_signed = true) -> Value;
    [[nodiscard]] auto const_float(double value, bool is_f64 = false) -> Value;
    [[nodiscard]] auto const_bool(bool value) -> Value;
    [[nodiscard]] auto const_string(const std::string& value) -> Value;
    [[nodiscard]] auto const_unit() -> Value;

    // Variable management
    [[nodiscard]] auto get_variable(const std::string& name) -> Value;
    void set_variable(const std::string& name, Value value);

    // Binary operation conversion
    [[nodiscard]] auto convert_binop(hir::HirBinOp op) -> BinOp;
    [[nodiscard]] auto convert_compound_op(hir::HirCompoundOp op) -> BinOp;
    [[nodiscard]] auto is_comparison_op(hir::HirBinOp op) -> bool;

    // Unary operation conversion
    [[nodiscard]] auto convert_unaryop(hir::HirUnaryOp op) -> UnaryOp;

    // Drop helpers for RAII
    void emit_drop_calls(const std::vector<BuildContext::DropInfo>& drops);
    void emit_drop_for_value(Value value, const MirTypePtr& type, const std::string& type_name);
    void emit_scope_drops();
    void emit_all_drops();
    [[nodiscard]] auto get_type_name(const MirTypePtr& type) const -> std::string;
};

} // namespace tml::mir
