//! # HIR Builder
//!
//! This module defines the `HirBuilder` class that lowers type-checked AST to HIR.
//! The builder is the bridge between the frontend (parsing, type checking) and
//! backend (code generation) of the TML compiler.
//!
//! ## Overview
//!
//! The HIR builder performs several key transformations:
//!
//! 1. **Type Resolution**: Every expression gets its fully-resolved type from
//!    the type environment built during type checking.
//!
//! 2. **Desugaring**: Syntactic sugar is expanded to core forms:
//!    - `var x = e` → `let mut x = e`
//!    - `x += 1` → compound assignment with resolved operator
//!    - `for` loops → iterator protocol calls
//!    - `if let` → `when` with two arms
//!
//! 3. **Monomorphization**: Generic types and functions are instantiated with
//!    concrete type arguments, creating separate copies for each usage.
//!
//! 4. **Index Resolution**: Field accesses and enum variants get their numeric
//!    indices resolved for efficient code generation.
//!
//! 5. **Capture Analysis**: For closures, the builder identifies which variables
//!    from enclosing scopes are captured.
//!
//! ## Usage
//!
//! ```cpp
//! #include "hir/hir_builder.hpp"
//!
//! // After type checking
//! types::TypeEnv& type_env = /* from type checker */;
//!
//! // Create builder and lower module
//! HirBuilder builder(type_env);
//! HirModule hir_module = builder.lower_module(ast_module);
//!
//! // Use the HIR for code generation
//! codegen(hir_module);
//! ```
//!
//! ## Monomorphization Cache
//!
//! The builder uses a `MonomorphizationCache` to track which generic instantiations
//! have been created. This ensures each unique instantiation is only emitted once.
//!
//! ## Error Handling
//!
//! The builder assumes the AST has passed type checking. If invalid AST is
//! provided, the behavior is undefined. Always run the type checker first.
//!
//! ## See Also
//!
//! - `docs/specs/31-HIR.md` - Complete HIR documentation
//! - `hir_module.hpp` - Output module type
//! - `types/checker.hpp` - Type checking that precedes HIR building

#pragma once

#include "hir/hir.hpp"
#include "parser/ast.hpp"
#include "types/checker.hpp"
#include "types/type.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::hir {

/// Tracks monomorphized instances of generic types and functions.
///
/// The cache prevents duplicate instantiations and provides consistent naming
/// for monomorphized items. When a generic is used with specific type arguments,
/// the cache either returns the existing mangled name or creates a new one.
///
/// ## Name Mangling Scheme
///
/// The mangling scheme uses double underscores to separate base name from type
/// arguments, and single underscores between type arguments:
///
/// | Generic Usage | Mangled Name |
/// |---------------|--------------|
/// | `Vec[I32]` | `Vec__I32` |
/// | `Map[Str, I32]` | `Map__Str_I32` |
/// | `Vec[Vec[I32]]` | `Vec__Vec__I32` |
///
/// ## Thread Safety
///
/// The cache is not thread-safe. Each compilation thread should have its own
/// builder instance with its own cache.
///
/// ## Example
///
/// ```cpp
/// MonomorphizationCache cache;
///
/// // First usage of Vec[I32]
/// auto name1 = cache.get_or_create_type("Vec", {i32_type});  // "Vec__I32"
///
/// // Second usage returns same name
/// auto name2 = cache.get_or_create_type("Vec", {i32_type});  // "Vec__I32"
/// assert(name1 == name2);
///
/// // Different type args get different name
/// auto name3 = cache.get_or_create_type("Vec", {str_type});  // "Vec__Str"
/// assert(name1 != name3);
/// ```
struct MonomorphizationCache {
    /// Maps mangling key to mangled name for types.
    /// Key format: "TypeName[Arg1,Arg2,...]"
    std::unordered_map<std::string, std::string> type_instances;

    /// Maps mangling key to mangled name for functions.
    /// Key format: "FuncName[Arg1,Arg2,...]"
    std::unordered_map<std::string, std::string> func_instances;

    /// Check if a monomorphized type instance exists.
    /// @param key The mangling key to check
    /// @return true if the instance has been created
    [[nodiscard]] auto has_type(const std::string& key) const -> bool;

    /// Check if a monomorphized function instance exists.
    /// @param key The mangling key to check
    /// @return true if the instance has been created
    [[nodiscard]] auto has_func(const std::string& key) const -> bool;

