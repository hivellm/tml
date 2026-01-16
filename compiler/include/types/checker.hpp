//! # Type Checker
//!
//! This module implements semantic analysis and type checking for TML. The
//! type checker validates that programs are well-typed and resolves all type
//! information needed for code generation.
//!
//! ## Phases
//!
//! Type checking proceeds in multiple passes:
//!
//! 1. **Declaration registration**: Collect all type, function, and behavior definitions
//! 2. **Use declaration processing**: Resolve imports
//! 3. **Impl block registration**: Register behavior implementations
//! 4. **Body checking**: Type check function bodies and expressions
//!
//! ## Type Inference
//!
//! The type checker uses Hindley-Milner style inference with unification. Type
//! variables are created for unknown types and resolved as constraints accumulate.
//!
//! ## Error Recovery
//!
//! The checker continues after errors to report multiple issues in a single pass.
//! Errors include suggestions based on Levenshtein distance for typos.

#ifndef TML_TYPES_CHECKER_HPP
#define TML_TYPES_CHECKER_HPP

#include "common.hpp"
#include "parser/ast.hpp"
#include "types/env.hpp"
#include "types/type.hpp"

#include <vector>

namespace tml::types {

/// A type error with location and optional notes.
struct TypeError {
    std::string message;            ///< Error message.
    SourceSpan span;                ///< Error location.
    std::vector<std::string> notes; ///< Additional notes and suggestions.
};

/// Type checker for TML modules.
///
/// Performs semantic analysis including type inference, behavior checking,
/// and const evaluation. Reports all errors found in the module.
///
/// # Usage
///
/// ```cpp
/// TypeChecker checker;
/// checker.set_module_registry(registry);
/// auto result = checker.check_module(module);
/// if (result.is_err()) {
///     for (const auto& err : result.error()) {
///         report(err);
///     }
/// }
/// ```
class TypeChecker {
public:
    /// Constructs a type checker with default builtins.
    TypeChecker();

    /// Type checks a module, returning the populated type environment.
    [[nodiscard]] auto check_module(const parser::Module& module)
        -> Result<TypeEnv, std::vector<TypeError>>;

    /// Returns all accumulated errors.
    [[nodiscard]] auto errors() const -> const std::vector<TypeError>& {
        return errors_;
    }

    /// Returns true if any errors occurred.
    [[nodiscard]] auto has_errors() const -> bool {
        return !errors_.empty();
    }

    /// Sets the module registry for import resolution.
    void set_module_registry(std::shared_ptr<ModuleRegistry> registry) {
        env_.set_module_registry(std::move(registry));
    }

    /// Sets the source directory for local module resolution.
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
    std::vector<WhereConstraint> current_where_constraints_; // Current function's where clauses
    std::vector<std::string> current_namespace_; // Current namespace path for qualified names

    // Namespace support
    auto qualified_name(const std::string& name) const -> std::string;
    void register_namespace_decl(const parser::NamespaceDecl& decl);

    // Declaration registration (first pass)
    void register_struct_decl(const parser::StructDecl& decl);
    void register_enum_decl(const parser::EnumDecl& decl);
    void register_trait_decl(const parser::TraitDecl& decl);
    void register_type_alias(const parser::TypeAliasDecl& decl);
    void process_use_decl(const parser::UseDecl& use_decl);

    // OOP declaration registration (first pass)
    void register_interface_decl(const parser::InterfaceDecl& decl);
    void register_class_decl(const parser::ClassDecl& decl);

    // OOP declaration checking (second pass)
    void check_class_decl(const parser::ClassDecl& cls);
    void check_interface_decl(const parser::InterfaceDecl& iface);

    // OOP body checking (third pass)
    void check_class_body(const parser::ClassDecl& cls);

    // OOP validation helpers
    void validate_inheritance(const parser::ClassDecl& cls);
    void validate_override(const parser::ClassDecl& cls, const parser::ClassMethod& method);
    void validate_interface_impl(const parser::ClassDecl& cls);
    void validate_abstract_methods(const parser::ClassDecl& cls);
    void validate_value_class(const parser::ClassDecl& cls);
    void validate_pool_class(const parser::ClassDecl& cls);

    // Visibility checking helpers
    bool check_member_visibility(MemberVisibility vis, const std::string& defining_class,
                                 const std::string& member_name, SourceSpan span);
    bool is_subclass_of(const std::string& derived_class, const std::string& base_class);

    // Function and declaration checking
    void check_func_decl(const parser::FuncDecl& func);
    void check_func_body(const parser::FuncDecl& func);
    void check_const_decl(const parser::ConstDecl& const_decl);
    void check_impl_decl(const parser::ImplDecl& impl);
    void check_impl_body(const parser::ImplDecl& impl);

    // Expression checking
    auto check_expr(const parser::Expr& expr) -> TypePtr;
    auto check_expr(const parser::Expr& expr, TypePtr expected_type) -> TypePtr;
    auto check_literal(const parser::LiteralExpr& lit) -> TypePtr;
    auto check_literal(const parser::LiteralExpr& lit, TypePtr expected_type) -> TypePtr;
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
    auto check_array(const parser::ArrayExpr& array, TypePtr expected_type) -> TypePtr;
    auto check_struct_expr(const parser::StructExpr& struct_expr) -> TypePtr;
    auto check_closure(const parser::ClosureExpr& closure) -> TypePtr;
    auto check_try(const parser::TryExpr& try_expr) -> TypePtr;
    auto check_path(const parser::PathExpr& path, SourceSpan span) -> TypePtr;
    auto check_range(const parser::RangeExpr& range) -> TypePtr;
    auto check_lowlevel(const parser::LowlevelExpr& lowlevel) -> TypePtr;
    auto check_interp_string(const parser::InterpolatedStringExpr& interp) -> TypePtr;
    auto check_template_literal(const parser::TemplateLiteralExpr& tpl) -> TypePtr;
    auto check_cast(const parser::CastExpr& cast) -> TypePtr;
    auto check_is(const parser::IsExpr& is_expr) -> TypePtr;
    auto check_await(const parser::AwaitExpr& await_expr, SourceSpan span) -> TypePtr;
    auto check_base(const parser::BaseExpr& base) -> TypePtr;
    auto check_new(const parser::NewExpr& new_expr) -> TypePtr;

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
