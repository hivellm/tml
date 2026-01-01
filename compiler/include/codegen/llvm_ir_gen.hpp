#pragma once

#include "common.hpp"
#include "parser/ast.hpp"
#include "types/checker.hpp"

#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    bool coverage_enabled = false;    // Inject coverage instrumentation
    bool dll_export = false;          // Add dllexport for public functions (Windows DLL)
    bool emit_debug_info = false;     // Generate DWARF debug information
    bool generate_dll_entry = false;  // Generate tml_test_entry for DLL loading (no main)
    bool generate_fuzz_entry = false; // Generate tml_fuzz_target for fuzz testing (no main)
    int debug_level = 2;              // Debug level: 1=minimal, 2=standard, 3=full
    std::string target_triple = "x86_64-pc-windows-msvc";
    std::string source_file;          // Source file path for coverage/debug tracking
    std::string coverage_output_file; // Path for HTML coverage output (empty = print only)
};

// LLVM IR text generator
// Generates LLVM IR as text (.ll format)
class LLVMIRGen {
public:
    explicit LLVMIRGen(const types::TypeEnv& env, LLVMGenOptions options = {});

    // Generate LLVM IR for a module
    auto generate(const parser::Module& module) -> Result<std::string, std::vector<LLVMGenError>>;

    // Get external libraries to link (from @link decorators)
    auto get_link_libs() const -> const std::set<std::string>& {
        return extern_link_libs_;
    }

private:
    const types::TypeEnv& env_;
    LLVMGenOptions options_;
    std::stringstream output_;
    std::stringstream
        type_defs_buffer_; // Buffer for generic type definitions (emitted before functions)
    int temp_counter_ = 0;
    int label_counter_ = 0;
    std::vector<LLVMGenError> errors_;

    // Current function context
    std::string current_func_;
    std::string current_ret_type_;        // Return type of current function
    bool current_func_is_async_ = false;  // Whether current function is async
    std::string current_poll_type_;       // Poll[T] type for async functions
    std::string current_poll_inner_type_; // Inner T type for Poll[T] in async functions
    std::string current_block_;
    bool block_terminated_ = false;

    // Current impl self type (for resolving 'this' in impl methods)
    std::string current_impl_type_; // e.g., "Counter" when in impl Describable for Counter

    // Current associated type bindings (for resolving This::Item in impl blocks)
    // Maps associated type names to their concrete types (e.g., "Item" -> I32)
    std::unordered_map<std::string, types::TypePtr> current_associated_types_;

    // Current module prefix (for generating imported module functions)
    std::string
        current_module_prefix_; // e.g., "algorithms" when generating functions from algorithms.tml

    // Current loop context for break/continue
    std::string current_loop_start_;
    std::string current_loop_end_;

    // Track last expression type for type-aware codegen
    std::string last_expr_type_ = "i32";

    // Expected type context for enum constructors (used in gen_call_expr)
    // When set, enum constructors will use this type instead of inferring
    std::string expected_enum_type_; // e.g., "%struct.Outcome__I32__I32"

public:
    // Closure capture info for closures with captured variables
    struct ClosureCaptureInfo {
        std::vector<std::string> captured_names; // Names of captured variables
        std::vector<std::string> captured_types; // LLVM types of captured variables
    };

    // Variable name to LLVM register/type mapping (public for is_bool_expr helper)
    struct VarInfo {
        std::string reg;
        std::string type;
        types::TypePtr semantic_type; // Full semantic type for complex types like Ptr[T]
        std::optional<ClosureCaptureInfo>
            closure_captures; // Present if this is a closure with captures
    };

private:
    std::unordered_map<std::string, VarInfo> locals_;

    // Type mapping
    std::unordered_map<std::string, std::string> struct_types_;

    // Enum variant values (EnumName::VariantName -> tag value)
    std::unordered_map<std::string, int> enum_variants_;

    // Struct field info for dynamic field access
    struct FieldInfo {
        std::string name;
        int index;
        std::string llvm_type;
    };
    std::unordered_map<std::string, std::vector<FieldInfo>> struct_fields_; // struct_name -> fields

    // Function registry for first-class functions (name -> LLVM function info)
    struct FuncInfo {
        std::string llvm_name;                // e.g., "@tml_double"
        std::string llvm_func_type;           // e.g., "i32 (i32)"
        std::string ret_type;                 // e.g., "i32"
        std::vector<std::string> param_types; // e.g., {"i32", "%struct.Layout"}
    };
    std::unordered_map<std::string, FuncInfo> functions_;

    // Global constants (name -> value as string)
    std::unordered_map<std::string, std::string> global_constants_;

    // FFI support - external libraries to link (from @link decorator)
    std::set<std::string> extern_link_libs_;

