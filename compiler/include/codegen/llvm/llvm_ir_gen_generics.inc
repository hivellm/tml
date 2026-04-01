// Generic instantiation, lazy library, loop metadata, alloca hoisting, and lifetime state for LLVMIRGen.
// This file is #included directly inside the LLVMIRGen class body in llvm_ir_gen.hpp.
// Do NOT add #pragma once, #include directives, or namespace declarations here.

    // ============ Generic Instantiation Support ============
    // Tracks generic type/function instantiations to avoid duplicates
    // and generate specialized code for each unique type argument combination

    struct GenericInstantiation {
        std::string base_name;                 // Original name (e.g., "Pair")
        std::vector<types::TypePtr> type_args; // Type arguments (e.g., [I32, Str])
        std::string mangled_name;              // Mangled name (e.g., "Pair__I32__Str")
        bool generated = false;                // Has code been generated?
    };

    // Cache of struct/enum/class instantiations (mangled_name -> info)
    std::unordered_map<std::string, GenericInstantiation> struct_instantiations_;
    std::unordered_map<std::string, GenericInstantiation> enum_instantiations_;
    std::unordered_map<std::string, GenericInstantiation> func_instantiations_;
    std::unordered_map<std::string, GenericInstantiation> class_instantiations_;

    // Pending queues: keys of instantiations not yet generated.
    // Avoids O(n) scan of entire maps on each iteration — only new items are processed.
    std::vector<std::string> pending_func_keys_;
    std::vector<std::string> pending_class_keys_;

    // Pending generic declarations (base_name -> AST node pointer)
    // These are registered but not generated until instantiated
    std::unordered_map<std::string, const parser::StructDecl*> pending_generic_structs_;
    std::unordered_map<std::string, const parser::EnumDecl*> pending_generic_enums_;

    // Names of generic structs defined in the local (user) module.
    // Used to prevent library structs with the same simple name from shadowing
    // local definitions during generic struct instantiation.
    std::unordered_set<std::string> local_generic_struct_names_;

    // All struct declarations (for accessing default field values during codegen)
    std::unordered_map<std::string, const parser::StructDecl*> struct_decls_;
    std::unordered_map<std::string, const parser::FuncDecl*> pending_generic_funcs_;
    /// All generic functions per bare name (for arity-based disambiguation).
    /// When multiple generic functions share the same bare name (e.g., mem::take[T](1 param)
    /// and iter::take[I](2 params)), the primary map stores only the first one registered.
    /// This map stores ALL of them so arity-based lookup can find the correct one.
    std::unordered_map<std::string, std::vector<std::pair<const parser::FuncDecl*, std::string>>>
        pending_generic_funcs_all_;
    /// Maps generic function bare name → module path (e.g., "take" → "core::iter")
    /// Used by Phase 4 mangling to add hierarchical path to generic instantiations.
    std::unordered_map<std::string, std::string> generic_func_modules_;
    std::unordered_map<std::string, const parser::ClassDecl*> pending_generic_classes_;

    // Pending generic impl blocks (type_name -> impl block pointer)
    // These are registered and methods are instantiated when called on concrete types
    std::unordered_map<std::string, const parser::ImplDecl*> pending_generic_impls_;

    // All generic impls per type (for types with multiple trait impls, like tuples)
    std::unordered_map<std::string, std::vector<const parser::ImplDecl*>>
        pending_generic_impls_all_;

    // Generated impl method instantiations (mangled_name -> true)
    // Tracks which specialized methods have been REQUESTED for generation
    std::unordered_set<std::string> generated_impl_methods_;

    // Tracks which impl methods have actually been OUTPUT to prevent duplicates
    // (separate from generated_impl_methods_ because the same method can be requested
    // from multiple code paths before being processed)
    std::unordered_set<std::string> generated_impl_methods_output_;

    // Generated function names (full LLVM names) to avoid duplicates
    // Used when processing directory modules that may have same-named functions
    std::unordered_set<std::string> generated_functions_;

    // Whether JSON runtime declarations have been emitted for @derive(Deserialize)
    bool json_runtime_declared_ = false;

    // Generated TypeInfo globals for @derive(Reflect) types
    std::unordered_set<std::string> generated_typeinfo_;

    // Pending impl method instantiation requests
    // Each entry: (mangled_type_name, method_name, type_subs, base_type_name, method_type_suffix)
    struct PendingImplMethod {
        std::string mangled_type_name;
        std::string method_name;
        std::unordered_map<std::string, types::TypePtr> type_subs;
        std::string base_type_name;     // Used to find the impl block
        std::string method_type_suffix; // For method-level generics like cast[U8] -> "U8"
        bool is_library_type = false;   // True for library types (no suite prefix)
    };
    std::vector<PendingImplMethod> pending_impl_method_instantiations_;

    // Pending generic class methods ((class_name::method_name) -> (class_decl, method_index))
    // For generic static methods like Utils::identity[T]
    struct PendingGenericClassMethod {
        const parser::ClassDecl* class_decl;
        size_t method_index;
    };
    std::unordered_map<std::string, PendingGenericClassMethod> pending_generic_class_methods_;

    // Pending generic class method instantiations to generate at end
    struct PendingGenericClassMethodInst {
        const parser::ClassDecl* class_decl;
        const parser::ClassMethod* method;
        std::string method_suffix;
        std::unordered_map<std::string, types::TypePtr> type_subs;
    };
    std::vector<PendingGenericClassMethodInst> pending_generic_class_method_insts_;

    // Function return types (func_name -> semantic return type)
    // Used by infer_expr_type to determine return types of function calls
    std::unordered_map<std::string, types::TypePtr> func_return_types_;

    // Concrete types for impl Behavior returns (func_name -> concrete LLVM type)
    // When a function returns `impl Behavior`, we analyze the function body to find
    // the actual concrete type being returned
    std::unordered_map<std::string, std::string> impl_behavior_concrete_types_;

    // ============ Lazy Library Definition Support ============
    // Instead of emitting full `define` for all library functions upfront,
    // we emit only `declare` and store the method info here. After user code
    // is processed, we generate `define` only for functions actually called.
    struct PendingLibraryMethod {
        std::string type_name;
        const parser::FuncDecl* method;
        std::string module_prefix;  // current_module_prefix_ when deferred
        std::string module_name;    // current_module_name_ when deferred (:: separated)
        std::string submodule_name; // current_submodule_name_ when deferred
    };
    // Key: LLVM function name (e.g., "@tml_RawSocket_close")
    std::unordered_map<std::string, PendingLibraryMethod> pending_library_methods_;

    struct PendingLibraryFunc {
        const parser::FuncDecl* func;
        std::string module_prefix;  // current_module_prefix_ when deferred
        std::string module_name;    // current_module_name_ when deferred (:: separated)
        std::string submodule_name; // current_submodule_name_ when deferred
    };
    std::unordered_map<std::string, PendingLibraryFunc> pending_library_funcs_;

    // Set of library function LLVM names that were referenced during user code generation
    std::unordered_set<std::string> referenced_library_funcs_;

    // Generate definitions for referenced library functions (called after user code)
    void emit_referenced_library_definitions();

    // Generate declarations for referenced library functions (library_decls_only + lazy mode)
    void emit_referenced_library_declarations();

    // Storage for imported module ASTs (keeps AST alive so pointers in pending_generic_* remain
    // valid). Uses deque instead of vector to prevent pointer invalidation on push_back,
    // since eligible_modules stores raw pointers into this container.
    std::deque<parser::Module> imported_module_asts_;

    // Storage for builtin generic enum declarations (keeps AST alive)
    std::vector<std::unique_ptr<parser::EnumDecl>> builtin_enum_decls_;

    // ============ Loop Metadata Support ============
    // LLVM loop metadata for optimization hints (vectorization, unrolling)
    int loop_metadata_counter_ =
        1000; // Counter for loop metadata IDs (start high to avoid debug ID conflicts)
    std::vector<std::string> loop_metadata_; // Loop metadata nodes to emit at end

    // Create loop metadata node and return its ID
    int create_loop_metadata(bool enable_vectorize = false, int unroll_count = 0);

    // Emit all loop metadata at end of module
    void emit_loop_metadata();

    // ============ Entry-Block Alloca Hoisting ============
    // Collects alloca instructions emitted during function body codegen.
    // At function close, these are spliced into the entry block so LLVM's
    // mem2reg can promote them to SSA registers (critical for SIMD perf).
    std::vector<std::string> entry_allocas_; // Collected alloca lines
    std::string alloca_hoisting_marker_;     // Unique marker string in output_
    bool alloca_hoisting_active_ = false;    // True during function body codegen

    /// Emit an alloca that will be hoisted to the function entry block.
    /// Returns the register name (e.g., "%t5").
    auto emit_hoisted_alloca(const std::string& type, const std::string& align = "") -> std::string;

    /// Begin collecting hoisted allocas (called after entry: + param allocas).
    void begin_alloca_hoisting();

    /// Splice collected allocas into the entry block (called before closing brace).
    void end_alloca_hoisting();

    // ============ Lifetime Intrinsics Support ============
    // Track stack allocations for lifetime intrinsics
    struct AllocaInfo {
        std::string reg; // Alloca register (e.g., %v1)
        int64_t size;    // Size in bytes (-1 if unknown)
    };

    // Stack of scope allocations - each scope has its list of allocas
    std::vector<std::vector<AllocaInfo>> scope_allocas_;

    // Push a new scope for tracking allocas
    void push_lifetime_scope();

    // Pop scope and emit lifetime.end for all allocas in scope
    void pop_lifetime_scope();

    // Clear scope without emitting lifetime.end (used when already emitted)
    void clear_lifetime_scope();

    // Emit lifetime.start for an alloca
    void emit_lifetime_start(const std::string& alloca_reg, int64_t size);

    // Emit lifetime.end for an alloca
    void emit_lifetime_end(const std::string& alloca_reg, int64_t size);

    // Register an alloca in current scope (for automatic lifetime.end on scope exit)
    void register_alloca_in_scope(const std::string& alloca_reg, int64_t size);

    // Emit lifetime.end for all allocas in all scopes (for early return)
    void emit_all_lifetime_ends();

    // Emit lifetime.end for allocas in current scope only (for break/continue)
    void emit_scope_lifetime_ends();

    // Get size in bytes for LLVM type
    int64_t get_type_size(const std::string& llvm_type);
