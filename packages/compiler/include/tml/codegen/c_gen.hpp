#pragma once

#include "tml/common.hpp"
#include "tml/parser/ast.hpp"
#include "tml/types/checker.hpp"

#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace tml::codegen {

// C code generation error
struct CodegenError {
    std::string message;
    SourceSpan span;
    std::vector<std::string> notes;
};

// C code generator options
struct CGenOptions {
    bool emit_comments = true;           // Include source location comments
    bool emit_debug_info = false;        // Emit debug printf statements
    std::string runtime_prefix = "tml_"; // Prefix for runtime functions
};

// C code generator
// Translates TML AST to C code
class CCodeGen {
public:
    explicit CCodeGen(const types::TypeEnv& env, CGenOptions options = {});

    // Generate C code for a module
    auto generate(const parser::Module& module) -> Result<std::string, std::vector<CodegenError>>;

private:
    const types::TypeEnv& env_;
    CGenOptions options_;
    std::stringstream output_;
    std::stringstream forward_decls_;
    std::stringstream type_defs_;
    std::stringstream func_defs_;
    int indent_level_ = 0;
    int temp_counter_ = 0;
    std::vector<CodegenError> errors_;

    // Current function context
    std::string current_func_;
    bool in_expression_context_ = false;

    // Main function return type (for entry point)
    std::string main_return_type_ = "void";

    // Type name mapping (TML type -> C type)
    std::unordered_map<std::string, std::string> type_map_;

    // Helper methods
    void emit(const std::string& code);
    void emit_line(const std::string& code);
    void emit_indent();
    void push_indent();
    void pop_indent();

    // Type translation
    auto translate_type(const parser::Type& type) -> std::string;
    auto translate_type_ptr(const parser::TypePtr& type) -> std::string;

    // Declaration generation
    void gen_decl(const parser::Decl& decl);
    void gen_func_decl(const parser::FuncDecl& func);
    void gen_struct_decl(const parser::StructDecl& s);
    void gen_enum_decl(const parser::EnumDecl& e);
    void gen_trait_decl(const parser::TraitDecl& t);
    void gen_impl_decl(const parser::ImplDecl& impl);
    void gen_type_alias(const parser::TypeAliasDecl& alias);

    // Statement generation
    void gen_stmt(const parser::Stmt& stmt);
    void gen_let_stmt(const parser::LetStmt& let);
    void gen_expr_stmt(const parser::ExprStmt& expr);

    // Expression generation
    auto gen_expr(const parser::Expr& expr) -> std::string;
    auto gen_literal(const parser::LiteralExpr& lit) -> std::string;
    auto gen_ident(const parser::IdentExpr& ident) -> std::string;
    auto gen_binary(const parser::BinaryExpr& bin) -> std::string;
    auto gen_unary(const parser::UnaryExpr& unary) -> std::string;
    auto gen_call(const parser::CallExpr& call) -> std::string;
    auto gen_method_call(const parser::MethodCallExpr& call) -> std::string;
    auto gen_field(const parser::FieldExpr& field) -> std::string;
    auto gen_index(const parser::IndexExpr& index) -> std::string;
    auto gen_if(const parser::IfExpr& if_expr) -> std::string;
    auto gen_block(const parser::BlockExpr& block) -> std::string;
    auto gen_loop(const parser::LoopExpr& loop) -> std::string;
    auto gen_for(const parser::ForExpr& for_expr) -> std::string;
    auto gen_when(const parser::WhenExpr& when) -> std::string;
    auto gen_return(const parser::ReturnExpr& ret) -> std::string;
    auto gen_struct_expr(const parser::StructExpr& s) -> std::string;
    auto gen_tuple(const parser::TupleExpr& tuple) -> std::string;
    auto gen_array(const parser::ArrayExpr& arr) -> std::string;
    auto gen_closure(const parser::ClosureExpr& closure) -> std::string;

    // Pattern generation (for let bindings, match arms)
    void gen_pattern_binding(const parser::Pattern& pattern, const std::string& value);

    // Utility
    auto fresh_temp() -> std::string;
    auto mangle_name(const std::string& name) -> std::string;
    void report_error(const std::string& msg, const SourceSpan& span);

    // C runtime header
    void emit_runtime_header();
    void emit_runtime_footer();
};

} // namespace tml::codegen
