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
    std::string current_block_;
    bool block_terminated_ = false;

    // Current loop context for break/continue
    std::string current_loop_start_;
    std::string current_loop_end_;

    // Variable name to LLVM register/type mapping
    struct VarInfo {
        std::string reg;
        std::string type;
    };
    std::unordered_map<std::string, VarInfo> locals_;

    // Type mapping
    std::unordered_map<std::string, std::string> struct_types_;

    // Helper methods
    auto fresh_reg() -> std::string;
    auto fresh_label(const std::string& prefix = "L") -> std::string;
    void emit(const std::string& code);
    void emit_line(const std::string& code);

    // Type translation
    auto llvm_type(const parser::Type& type) -> std::string;
    auto llvm_type_ptr(const parser::TypePtr& type) -> std::string;
    auto llvm_type_name(const std::string& name) -> std::string;

    // Module structure
    void emit_header();
    void emit_runtime_decls();
    void emit_string_constants();

    // Declaration generation
    void gen_decl(const parser::Decl& decl);
    void gen_func_decl(const parser::FuncDecl& func);
    void gen_struct_decl(const parser::StructDecl& s);

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
    auto gen_struct_expr(const parser::StructExpr& s) -> std::string;
    auto gen_struct_expr_ptr(const parser::StructExpr& s) -> std::string;
    auto gen_field(const parser::FieldExpr& field) -> std::string;

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
};

} // namespace tml::codegen
