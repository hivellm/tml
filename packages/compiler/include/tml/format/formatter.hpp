#pragma once

#include "tml/common.hpp"
#include "tml/parser/ast.hpp"
#include <string>
#include <sstream>

namespace tml::format {

// Formatter options
struct FormatOptions {
    int indent_width = 4;           // Spaces per indent level
    bool use_tabs = false;          // Use tabs instead of spaces
    int max_line_width = 100;       // Preferred max line width
    bool trailing_commas = true;    // Add trailing commas in lists
    bool space_after_colon = true;  // "x: T" vs "x:T"
    bool align_fields = false;      // Align struct field types
};

// TML source code formatter
// Takes an AST and produces formatted TML source code
class Formatter {
public:
    explicit Formatter(FormatOptions options = {});

    // Format a complete module
    auto format(const parser::Module& module) -> std::string;

private:
    FormatOptions options_;
    std::stringstream output_;
    int indent_level_ = 0;

    // Indentation helpers
    void emit(const std::string& text);
    void emit_line(const std::string& text);
    void emit_newline();
    void emit_indent();
    void push_indent();
    void pop_indent();
    auto indent_str() const -> std::string;

    // Declaration formatting
    void format_decl(const parser::Decl& decl);
    void format_func_decl(const parser::FuncDecl& func);
    void format_struct_decl(const parser::StructDecl& s);
    void format_enum_decl(const parser::EnumDecl& e);
    void format_trait_decl(const parser::TraitDecl& t);
    void format_impl_decl(const parser::ImplDecl& impl);
    void format_type_alias(const parser::TypeAliasDecl& alias);
    void format_const_decl(const parser::ConstDecl& c);
    void format_use_decl(const parser::UseDecl& u);
    void format_mod_decl(const parser::ModDecl& m);

    // Statement formatting
    void format_stmt(const parser::Stmt& stmt);
    void format_let_stmt(const parser::LetStmt& let);
    void format_var_stmt(const parser::VarStmt& var);
    void format_expr_stmt(const parser::ExprStmt& expr);

    // Expression formatting
    auto format_expr(const parser::Expr& expr) -> std::string;
    auto format_literal(const parser::LiteralExpr& lit) -> std::string;
    auto format_ident(const parser::IdentExpr& ident) -> std::string;
    auto format_binary(const parser::BinaryExpr& bin) -> std::string;
    auto format_unary(const parser::UnaryExpr& unary) -> std::string;
    auto format_call(const parser::CallExpr& call) -> std::string;
    auto format_method_call(const parser::MethodCallExpr& call) -> std::string;
    auto format_field(const parser::FieldExpr& field) -> std::string;
    auto format_index(const parser::IndexExpr& index) -> std::string;
    auto format_if(const parser::IfExpr& if_expr) -> std::string;
    auto format_block(const parser::BlockExpr& block, bool inline_single = false) -> std::string;
    auto format_loop(const parser::LoopExpr& loop) -> std::string;
    auto format_while(const parser::WhileExpr& while_expr) -> std::string;
    auto format_for(const parser::ForExpr& for_expr) -> std::string;
    auto format_when(const parser::WhenExpr& when) -> std::string;
    auto format_return(const parser::ReturnExpr& ret) -> std::string;
    auto format_break(const parser::BreakExpr& brk) -> std::string;
    auto format_continue(const parser::ContinueExpr& cont) -> std::string;
    auto format_struct_expr(const parser::StructExpr& s) -> std::string;
    auto format_tuple(const parser::TupleExpr& tuple) -> std::string;
    auto format_array(const parser::ArrayExpr& arr) -> std::string;
    auto format_closure(const parser::ClosureExpr& closure) -> std::string;
    auto format_range(const parser::RangeExpr& range) -> std::string;
    auto format_cast(const parser::CastExpr& cast) -> std::string;
    auto format_try(const parser::TryExpr& try_expr) -> std::string;
    auto format_await(const parser::AwaitExpr& await) -> std::string;
    auto format_path(const parser::PathExpr& path) -> std::string;

    // Type formatting
    auto format_type(const parser::Type& type) -> std::string;
    auto format_type_ptr(const parser::TypePtr& type) -> std::string;
    auto format_type_path(const parser::TypePath& path) -> std::string;

    // Pattern formatting
    auto format_pattern(const parser::Pattern& pattern) -> std::string;

    // Helper formatting
    void format_decorators(const std::vector<parser::Decorator>& decorators);
    void format_visibility(parser::Visibility vis);
    void format_generics(const std::vector<parser::GenericParam>& generics);
    void format_where_clause(const std::optional<parser::WhereClause>& where);
    auto format_func_params(const std::vector<parser::FuncParam>& params) -> std::string;

    // Operator helpers
    auto binary_op_str(parser::BinaryOp op) -> std::string;
    auto unary_op_str(parser::UnaryOp op) -> std::string;
    auto needs_parens(const parser::Expr& expr, const parser::BinaryExpr& parent, bool is_right) -> bool;
};

} // namespace tml::format