    /// Get or create a monomorphized type name.
    ///
    /// If the type instance already exists, returns its mangled name.
    /// Otherwise, creates a new mangled name and caches it.
    ///
    /// @param base_name The generic type name (e.g., "Vec")
    /// @param type_args The concrete type arguments
    /// @return The mangled name for this instantiation
    auto get_or_create_type(const std::string& base_name, const std::vector<HirType>& type_args)
        -> std::string;

    /// Get or create a monomorphized function name.
    ///
    /// If the function instance already exists, returns its mangled name.
    /// Otherwise, creates a new mangled name and caches it.
    ///
    /// @param base_name The generic function name
    /// @param type_args The concrete type arguments
    /// @return The mangled name for this instantiation
    auto get_or_create_func(const std::string& base_name, const std::vector<HirType>& type_args)
        -> std::string;

private:
    /// Generate a mangled name for a monomorphized instance.
    ///
    /// @param base The base name
    /// @param args The type arguments
    /// @return Mangled name like "Base__Arg1_Arg2"
    static auto mangle_name(const std::string& base, const std::vector<HirType>& args)
        -> std::string;
};

/// Builds HIR from type-checked AST.
///
/// The HirBuilder is the main class responsible for lowering AST to HIR.
/// It walks the AST, resolves types from the type environment, and constructs
/// the HIR representation.
///
/// ## Lifetime
///
/// The builder holds a reference to the type environment, which must outlive
/// the builder. Typically, both are created during a single compilation pass.
///
/// ## Usage Pattern
///
/// ```cpp
/// // 1. Parse source to AST
/// auto ast = parser.parse(source);
///
/// // 2. Type check and build environment
/// types::TypeChecker checker;
/// checker.check(ast);
/// auto& type_env = checker.env();
///
/// // 3. Lower to HIR
/// HirBuilder builder(type_env);
/// HirModule hir = builder.lower_module(ast);
///
/// // 4. Generate code from HIR
/// codegen(hir);
/// ```
///
/// ## Monomorphization
///
/// Generic items are monomorphized on-demand. When the builder encounters a
/// usage of a generic type or function, it:
/// 1. Records the instantiation in the monomorphization cache
/// 2. Queues the item for full lowering with those type arguments
/// 3. Processes queued items after the main module is lowered
class HirBuilder {
public:
    /// Construct a builder with the type environment from type checking.
    ///
    /// @param type_env Reference to the populated type environment
    explicit HirBuilder(types::TypeEnv& type_env);

    // ========================================================================
    // Top-Level Lowering
    // ========================================================================

    /// Lower a complete AST module to HIR.
    ///
    /// This is the main entry point. It processes all declarations in the
    /// module and returns a complete HirModule.
    ///
    /// @param ast_module The type-checked AST module
    /// @return The lowered HIR module
    auto lower_module(const parser::Module& ast_module) -> HirModule;

    /// Lower a single function declaration.
    /// @param func The function AST node
    /// @return The lowered HIR function
    auto lower_function(const parser::FuncDecl& func) -> HirFunction;

    /// Lower a struct declaration.
    /// @param struct_decl The struct AST node
    /// @return The lowered HIR struct
    auto lower_struct(const parser::StructDecl& struct_decl) -> HirStruct;

    /// Lower an enum declaration.
    /// @param enum_decl The enum AST node
    /// @return The lowered HIR enum
    auto lower_enum(const parser::EnumDecl& enum_decl) -> HirEnum;

    /// Lower an impl block.
    /// @param impl_decl The impl AST node
    /// @return The lowered HIR impl
    auto lower_impl(const parser::ImplDecl& impl_decl) -> HirImpl;

    /// Lower a behavior (trait) declaration.
    /// @param trait_decl The trait AST node
    /// @return The lowered HIR behavior
    auto lower_behavior(const parser::TraitDecl& trait_decl) -> HirBehavior;

    /// Lower a constant declaration.
    /// @param const_decl The const AST node
    /// @return The lowered HIR constant
    auto lower_const(const parser::ConstDecl& const_decl) -> HirConst;

    /// Lower a class to struct (for data layout).
    /// @param class_decl The class AST node
    /// @return The lowered HIR struct representing the class data
    auto lower_class_to_struct(const parser::ClassDecl& class_decl) -> HirStruct;

    /// Lower a class to impl block (for methods).
    /// @param class_decl The class AST node
    /// @return The lowered HIR impl with methods
    auto lower_class_to_impl(const parser::ClassDecl& class_decl) -> HirImpl;

private:
    // ========================================================================
    // Expression Lowering
    // ========================================================================

