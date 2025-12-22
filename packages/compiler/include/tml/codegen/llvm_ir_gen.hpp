#pragma once

#include "tml/common.hpp"
#include "tml/parser/ast.hpp"
#include "tml/types/checker.hpp"
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace tml::codegen {

// LLVM IR generation error
struct LLVMGenError {
    std::string message;
    SourceSpan span;
    std::vector<std::string> notes;
};

// LLVM IR generator options
struct LLVMGenOptions {
    bool emit_comments = true;
    std::string target_triple = "x86_64-pc-windows-msvc";
};

// LLVM IR text generator
// Generates LLVM IR as text (.ll format)
class LLVMIRGen {
public:
    explicit LLVMIRGen(const types::TypeEnv& env, LLVMGenOptions options = {});

    // Generate LLVM IR for a module
    auto generate(const parser::Module& module) -> Result<std::string, std::vector<LLVMGenError>>;

private:
    const types::TypeEnv& env_;
    LLVMGenOptions options_;
    std::stringstream output_;
    int temp_counter_ = 0;
    int label_counter_ = 0;
    std::vector<LLVMGenError> errors_;

    // Current function context
    std::string current_func_;
    std::string current_ret_type_;  // Return type of current function
    std::string current_block_;
    bool block_terminated_ = false;

    // Current loop context for break/continue
    std::string current_loop_start_;
    std::string current_loop_end_;

    // Track last expression type for type-aware codegen
    std::string last_expr_type_ = "i32";

public:
    // Variable name to LLVM register/type mapping (public for is_bool_expr helper)
    struct VarInfo {
        std::string reg;
        std::string type;
    };

private:
    std::unordered_map<std::string, VarInfo> locals_;

    // Type mapping
    std::unordered_map<std::string, std::string> struct_types_;

    // Enum variant values (EnumName::VariantName -> tag value)
    std::unordered_map<std::string, int> enum_variants_;

    // Function registry for first-class functions (name -> LLVM function info)
    struct FuncInfo {
        std::string llvm_name;      // e.g., "@tml_double"
        std::string llvm_func_type; // e.g., "i32 (i32)"
        std::string ret_type;       // e.g., "i32"
    };
    std::unordered_map<std::string, FuncInfo> functions_;

    // Global constants (name -> value as string)
    std::unordered_map<std::string, std::string> global_constants_;

    // Helper methods
    auto fresh_reg() -> std::string;
    auto fresh_label(const std::string& prefix = "L") -> std::string;
    void emit(const std::string& code);
    void emit_line(const std::string& code);

    // Type translation
    auto llvm_type(const parser::Type& type) -> std::string;
    auto llvm_type_ptr(const parser::TypePtr& type) -> std::string;
    auto llvm_type_name(const std::string& name) -> std::string;
    auto llvm_type_from_semantic(const types::TypePtr& type) -> std::string;

    // Module structure
    void emit_header();
    void emit_runtime_decls();
    void emit_string_constants();

    // Declaration generation
    void gen_decl(const parser::Decl& decl);
    void gen_func_decl(const parser::FuncDecl& func);
    void gen_struct_decl(const parser::StructDecl& s);
    void gen_enum_decl(const parser::EnumDecl& e);

    // Statement generation
    void gen_stmt(const parser::Stmt& stmt);
    void gen_let_stmt(const parser::LetStmt& let);
    void gen_expr_stmt(const parser::ExprStmt& expr);

    // Expression generation - returns the register holding the value
    auto gen_expr(const parser::Expr& expr) -> std::string;
    auto gen_literal(const parser::LiteralExpr& lit) -> std::string;
    auto gen_ident(const parser::IdentExpr& ident) -> std::string;
    auto gen_binary(const parser::BinaryExpr& bin) -> std::string;
    auto gen_unary(const parser::UnaryExpr& unary) -> std::string;
    auto gen_call(const parser::CallExpr& call) -> std::string;
    auto gen_if(const parser::IfExpr& if_expr) -> std::string;
    auto gen_block(const parser::BlockExpr& block) -> std::string;
    auto gen_loop(const parser::LoopExpr& loop) -> std::string;
    auto gen_while(const parser::WhileExpr& while_expr) -> std::string;
    auto gen_for(const parser::ForExpr& for_expr) -> std::string;
    auto gen_return(const parser::ReturnExpr& ret) -> std::string;
    auto gen_when(const parser::WhenExpr& when) -> std::string;
    auto gen_struct_expr(const parser::StructExpr& s) -> std::string;
    auto gen_struct_expr_ptr(const parser::StructExpr& s) -> std::string;
    auto gen_field(const parser::FieldExpr& field) -> std::string;
    auto gen_array(const parser::ArrayExpr& arr) -> std::string;
    auto gen_index(const parser::IndexExpr& idx) -> std::string;
    auto gen_path(const parser::PathExpr& path) -> std::string;
    auto gen_method_call(const parser::MethodCallExpr& call) -> std::string;

    // Format string print
    auto gen_format_print(const std::string& format,
                          const std::vector<parser::ExprPtr>& args,
                          size_t start_idx,
                          bool with_newline) -> std::string;

    // Utility
    void report_error(const std::string& msg, const SourceSpan& span);

    // String literal handling
    std::vector<std::pair<std::string, std::string>> string_literals_;
    auto add_string_literal(const std::string& value) -> std::string;

public:
    // Print argument type inference (used by gen_call and gen_format_print)
    enum class PrintArgType { Int, I64, Float, Bool, Str, Unknown };
    static PrintArgType infer_print_type(const parser::Expr& expr);
};

} // namespace tml::codegen
