// MIR Builder - Converts type-checked AST to MIR
//
// The builder performs lowering from the high-level AST to MIR in SSA form.
// This includes:
// - Converting expressions to SSA values
// - Generating basic blocks for control flow
// - Resolving variable references to SSA values
// - Inserting phi nodes at control flow merge points

#pragma once

#include "mir/mir.hpp"
#include "parser/ast.hpp"
#include "types/env.hpp"

#include <stack>
#include <unordered_map>

namespace tml::mir {

// Build context for tracking state during MIR construction
struct BuildContext {
    // Current function being built
    Function* current_func = nullptr;

    // Current basic block
    uint32_t current_block = 0;

    // Variable name to current SSA value mapping
    // This is updated when variables are assigned (for mutable vars)
    std::unordered_map<std::string, Value> variables;

    // Loop context for break/continue
    struct LoopContext {
        uint32_t header_block;            // Loop header (for continue)
        uint32_t exit_block;              // Loop exit (for break)
        std::optional<Value> break_value; // Value to use for break (if loop returns value)
    };
    std::stack<LoopContext> loop_stack;
};

class MirBuilder {
public:
    explicit MirBuilder(const types::TypeEnv& env);

    // Build MIR module from AST module
    auto build(const parser::Module& ast_module) -> Module;

private:
    const types::TypeEnv& env_;
    Module module_;
    BuildContext ctx_;

    // ============ Type Conversion ============
    auto convert_type(const parser::Type& type) -> MirTypePtr;
    auto convert_type(const types::TypePtr& type) -> MirTypePtr;
    auto convert_semantic_type(const types::TypePtr& type) -> MirTypePtr;

    // ============ Declaration Building ============
    void build_decl(const parser::Decl& decl);
    void build_func_decl(const parser::FuncDecl& func);
    void build_struct_decl(const parser::StructDecl& s);
    void build_enum_decl(const parser::EnumDecl& e);

    // ============ Statement Building ============
    void build_stmt(const parser::Stmt& stmt);
    void build_let_stmt(const parser::LetStmt& let);
    void build_var_stmt(const parser::VarStmt& var);
    void build_expr_stmt(const parser::ExprStmt& expr);

    // ============ Expression Building ============
    // Returns the SSA value representing the expression result
    auto build_expr(const parser::Expr& expr) -> Value;
    auto build_literal(const parser::LiteralExpr& lit) -> Value;
    auto build_ident(const parser::IdentExpr& ident) -> Value;
    auto build_binary(const parser::BinaryExpr& bin) -> Value;
    auto build_unary(const parser::UnaryExpr& unary) -> Value;
    auto build_call(const parser::CallExpr& call) -> Value;
    auto build_method_call(const parser::MethodCallExpr& call) -> Value;
    auto build_field(const parser::FieldExpr& field) -> Value;
    auto build_index(const parser::IndexExpr& index) -> Value;
    auto build_if(const parser::IfExpr& if_expr) -> Value;
    auto build_ternary(const parser::TernaryExpr& ternary) -> Value;
    auto build_block(const parser::BlockExpr& block) -> Value;
    auto build_loop(const parser::LoopExpr& loop) -> Value;
    auto build_while(const parser::WhileExpr& while_expr) -> Value;
    auto build_for(const parser::ForExpr& for_expr) -> Value;
    auto build_return(const parser::ReturnExpr& ret) -> Value;
    auto build_break(const parser::BreakExpr& brk) -> Value;
    auto build_continue(const parser::ContinueExpr& cont) -> Value;
    auto build_when(const parser::WhenExpr& when) -> Value;
    auto build_struct_expr(const parser::StructExpr& s) -> Value;
    auto build_tuple(const parser::TupleExpr& tuple) -> Value;
    auto build_array(const parser::ArrayExpr& arr) -> Value;
    auto build_path(const parser::PathExpr& path) -> Value;
    auto build_cast(const parser::CastExpr& cast) -> Value;
    auto build_closure(const parser::ClosureExpr& closure) -> Value;

    // ============ Pattern Building ============
    // Build pattern matching, returns the value bound (for simple patterns)
    // or handles complex destructuring
    void build_pattern_binding(const parser::Pattern& pattern, Value value);

    // ============ Helper Methods ============

    // Create a new basic block and return its ID
    auto create_block(const std::string& name = "") -> uint32_t;

    // Switch to a basic block (set as current)
    void switch_to_block(uint32_t block_id);

    // Check if current block is terminated
    auto is_terminated() const -> bool;

    // Emit instructions to current block
    auto emit(Instruction inst, MirTypePtr type) -> Value;
    void emit_void(Instruction inst);

    // Emit terminators
    void emit_return(std::optional<Value> value = std::nullopt);
    void emit_branch(uint32_t target);
    void emit_cond_branch(Value cond, uint32_t true_block, uint32_t false_block);
    void emit_unreachable();

    // Create constants
    auto const_int(int64_t value, int bit_width = 32, bool is_signed = true) -> Value;
    auto const_float(double value, bool is_f64 = false) -> Value;
    auto const_bool(bool value) -> Value;
    auto const_string(const std::string& value) -> Value;
    auto const_unit() -> Value;

    // Get or create variable
    auto get_variable(const std::string& name) -> Value;
    void set_variable(const std::string& name, Value value);

    // Binary operation helpers
    auto get_binop(parser::BinaryOp op) -> BinOp;
    auto is_comparison_op(parser::BinaryOp op) -> bool;

    // Unary operation helpers
    auto get_unaryop(parser::UnaryOp op) -> UnaryOp;
};

} // namespace tml::mir