    /// Lower any expression.
    auto lower_expr(const parser::Expr& expr) -> HirExprPtr;

    /// Lower specific expression types.
    auto lower_literal(const parser::LiteralExpr& lit) -> HirExprPtr;
    auto lower_ident(const parser::IdentExpr& ident) -> HirExprPtr;
    auto lower_binary(const parser::BinaryExpr& binary) -> HirExprPtr;
    auto lower_unary(const parser::UnaryExpr& unary) -> HirExprPtr;
    auto lower_call(const parser::CallExpr& call) -> HirExprPtr;
    auto lower_method_call(const parser::MethodCallExpr& call) -> HirExprPtr;
    auto lower_field(const parser::FieldExpr& field) -> HirExprPtr;
    auto lower_index(const parser::IndexExpr& index) -> HirExprPtr;
    auto lower_tuple(const parser::TupleExpr& tuple) -> HirExprPtr;
    auto lower_array(const parser::ArrayExpr& array) -> HirExprPtr;
    auto lower_struct_expr(const parser::StructExpr& struct_expr) -> HirExprPtr;
    auto lower_if(const parser::IfExpr& if_expr) -> HirExprPtr;
    auto lower_ternary(const parser::TernaryExpr& ternary) -> HirExprPtr;
    auto lower_if_let(const parser::IfLetExpr& if_let) -> HirExprPtr;
    auto lower_when(const parser::WhenExpr& when) -> HirExprPtr;
    auto lower_loop(const parser::LoopExpr& loop) -> HirExprPtr;
    auto lower_while(const parser::WhileExpr& while_expr) -> HirExprPtr;
    auto lower_for(const parser::ForExpr& for_expr) -> HirExprPtr;
    auto lower_block(const parser::BlockExpr& block) -> HirExprPtr;
    auto lower_return(const parser::ReturnExpr& ret) -> HirExprPtr;
    auto lower_break(const parser::BreakExpr& brk) -> HirExprPtr;
    auto lower_continue(const parser::ContinueExpr& cont) -> HirExprPtr;
    auto lower_closure(const parser::ClosureExpr& closure) -> HirExprPtr;
    auto lower_range(const parser::RangeExpr& range) -> HirExprPtr;
    auto lower_cast(const parser::CastExpr& cast) -> HirExprPtr;
    auto lower_try(const parser::TryExpr& try_expr) -> HirExprPtr;
    auto lower_await(const parser::AwaitExpr& await) -> HirExprPtr;
    auto lower_path(const parser::PathExpr& path) -> HirExprPtr;
    auto lower_lowlevel(const parser::LowlevelExpr& lowlevel) -> HirExprPtr;

    // ========================================================================
    // Statement Lowering
    // ========================================================================

    /// Lower any statement.
    auto lower_stmt(const parser::Stmt& stmt) -> HirStmtPtr;

    /// Lower specific statement types.
    auto lower_let(const parser::LetStmt& let_stmt) -> HirStmtPtr;
    auto lower_var(const parser::VarStmt& var_stmt) -> HirStmtPtr;
    auto lower_expr_stmt(const parser::ExprStmt& expr_stmt) -> HirStmtPtr;

    // ========================================================================
    // Pattern Lowering
    // ========================================================================

    /// Lower a pattern with its expected type.
    ///
    /// The expected type is needed to resolve pattern types, especially for
    /// enum patterns where the variant type must match the scrutinee type.
    ///
    /// @param pattern The AST pattern
    /// @param expected_type The type being matched against
    /// @return The lowered HIR pattern
    auto lower_pattern(const parser::Pattern& pattern, HirType expected_type) -> HirPatternPtr;

    /// Lower specific pattern types.
    auto lower_wildcard_pattern(const parser::WildcardPattern& pattern) -> HirPatternPtr;
    auto lower_ident_pattern(const parser::IdentPattern& pattern, HirType expected_type)
        -> HirPatternPtr;
    auto lower_literal_pattern(const parser::LiteralPattern& pattern, HirType expected_type)
        -> HirPatternPtr;
    auto lower_tuple_pattern(const parser::TuplePattern& pattern, HirType expected_type)
        -> HirPatternPtr;
    auto lower_struct_pattern(const parser::StructPattern& pattern, HirType expected_type)
        -> HirPatternPtr;
    auto lower_enum_pattern(const parser::EnumPattern& pattern, HirType expected_type)
        -> HirPatternPtr;
    auto lower_or_pattern(const parser::OrPattern& pattern, HirType expected_type) -> HirPatternPtr;
    auto lower_array_pattern(const parser::ArrayPattern& pattern, HirType expected_type)
        -> HirPatternPtr;