    // Closure support
    std::vector<std::string> module_functions_; // Generated closure functions
    uint32_t closure_counter_ = 0;              // For unique closure names
    std::optional<ClosureCaptureInfo>
        last_closure_captures_; // Capture info from last gen_closure call

    // ============ Vtable Support for Trait Objects ============
    // Tracks behavior implementations and generates vtables for dyn dispatch

    // Vtable info: maps (type_name, behavior_name) -> vtable global name
    std::unordered_map<std::string, std::string>
        vtables_; // "Type::Behavior" -> "@vtable.Type.Behavior"

    // Behavior method order: behavior_name -> [method_names in order]
    std::unordered_map<std::string, std::vector<std::string>> behavior_method_order_;

    // Pending impl blocks to process
    std::vector<const parser::ImplDecl*> pending_impls_;

    // Behavior/trait declarations (for default implementations)
    std::unordered_map<std::string, const parser::TraitDecl*> trait_decls_;

    // Dyn type definitions (emitted once per behavior)
    std::set<std::string> emitted_dyn_types_;

    // Register an impl block for vtable generation
    void register_impl(const parser::ImplDecl* impl);

    // Generate all vtables from registered impls
    void emit_vtables();

    // Emit dyn type definition if not already emitted
    void emit_dyn_type(const std::string& behavior_name);

    // Get vtable global name for a type/behavior pair
    auto get_vtable(const std::string& type_name, const std::string& behavior_name) -> std::string;

    // ============ Generic Instantiation Support ============
    // Tracks generic type/function instantiations to avoid duplicates
    // and generate specialized code for each unique type argument combination

    struct GenericInstantiation {
        std::string base_name;                 // Original name (e.g., "Pair")
        std::vector<types::TypePtr> type_args; // Type arguments (e.g., [I32, Str])
        std::string mangled_name;              // Mangled name (e.g., "Pair__I32__Str")
        bool generated = false;                // Has code been generated?
    };

    // Cache of struct/enum instantiations (mangled_name -> info)
    std::unordered_map<std::string, GenericInstantiation> struct_instantiations_;
    std::unordered_map<std::string, GenericInstantiation> enum_instantiations_;
    std::unordered_map<std::string, GenericInstantiation> func_instantiations_;

    // Pending generic declarations (base_name -> AST node pointer)
    // These are registered but not generated until instantiated
    std::unordered_map<std::string, const parser::StructDecl*> pending_generic_structs_;
    std::unordered_map<std::string, const parser::EnumDecl*> pending_generic_enums_;
    std::unordered_map<std::string, const parser::FuncDecl*> pending_generic_funcs_;

    // Pending generic impl blocks (type_name -> impl block pointer)
    // These are registered and methods are instantiated when called on concrete types
    std::unordered_map<std::string, const parser::ImplDecl*> pending_generic_impls_;

    // Generated impl method instantiations (mangled_name -> true)
    // Tracks which specialized methods have already been generated
    std::unordered_set<std::string> generated_impl_methods_;

    // Pending impl method instantiation requests
    // Each entry: (mangled_type_name, method_name, type_subs, base_type_name)
    struct PendingImplMethod {
        std::string mangled_type_name;
        std::string method_name;
        std::unordered_map<std::string, types::TypePtr> type_subs;
        std::string base_type_name; // Used to find the impl block
    };
    std::vector<PendingImplMethod> pending_impl_method_instantiations_;

    // Function return types (func_name -> semantic return type)
    // Used by infer_expr_type to determine return types of function calls
    std::unordered_map<std::string, types::TypePtr> func_return_types_;

    // Storage for imported module ASTs (keeps AST alive so pointers in pending_generic_* remain
    // valid)
    std::vector<parser::Module> imported_module_asts_;

    // Storage for builtin generic enum declarations (keeps AST alive)
    std::vector<std::unique_ptr<parser::EnumDecl>> builtin_enum_decls_;

    // ============ Debug Info Support ============
    // LLVM debug metadata for DWARF generation
    int debug_metadata_counter_ = 0;          // Counter for unique metadata IDs
    int current_scope_id_ = 0;                // Current debug scope (function)
    int current_debug_loc_id_ = 0;            // Current debug location ID for instructions
    int file_id_ = 0;                         // File metadata ID
    int compile_unit_id_ = 0;                 // Compile unit metadata ID
    std::vector<std::string> debug_metadata_; // Pending debug metadata to emit at end
    std::unordered_map<std::string, int> func_debug_scope_; // function name -> scope ID
    std::unordered_map<std::string, int> var_debug_info_;   // var name -> debug info ID
    std::unordered_map<std::string, int> type_debug_info_;  // type name -> debug info ID

