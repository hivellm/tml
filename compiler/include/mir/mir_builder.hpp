//! # MIR Builder
//!
//! Converts type-checked AST to MIR in SSA form. The builder performs
//! lowering from the high-level AST to the MIR intermediate representation.
//!
//! ## Responsibilities
//!
//! - Converting expressions to SSA values
//! - Generating basic blocks for control flow
//! - Resolving variable references to SSA values
//! - Inserting phi nodes at control flow merge points
//! - Tracking drop scopes for RAII
//!
//! ## Usage
//!
//! ```cpp
//! MirBuilder builder(type_env);
//! mir::Module mir_module = builder.build(ast_module);
//! ```

#pragma once

#include "mir/mir.hpp"
#include "parser/ast.hpp"
#include "types/env.hpp"

#include <stack>
#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Build context for tracking state during MIR construction.
///
/// Maintains all mutable state needed while building a function's MIR,
/// including the current block, variable bindings, loop context, and
/// drop scopes for RAII.
struct BuildContext {
    Function* current_func = nullptr;                 ///< Function being built.
    uint32_t current_block = 0;                       ///< Current basic block ID.
    std::unordered_map<std::string, Value> variables; ///< Variable -> SSA value.
    std::unordered_set<std::string> volatile_vars;    ///< Set of volatile variable names.

    // Loop context for break/continue
    struct LoopContext {
        uint32_t header_block;            // Loop header (for continue)
        uint32_t exit_block;              // Loop exit (for break)
        std::optional<Value> break_value; // Value to use for break (if loop returns value)

        // Track break sources for exit PHI creation
        // Each entry: (source_block_id, variable_snapshot)
        std::vector<std::pair<uint32_t, std::unordered_map<std::string, Value>>> break_sources;
    };
    std::stack<LoopContext> loop_stack;

    // Async context
    bool in_async_func = false;      // Whether we're building an async function
    uint32_t next_suspension_id = 0; // Counter for suspension points

    // Drop scope tracking for RAII
    // Each scope tracks variables that need drop calls when exiting
    struct DropInfo {
        std::string var_name;    // Variable name
        Value value;             // SSA value to drop
        std::string type_name;   // Type name for drop call resolution
        MirTypePtr type;         // Full type for codegen
        bool is_moved = false;   // True if value was moved (don't drop)
        bool is_dropped = false; // True if drop was already emitted (for break paths)
    };
    std::vector<std::vector<DropInfo>> drop_scopes;

    // Push/pop drop scope
    void push_drop_scope() {
        drop_scopes.push_back({});
    }
    void pop_drop_scope() {
        if (!drop_scopes.empty()) {
            drop_scopes.pop_back();
        }
    }

    // Register a variable for drop when leaving scope
    void register_for_drop(const std::string& var_name, Value value, const std::string& type_name,
                           MirTypePtr type) {
        if (!drop_scopes.empty()) {
            drop_scopes.back().push_back({var_name, value, type_name, type, false});
        }
    }

    // Mark a variable as moved (won't be dropped)
    void mark_moved(const std::string& var_name) {
        for (auto& scope : drop_scopes) {
            for (auto& info : scope) {
                if (info.var_name == var_name) {
                    info.is_moved = true;
                    return;
                }
            }
        }
    }

    // Get all variables that need drop in current scope (in reverse order - LIFO)
    [[nodiscard]] auto get_drops_for_current_scope() const -> std::vector<DropInfo> {
        if (drop_scopes.empty()) {
            return {};
        }
        std::vector<DropInfo> drops;
        for (auto it = drop_scopes.back().rbegin(); it != drop_scopes.back().rend(); ++it) {
            if (!it->is_moved && !it->is_dropped) {
                drops.push_back(*it);
            }
        }
        return drops;
    }

    // Get all drops for all scopes (for return - drop everything)
    [[nodiscard]] auto get_all_drops() const -> std::vector<DropInfo> {
        std::vector<DropInfo> drops;
        // Iterate scopes from innermost to outermost
        for (auto scope_it = drop_scopes.rbegin(); scope_it != drop_scopes.rend(); ++scope_it) {
            // Within each scope, drop in reverse order
            for (auto it = scope_it->rbegin(); it != scope_it->rend(); ++it) {
                if (!it->is_moved && !it->is_dropped) {
                    drops.push_back(*it);
                }
            }
        }
        return drops;
    }

    // Mark drops as emitted (for break/continue paths to avoid double drops)
    void mark_scope_dropped() {
        if (!drop_scopes.empty()) {
            for (auto& info : drop_scopes.back()) {
                info.is_dropped = true;
            }
        }
    }

    // Mark all drops as emitted (for return paths)
    void mark_all_dropped() {
        for (auto& scope : drop_scopes) {
            for (auto& info : scope) {
                info.is_dropped = true;
            }
        }
    }
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
    auto build_await(const parser::AwaitExpr& await_expr) -> Value;

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

    // Emit instruction at function entry block (for allocas that need to dominate uses)
    auto emit_at_entry(Instruction inst, MirTypePtr type) -> Value;

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

    // Drop helpers for RAII
    void emit_drop_calls(const std::vector<BuildContext::DropInfo>& drops);
    void emit_drop_for_value(Value value, const MirTypePtr& type, const std::string& type_name);
    void emit_scope_drops(); // Emit drops for current scope
    void emit_all_drops();   // Emit drops for all scopes (for return)
    [[nodiscard]] auto get_type_name(const MirTypePtr& type) const -> std::string;
    [[nodiscard]] auto get_type_name_from_semantic(const types::TypePtr& type) const -> std::string;
};

} // namespace tml::mir