    // ========================================================================
    // Type Resolution
    // ========================================================================

    /// Resolve an AST type to a semantic type.
    ///
    /// Looks up the type in the type environment and returns the fully
    /// resolved semantic type.
    ///
    /// @param type The AST type to resolve
    /// @return The resolved semantic type
    auto resolve_type(const parser::Type& type) -> HirType;

    /// Get the type of an expression from the type checker.
    ///
    /// @param expr The expression to get the type for
    /// @return The resolved type of the expression
    auto get_expr_type(const parser::Expr& expr) -> HirType;

    /// Get the return type of the current function.
    ///
    /// Used when lowering return expressions to verify/infer types.
    ///
    /// @return The current function's return type
    [[nodiscard]] auto current_return_type() const -> HirType;

    // ========================================================================
    // Closure Analysis
    // ========================================================================

    /// Collect captured variables from a closure.
    ///
    /// Analyzes the closure body to find all references to variables from
    /// enclosing scopes, determining what needs to be captured.
    ///
    /// @param closure The closure expression to analyze
    /// @return List of captures with their types and capture modes
    auto collect_captures(const parser::ClosureExpr& closure) -> std::vector<HirCapture>;

    /// Check if a name refers to a captured variable.
    ///
    /// @param name The variable name to check
    /// @return true if the name is a captured variable in the current closure
    [[nodiscard]] auto is_captured(const std::string& name) const -> bool;

    // ========================================================================
    // Monomorphization
    // ========================================================================

    /// Request monomorphization of a generic function.
    ///
    /// Adds a request to the pending queue. Requests are processed after
    /// the main module lowering completes.
    ///
    /// @param func_name The generic function name
    /// @param type_args The concrete type arguments
    void request_monomorphization(const std::string& func_name,
                                  const std::vector<HirType>& type_args);

    /// Process pending monomorphization requests.
    ///
    /// Called after main module lowering to emit all requested generic
    /// instantiations.
    void process_monomorphizations();

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /// Generate a fresh HIR ID.
    ///
    /// Each call returns a unique ID within this builder's session.
    ///
    /// @return A fresh, unique HirId
    auto fresh_id() -> HirId;

    /// Convert AST binary operator to HIR binary operator.
    /// @param op The AST operator
    /// @return The corresponding HIR operator
    static auto convert_binary_op(parser::BinaryOp op) -> HirBinOp;

    /// Convert AST unary operator to HIR unary operator.
    /// @param op The AST operator
    /// @return The corresponding HIR operator
    static auto convert_unary_op(parser::UnaryOp op) -> HirUnaryOp;

    /// Get the field index for a struct field.
    ///
    /// @param struct_name The struct type name
    /// @param field_name The field name
    /// @return Zero-based field index
    auto get_field_index(const std::string& struct_name, const std::string& field_name) -> int;

    /// Get the variant index for an enum variant.
    ///
    /// @param enum_name The enum type name
    /// @param variant_name The variant name
    /// @return Zero-based variant index
    auto get_variant_index(const std::string& enum_name, const std::string& variant_name) -> int;

    // ========================================================================
    // State
    // ========================================================================

    /// Reference to the type environment from type checking.
    types::TypeEnv& type_env_;

    /// Generator for unique HIR IDs.
    HirIdGenerator id_gen_;

    /// Cache for monomorphized type and function names.
    MonomorphizationCache mono_cache_;

    /// Name of the function currently being lowered.
    std::string current_func_name_;

    /// Return type of the function currently being lowered.
    HirType current_return_type_;

    /// Stack of variable scopes for closure capture analysis.
    /// Each scope is a set of variable names defined at that level.
    std::vector<std::unordered_set<std::string>> scopes_;

    /// Pending monomorphization requests.
    struct MonoRequest {
        std::string func_name;
        std::vector<HirType> type_args;
    };
    std::vector<MonoRequest> mono_requests_;

    /// The module currently being built (for adding monomorphized items).
    HirModule* current_module_ = nullptr;

    /// Current impl self type for resolving 'This'/'Self' types.
    /// Set when lowering impl methods, cleared after.
    std::optional<HirType> current_impl_self_type_ = std::nullopt;
};

} // namespace tml::hir