    // Debug info generation helpers
    int fresh_debug_id();
    void emit_debug_info_header();
    void emit_debug_info_footer();
    int create_function_debug_scope(const std::string& func_name, uint32_t line, uint32_t column);
    std::string get_debug_location(uint32_t line, uint32_t column);
    std::string get_debug_loc_suffix(); // Returns ", !dbg !N" if in debug scope, else ""
    int create_debug_location(uint32_t line,
                              uint32_t column); // Create and register a debug location

    // Variable debug info
    int create_local_variable_debug_info(const std::string& var_name, const std::string& llvm_type,
                                         uint32_t line, uint32_t arg_no = 0);
    void emit_debug_declare(const std::string& alloca_reg, int var_debug_id, int loc_id);
    int get_or_create_type_debug_info(const std::string& type_name, const std::string& llvm_type);

    // Helper methods
    auto fresh_reg() -> std::string;
    auto fresh_label(const std::string& prefix = "L") -> std::string;
    void emit(const std::string& code);
    void emit_line(const std::string& code);

    // Type translation
    auto llvm_type(const parser::Type& type) -> std::string;
    auto llvm_type_ptr(const parser::TypePtr& type) -> std::string;
    auto llvm_type_name(const std::string& name) -> std::string;
    // for_data=true: use "{}" for Unit (when used as data field), false: use "void" (for return
    // types)
    auto llvm_type_from_semantic(const types::TypePtr& type, bool for_data = false) -> std::string;

    // Generic type mangling
    auto mangle_type(const types::TypePtr& type) -> std::string;
    auto mangle_type_args(const std::vector<types::TypePtr>& args) -> std::string;
    auto mangle_struct_name(const std::string& base_name,
                            const std::vector<types::TypePtr>& type_args) -> std::string;
    auto mangle_func_name(const std::string& base_name,
                          const std::vector<types::TypePtr>& type_args) -> std::string;

    // Generic instantiation management
    auto require_struct_instantiation(const std::string& base_name,
                                      const std::vector<types::TypePtr>& type_args) -> std::string;
    auto require_enum_instantiation(const std::string& base_name,
                                    const std::vector<types::TypePtr>& type_args) -> std::string;
    auto require_func_instantiation(const std::string& base_name,
                                    const std::vector<types::TypePtr>& type_args) -> std::string;
    void generate_pending_instantiations();
    void gen_struct_instantiation(const parser::StructDecl& decl,
                                  const std::vector<types::TypePtr>& type_args);
    void gen_enum_instantiation(const parser::EnumDecl& decl,
                                const std::vector<types::TypePtr>& type_args);
    void gen_func_instantiation(const parser::FuncDecl& decl,
                                const std::vector<types::TypePtr>& type_args);

    // Helper: convert parser type to semantic type with generic substitution
    auto resolve_parser_type_with_subs(const parser::Type& type,
                                       const std::unordered_map<std::string, types::TypePtr>& subs)
        -> types::TypePtr;

    // Helper: convert LLVM type string back to semantic type (for common primitives)
    auto semantic_type_from_llvm(const std::string& llvm_type) -> types::TypePtr;

    // Helper: unify a parser type pattern with a semantic type to extract type bindings
    // Example: unify(Maybe[T], Maybe[I32], {T}) -> {T: I32}
    void unify_types(const parser::Type& pattern, const types::TypePtr& concrete,
                     const std::unordered_set<std::string>& generics,
                     std::unordered_map<std::string, types::TypePtr>& bindings);

    // Module structure
    void emit_header();
    void emit_runtime_decls();
    void emit_module_lowlevel_decls();
    void
    emit_module_pure_tml_functions(); // Generate code for pure TML functions from imported modules
    void emit_string_constants();

    // Declaration generation
    void gen_decl(const parser::Decl& decl);
    void gen_func_decl(const parser::FuncDecl& func);
    void gen_impl_method(const std::string& type_name, const parser::FuncDecl& method);
    void
    gen_impl_method_instantiation(const std::string& mangled_type_name,
                                  const parser::FuncDecl& method,
                                  const std::unordered_map<std::string, types::TypePtr>& type_subs,
                                  const std::vector<parser::GenericParam>& impl_generics);
    void gen_struct_decl(const parser::StructDecl& s);
    void gen_enum_decl(const parser::EnumDecl& e);

    // Statement generation
    void gen_stmt(const parser::Stmt& stmt);
    void gen_let_stmt(const parser::LetStmt& let);
    void gen_expr_stmt(const parser::ExprStmt& expr);

    // Pattern binding for destructuring
    // Binds pattern elements to extracted values from a tuple/struct
    // expected_type: the type we want to bind to (from annotation)
    // actual_type: the type of the expression value (defaults to expected_type if empty)
    void gen_pattern_binding(const parser::Pattern& pattern, const std::string& value,
                             const std::string& expected_type, const std::string& actual_type = "");

    // Tuple pattern binding helper for nested tuple destructuring
    void gen_tuple_pattern_binding(const parser::TuplePattern& pattern, const std::string& value,
                                   const std::string& tuple_type,
                                   const types::TypePtr& semantic_type);

