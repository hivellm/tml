#ifndef TML_TYPES_CHECKER_HPP
#define TML_TYPES_CHECKER_HPP

#include "common.hpp"
#include "parser/ast.hpp"
#include "types/env.hpp"
#include "types/type.hpp"

#include <vector>

namespace tml::types {

struct TypeError {
    std::string message;
    SourceSpan span;
    std::vector<std::string> notes;
};

class TypeChecker {
public:
    TypeChecker();

    [[nodiscard]] auto check_module(const parser::Module& module)
        -> Result<TypeEnv, std::vector<TypeError>>;

    [[nodiscard]] auto errors() const -> const std::vector<TypeError>& {
        return errors_;
    }
    [[nodiscard]] auto has_errors() const -> bool {
        return !errors_.empty();
    }

    // Module system integration
    void set_module_registry(std::shared_ptr<ModuleRegistry> registry) {
        env_.set_module_registry(std::move(registry));
    }

    void set_source_directory(const std::string& dir_path) {
        env_.set_source_directory(dir_path);
    }

private:
    TypeEnv env_;
    std::vector<TypeError> errors_;
    TypePtr current_return_type_ = nullptr;
    TypePtr current_self_type_ = nullptr; // For resolving 'This' in impl blocks
    std::unordered_map<std::string, TypePtr>
        current_associated_types_; // For resolving 'This::Owned', etc.
    std::unordered_map<std::string, TypePtr>
        current_type_params_; // Maps generic type param names to their types
    std::unordered_map<std::string, ConstGenericParam>
        current_const_params_; // Maps const generic param names to their definitions
    std::unordered_map<std::string, ConstValue>
        const_values_; // Maps const variable names to their evaluated values
    int loop_depth_ = 0;
    bool in_lowlevel_ = false;   // When true, & returns pointer instead of reference
    bool in_async_func_ = false; // When true, .await expressions are allowed

    // Declaration registration (first pass)
    void register_struct_decl(const parser::StructDecl& decl);
    void register_enum_decl(const parser::EnumDecl& decl);
    void register_trait_decl(const parser::TraitDecl& decl);
    void register_type_alias(const parser::TypeAliasDecl& decl);
    void process_use_decl(const parser::UseDecl& use_decl);

    // Function and declaration checking
    void check_func_decl(const parser::FuncDecl& func);
    void check_func_body(const parser::FuncDecl& func);
    void check_const_decl(const parser::ConstDecl& const_decl);
    void check_impl_decl(const parser::ImplDecl& impl);
    void check_impl_body(const parser::ImplDecl& impl);

    // Expression checking
    auto check_expr(const parser::Expr& expr) -> TypePtr;
    auto check_literal(const parser::LiteralExpr& lit) -> TypePtr;
    auto check_ident(const parser::IdentExpr& ident, SourceSpan span) -> TypePtr;
    auto check_binary(const parser::BinaryExpr& binary) -> TypePtr;
    auto check_unary(const parser::UnaryExpr& unary) -> TypePtr;
    auto check_call(const parser::CallExpr& call) -> TypePtr;
    auto check_method_call(const parser::MethodCallExpr& call) -> TypePtr;
    auto check_field_access(const parser::FieldExpr& field) -> TypePtr;
    auto check_index(const parser::IndexExpr& idx) -> TypePtr;
    auto check_block(const parser::BlockExpr& block) -> TypePtr;
    auto check_if(const parser::IfExpr& if_expr) -> TypePtr;
    auto check_ternary(const parser::TernaryExpr& ternary) -> TypePtr;
    auto check_if_let(const parser::IfLetExpr& if_let) -> TypePtr;
    auto check_when(const parser::WhenExpr& when) -> TypePtr;
    auto check_loop(const parser::LoopExpr& loop) -> TypePtr;
    auto check_for(const parser::ForExpr& for_expr) -> TypePtr;
    auto check_return(const parser::ReturnExpr& ret) -> TypePtr;
    auto check_break(const parser::BreakExpr& brk) -> TypePtr;
    auto check_tuple(const parser::TupleExpr& tuple) -> TypePtr;
    auto check_array(const parser::ArrayExpr& array) -> TypePtr;
    auto check_struct_expr(const parser::StructExpr& struct_expr) -> TypePtr;
    auto check_closure(const parser::ClosureExpr& closure) -> TypePtr;
    auto check_try(const parser::TryExpr& try_expr) -> TypePtr;
    auto check_path(const parser::PathExpr& path, SourceSpan span) -> TypePtr;
    auto check_range(const parser::RangeExpr& range) -> TypePtr;
    auto check_lowlevel(const parser::LowlevelExpr& lowlevel) -> TypePtr;
    auto check_interp_string(const parser::InterpolatedStringExpr& interp) -> TypePtr;
    auto check_cast(const parser::CastExpr& cast) -> TypePtr;
    auto check_await(const parser::AwaitExpr& await_expr, SourceSpan span) -> TypePtr;

    // Statement checking
    auto check_stmt(const parser::Stmt& stmt) -> TypePtr;
    auto check_let(const parser::LetStmt& let) -> TypePtr;
    auto check_var(const parser::VarStmt& var) -> TypePtr;

    // Pattern binding
    void bind_pattern(const parser::Pattern& pattern, TypePtr type);

    // Type resolution
    auto resolve_type(const parser::Type& type) -> TypePtr;
    auto resolve_type_path(const parser::TypePath& path) -> TypePtr;

    // Const generic evaluation
    // Evaluates a const expression at compile time and returns the value
    auto evaluate_const_expr(const parser::Expr& expr, TypePtr expected_type)
        -> std::optional<ConstValue>;
    // Extracts const generic parameters from parser GenericParam list
    auto extract_const_params(const std::vector<parser::GenericParam>& params)
        -> std::vector<ConstGenericParam>;

    // Closure capture analysis
    void collect_captures_from_expr(const parser::Expr& expr, std::shared_ptr<Scope> closure_scope,
                                    std::shared_ptr<Scope> parent_scope,
                                    std::vector<CapturedVar>& captures);

    // Return statement validation
    bool block_has_return(const parser::BlockExpr& block);
    bool stmt_has_return(const parser::Stmt& stmt);
    bool expr_has_return(const parser::Expr& expr);

    void error(const std::string& message, SourceSpan span);

    // Error message improvements
    auto find_similar_names(const std::string& name, const std::vector<std::string>& candidates,
                            size_t max_suggestions = 3) -> std::vector<std::string>;
    auto get_all_known_names() -> std::vector<std::string>;
    auto levenshtein_distance(const std::string& s1, const std::string& s2) -> size_t;
};

} // namespace tml::types

#endif // TML_TYPES_CHECKER_HPP