    // Expression generation - returns the register holding the value
    auto gen_expr(const parser::Expr& expr) -> std::string;
    auto gen_literal(const parser::LiteralExpr& lit) -> std::string;
    auto gen_ident(const parser::IdentExpr& ident) -> std::string;
    auto gen_binary(const parser::BinaryExpr& bin) -> std::string;
    auto gen_unary(const parser::UnaryExpr& unary) -> std::string;
    auto gen_call(const parser::CallExpr& call) -> std::string;
    auto gen_if(const parser::IfExpr& if_expr) -> std::string;
    auto gen_ternary(const parser::TernaryExpr& ternary) -> std::string;
    auto gen_if_let(const parser::IfLetExpr& if_let) -> std::string;
    auto gen_block(const parser::BlockExpr& block) -> std::string;
    auto gen_loop(const parser::LoopExpr& loop) -> std::string;
    auto gen_while(const parser::WhileExpr& while_expr) -> std::string;
    auto gen_for(const parser::ForExpr& for_expr) -> std::string;
    auto gen_return(const parser::ReturnExpr& ret) -> std::string;
    auto gen_when(const parser::WhenExpr& when) -> std::string;
    auto gen_pattern_cmp(const parser::Pattern& pattern, const std::string& scrutinee,
                         const std::string& scrutinee_type, const std::string& tag,
                         bool is_primitive) -> std::string;
    auto gen_struct_expr(const parser::StructExpr& s) -> std::string;
    auto gen_struct_expr_ptr(const parser::StructExpr& s) -> std::string;
    auto gen_field(const parser::FieldExpr& field) -> std::string;
    auto gen_array(const parser::ArrayExpr& arr) -> std::string;
    auto gen_index(const parser::IndexExpr& idx) -> std::string;
    auto gen_path(const parser::PathExpr& path) -> std::string;
    auto gen_method_call(const parser::MethodCallExpr& call) -> std::string;

    // Method call helpers - split into separate files for maintainability
    auto gen_maybe_method(const parser::MethodCallExpr& call, const std::string& receiver,
                          const std::string& enum_type_name, const std::string& tag_val,
                          const types::NamedType& named) -> std::optional<std::string>;
    auto gen_outcome_method(const parser::MethodCallExpr& call, const std::string& receiver,
                            const std::string& enum_type_name, const std::string& tag_val,
                            const types::NamedType& named) -> std::optional<std::string>;
    auto gen_array_method(const parser::MethodCallExpr& call, const std::string& method)
        -> std::optional<std::string>;

    auto gen_closure(const parser::ClosureExpr& closure) -> std::string;
    auto gen_lowlevel(const parser::LowlevelExpr& lowlevel) -> std::string;
    auto gen_interp_string(const parser::InterpolatedStringExpr& interp) -> std::string;
    auto gen_cast(const parser::CastExpr& cast) -> std::string;
    auto gen_tuple(const parser::TupleExpr& tuple) -> std::string;
    auto gen_await(const parser::AwaitExpr& await_expr) -> std::string;
    auto gen_try(const parser::TryExpr& try_expr) -> std::string;

    // Async/await helpers
    auto wrap_in_poll_ready(const std::string& value, const std::string& value_type) -> std::string;
    auto extract_poll_ready(const std::string& poll_value, const std::string& poll_type,
                            const std::string& inner_type) -> std::string;

    // Format string print
    auto gen_format_print(const std::string& format, const std::vector<parser::ExprPtr>& args,
                          size_t start_idx, bool with_newline) -> std::string;

    // ============ Builtin Function Handlers ============
    // Each returns std::optional<std::string> - if handled, returns the result register
    // If not handled, returns std::nullopt to fall through to user-defined functions
    auto try_gen_builtin_io(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_builtin_mem(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_builtin_atomic(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_builtin_sync(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_builtin_time(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_builtin_math(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_builtin_collections(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_builtin_string(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_builtin_assert(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_builtin_async(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;

    // Utility
    void report_error(const std::string& msg, const SourceSpan& span);

    // Struct field access helpers
    auto get_field_index(const std::string& struct_name, const std::string& field_name) -> int;
    auto get_field_type(const std::string& struct_name, const std::string& field_name)
        -> std::string;

    // Type inference for generics instantiation
    auto infer_expr_type(const parser::Expr& expr) -> types::TypePtr;

    // String literal handling
    std::vector<std::pair<std::string, std::string>> string_literals_;
    auto add_string_literal(const std::string& value) -> std::string;

public:
    // Print argument type inference (used by gen_call and gen_format_print)
    enum class PrintArgType { Int, I64, Float, Bool, Str, Unknown };
    static PrintArgType infer_print_type(const parser::Expr& expr);
};

} // namespace tml::codegen
