// Debug info, helper methods, type translation, code generation dispatch, and utility state for LLVMIRGen.
// This file is #included directly inside the LLVMIRGen class body in llvm_ir_gen.hpp.
// Do NOT add #pragma once, #include directives, or namespace declarations here.

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

    /// Returns the correct LLVM zero literal for a given type.
    /// ptr → "null", float/double → "0.0", struct types → "zeroinitializer", else → "0"
    static auto llvm_zero_value(const std::string& llvm_type) -> std::string {
        if (llvm_type == "ptr")
            return "null";
        if (llvm_type == "float" || llvm_type == "double")
            return "0.0";
        if (!llvm_type.empty() && llvm_type[0] == '{')
            return "zeroinitializer";
        return "0";
    }

    /// Emit a store, normalizing the value if it's a raw "0" to match the type.
    void emit_store(const std::string& type, const std::string& value, const std::string& ptr_reg) {
        std::string val = value;
        if (val == "0" && type == "ptr") {
            val = "null";
        } else if (val == "0" && (type.starts_with("%") || type.starts_with("{"))) {
            // Struct/aggregate types cannot use integer literal 0 — use zeroinitializer
            val = "zeroinitializer";
        }
        emit_line("  store " + type + " " + val + ", ptr " + ptr_reg);
    }

    /// Emits coverage instrumentation for a function call.
    /// Only emits if coverage_enabled is true. Tracks function calls for coverage reporting.
    void emit_coverage(const std::string& func_name);

    /// Returns suite prefix (e.g., "s0_") when in suite mode, empty string otherwise.
    /// Used to avoid symbol collisions when multiple test files are linked into one DLL.
    auto get_suite_prefix() const -> std::string;

    /// Hierarchical Itanium-style name mangling for library functions.
    /// Converts module_name ("::" separated) + func_name into N<len><seg>...<len><seg>E format.
    /// Example: mangle_tml_symbol("core::str", "to_lowercase") → "N4core3str12to_lowercaseE"
    /// For local (non-library) functions, returns func_name unchanged.
    auto mangle_tml_symbol(const std::string& module_name, const std::string& func_name) const
        -> std::string;

    /// Overload with parameter type encoding appended after the path.
    /// Example: mangle_tml_symbol("core::str", "repeat", {Str, I64}) → "N4core3str6repeatE_si"
    auto mangle_tml_symbol(const std::string& module_name, const std::string& func_name,
                           const std::vector<types::TypePtr>& param_types) const -> std::string;

    /// Encode a single TML type into a mangling code character/string.
    /// Primitives: I8→a, I16→s, I32→i, I64→l, I128→x, U8→h, U16→t, U32→j, U64→m, U128→y,
    ///            F32→f, F64→d, Bool→b, Char→c, Str→S, Unit→v, Never→z
    /// Structs/Enums: <len><name> (e.g., "4Text", "6Buffer")
    /// Ref types: R<inner>, Ptr: P<inner>, Slice: A<inner>
    static auto mangle_type_code(const types::TypePtr& type) -> std::string;

    /// FNV-1a 64-bit hash of a string, returned as 8-char hex.
    /// Used by Phase 4 to create compact hash suffixes for generic instantiations.
    static auto fnv1a_hash_hex(const std::string& input) -> std::string;

    /// Returns true if type_name::method is found in the module registry (library method).
    /// Used to avoid adding suite prefix to library method calls.
    auto is_library_method(const std::string& type_name, const std::string& method) const -> bool;

    /// Find the module that defines a given type (struct, enum, or class).
    /// Returns the module name (e.g., "core::str") or empty string if the type is local.
    auto find_module_for_type(const std::string& type_name) const -> std::string;

    /// Generate a mangled LLVM symbol name for an impl method.
    /// For library types: uses Itanium-style encoding with module path.
    ///   e.g., ("Str", "split", "core::str") → "tml_N4core3str3Str5splitE"
    /// For local types: uses flat naming with optional suite prefix.
    ///   e.g., ("MyStruct", "foo", "") → "tml_s0_MyStruct_foo"
    auto mangle_impl_method(const std::string& type_name, const std::string& method_name) const
        -> std::string;

    // Type translation
    auto llvm_type(const parser::Type& type) -> std::string;
    auto llvm_type_ptr(const parser::TypePtr& type) -> std::string;
    auto llvm_type_name(const std::string& name) -> std::string;
    // for_data=true (default): use "{}" for Unit (data contexts: alloca, store, load, args)
    // for_data=false: use "void" for Unit (only for LLVM function return types)
    auto llvm_type_from_semantic(const types::TypePtr& type, bool for_data = true) -> std::string;
    /// Ensures a type is defined in the LLVM IR output (emits type definition if needed)
    void ensure_type_defined(const parser::TypePtr& type);

    // Generic type mangling
    auto mangle_type(const types::TypePtr& type) -> std::string;
    auto mangle_type_args(const std::vector<types::TypePtr>& args) -> std::string;
    auto mangle_struct_name(const std::string& base_name,
                            const std::vector<types::TypePtr>& type_args) -> std::string;
    auto mangle_func_name(const std::string& base_name,
                          const std::vector<types::TypePtr>& type_args) -> std::string;

    // Generic instantiation management
    void ensure_generic_types_instantiated(const types::TypePtr& type);
    auto require_struct_instantiation(const std::string& base_name,
                                      const std::vector<types::TypePtr>& type_args) -> std::string;
    auto require_enum_instantiation(const std::string& base_name,
                                    const std::vector<types::TypePtr>& type_args) -> std::string;
    auto require_func_instantiation(const std::string& base_name,
                                    const std::vector<types::TypePtr>& type_args) -> std::string;
    auto require_class_instantiation(const std::string& base_name,
                                     const std::vector<types::TypePtr>& type_args) -> std::string;
    void generate_pending_instantiations();

    /// Process all pending impl method instantiations until queue is empty.
    /// Returns true if any methods were generated (triggers outer loop to continue).
    bool generate_pending_impl_method_instantiations();

    /// Library-only IR path: flush all pending lazy library methods/functions,
    /// emit instantiations, and return the complete library IR.
    /// Called from generate() when options_.library_ir_only is true.
    auto generate_library_only_ir(const parser::Module& module)
        -> Result<std::string, std::vector<LLVMGenError>>;

    /// First pass over module declarations: register const values, struct/enum/class types,
    /// trait declarations, and pre-register local function signatures.
    /// Called from generate() before function body codegen.
    void generate_first_pass(const parser::Module& module);

    /// Second pass over module declarations: generate all function and impl-method bodies.
    /// Writes generated IR into output_ (the caller saves it to func_output afterwards).
    /// Called from generate() after generate_first_pass().
    void generate_function_bodies(const parser::Module& module);

    /// Generate main entry point, test/bench/fuzz harness, and HTTP route registration.
    /// Called from generate() after all function bodies have been emitted.
    void generate_main_and_test_harness(const parser::Module& module);
    void gen_struct_instantiation(const parser::StructDecl& decl,
                                  const std::vector<types::TypePtr>& type_args);
    void gen_enum_instantiation(const parser::EnumDecl& decl,
                                const std::vector<types::TypePtr>& type_args);
    void gen_func_instantiation(const parser::FuncDecl& decl,
                                const std::vector<types::TypePtr>& type_args);
    void gen_class_instantiation(const parser::ClassDecl& decl,
                                 const std::vector<types::TypePtr>& type_args);

    // Helper: convert parser type to semantic type with generic substitution
    auto resolve_parser_type_with_subs(const parser::Type& type,
                                       const std::unordered_map<std::string, types::TypePtr>& subs)
        -> types::TypePtr;

    // Helper: apply type substitutions to a semantic type
    auto apply_type_substitutions(const types::TypePtr& type,
                                  const std::unordered_map<std::string, types::TypePtr>& subs)
        -> types::TypePtr;

    // Helper: convert LLVM type string back to semantic type (for common primitives)
    auto semantic_type_from_llvm(const std::string& llvm_type) -> types::TypePtr;

    // Helper: check if a type contains unresolved generic type parameters
    // Returns true if the type or any nested type contains GenericType
    auto contains_unresolved_generic(const types::TypePtr& type) -> bool;

    // Helper: unify a parser type pattern with a semantic type to extract type bindings
    // Example: unify(Maybe[T], Maybe[I32], {T}) -> {T: I32}
    void unify_types(const parser::Type& pattern, const types::TypePtr& concrete,
                     const std::unordered_set<std::string>& generics,
                     std::unordered_map<std::string, types::TypePtr>& bindings);

    // Helper: find an associated type for a concrete type
    // Example: lookup_associated_type("RangeIterI64", "Item") -> I64
    auto lookup_associated_type(const std::string& type_name, const std::string& assoc_name)
        -> types::TypePtr;

    /// Resolve an associated type for a concrete generic type.
    /// Given a concrete type like SliceIter[I32] and assoc name "Item",
    /// looks up the raw binding (ref T) and substitutes the type args (T=I32)
    /// to produce the fully resolved type (ref I32).
    auto resolve_assoc_type_for_concrete(const types::TypePtr& concrete_type,
                                         const std::string& assoc_name) -> types::TypePtr;

    // Module structure
    void emit_header();
    void emit_runtime_decls();
    void init_runtime_catalog(); ///< Populate catalog of all possible runtime decls
    void require_runtime_decl(const std::string& name); ///< Mark a runtime decl as needed
    void finalize_runtime_decls(); ///< Emit only needed decls into deferred buffer
    void scan_for_runtime_refs(const std::string& text); ///< Scan text block for @symbol refs
    void emit_module_lowlevel_decls();
    /// Generate code for pure TML functions from imported modules.
    /// @param skip_modules If non-empty, skip modules whose path is in this set.
    ///                     Used for supplemental processing after cached state restoration.
    void emit_module_pure_tml_functions(const std::unordered_set<std::string>& skip_modules = {});
    void emit_string_constants();

    // Declaration generation
    void gen_decl(const parser::Decl& decl);
    void gen_func_decl(const parser::FuncDecl& func);
    void pre_register_func(const parser::FuncDecl& func); // Pre-register without generating code
    void gen_impl_method(const std::string& type_name, const parser::FuncDecl& method);
    void gen_impl_method_instantiation(
        const std::string& mangled_type_name, const parser::FuncDecl& method,
        const std::unordered_map<std::string, types::TypePtr>& type_subs,
        const std::vector<parser::GenericParam>& impl_generics,
        const std::string& method_type_suffix = "", bool is_library_type = false,
        const std::string& base_type_name = "", const parser::Type* impl_self_type = nullptr);
    void gen_struct_decl(const parser::StructDecl& s);
    void gen_union_decl(const parser::UnionDecl& u);
    void gen_enum_decl(const parser::EnumDecl& e);
    void gen_namespace_decl(const parser::NamespaceDecl& ns);

    // @derive(Reflect) support
    void gen_derive_reflect_struct(const parser::StructDecl& s);
    void gen_derive_reflect_enum(const parser::EnumDecl& e);
    void gen_derive_reflect_impl(const std::string& type_name, const std::string& typeinfo_name);
    void gen_derive_reflect_enum_methods(const parser::EnumDecl& e, const std::string& type_name);
    void gen_derive_reflect_field_accessors(const parser::StructDecl& s,
                                            const std::string& type_name);
    void ensure_reflect_types_defined();

    // @derive(PartialEq, Eq) support
    void gen_derive_partial_eq_struct(const parser::StructDecl& s);
    void gen_derive_partial_eq_enum(const parser::EnumDecl& e);

    // @derive(Duplicate, Copy) support
    void gen_derive_duplicate_struct(const parser::StructDecl& s);
    void gen_derive_duplicate_enum(const parser::EnumDecl& e);

    // @derive(Hash) support
    void gen_derive_hash_struct(const parser::StructDecl& s);
    void gen_derive_hash_enum(const parser::EnumDecl& e);

    // @derive(Default) support
    void gen_derive_default_struct(const parser::StructDecl& s);
    void gen_derive_default_enum(const parser::EnumDecl& e);

    // @derive(PartialOrd) support
    void gen_derive_partial_ord_struct(const parser::StructDecl& s);
    void gen_derive_partial_ord_enum(const parser::EnumDecl& e);

    // @derive(Ord) support
    void gen_derive_ord_struct(const parser::StructDecl& s);
    void gen_derive_ord_enum(const parser::EnumDecl& e);

    // @derive(Debug) support
    void gen_derive_debug_struct(const parser::StructDecl& s);
    void gen_derive_debug_enum(const parser::EnumDecl& e);

    // @derive(Display) support
    void gen_derive_display_struct(const parser::StructDecl& s);
    void gen_derive_display_enum(const parser::EnumDecl& e);

    // @derive(Serialize) support
    void gen_derive_serialize_struct(const parser::StructDecl& s);
    void gen_derive_serialize_enum(const parser::EnumDecl& e);

    // @derive(Deserialize) support
    void gen_derive_deserialize_struct(const parser::StructDecl& s);
    void gen_derive_deserialize_enum(const parser::EnumDecl& e);

    // @derive(FromStr) support
    void gen_derive_fromstr_struct(const parser::StructDecl& s);
    void gen_derive_fromstr_enum(const parser::EnumDecl& e);

    // Statement generation
    void gen_stmt(const parser::Stmt& stmt);
    void gen_let_stmt(const parser::LetStmt& let);
    void gen_let_else_stmt(const parser::LetElseStmt& let_else);
    void gen_expr_stmt(const parser::ExprStmt& expr);
    void gen_nested_decl(const parser::Decl& decl);

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
    auto gen_binary_ops(const parser::BinaryExpr& bin) -> std::string;
    auto gen_unary(const parser::UnaryExpr& unary) -> std::string;
    auto gen_call(const parser::CallExpr& call) -> std::string;

    // gen_call sub-dispatchers (split for file size management)
    auto gen_call_primitive_or_intrinsic(const parser::CallExpr& call, const std::string& fn_name)
        -> std::optional<std::string>;
    auto gen_call_enum_constructor(const parser::CallExpr& call, const std::string& fn_name)
        -> std::optional<std::string>;
    auto gen_call_indirect(const parser::CallExpr& call, const std::string& fn_name)
        -> std::optional<std::string>;
    auto gen_call_generic_func(const parser::CallExpr& call, const std::string& fn_name)
        -> std::optional<std::string>;
    auto gen_call_class_constructor(const parser::CallExpr& call, const std::string& fn_name)
        -> std::optional<std::string>;
    auto gen_call_generic_struct_method(const parser::CallExpr& call, const std::string& fn_name)
        -> std::optional<std::string>;
    auto gen_call_user_function(const parser::CallExpr& call, const std::string& fn_name)
        -> std::string;

    auto gen_if(const parser::IfExpr& if_expr) -> std::string;
    auto gen_ternary(const parser::TernaryExpr& ternary) -> std::string;
    auto gen_if_let(const parser::IfLetExpr& if_let) -> std::string;
    auto gen_block(const parser::BlockExpr& block) -> std::string;
    auto gen_loop(const parser::LoopExpr& loop) -> std::string;
    auto gen_while(const parser::WhileExpr& while_expr) -> std::string;
    auto gen_for(const parser::ForExpr& for_expr) -> std::string;
    auto gen_for_iterator(const parser::ForExpr& for_expr, const std::string& type_name)
        -> std::string;
    auto gen_for_unrolled(const parser::ForExpr& for_expr, const std::string& var_name,
                          const std::string& type_name, size_t iteration_count) -> std::string;
    auto gen_return(const parser::ReturnExpr& ret) -> std::string;
    auto gen_throw(const parser::ThrowExpr& thr) -> std::string;
    auto gen_when(const parser::WhenExpr& when) -> std::string;
    auto gen_pattern_cmp(const parser::Pattern& pattern, const std::string& scrutinee,
                         const std::string& scrutinee_type, const std::string& tag,
                         bool is_primitive) -> std::string;
    auto gen_struct_expr(const parser::StructExpr& s) -> std::string;
    auto gen_struct_expr_ptr(const parser::StructExpr& s) -> std::string;
    auto gen_simd_struct_expr_ptr(const parser::StructExpr& s, const SimdTypeInfo& info)
        -> std::string;
    auto gen_field(const parser::FieldExpr& field) -> std::string;
    auto gen_array(const parser::ArrayExpr& arr) -> std::string;
    auto gen_index(const parser::IndexExpr& idx) -> std::string;
    auto gen_path(const parser::PathExpr& path) -> std::string;
    auto gen_method_call(const parser::MethodCallExpr& call) -> std::string;

    // Method call helpers - split into separate files for maintainability
    auto gen_static_method_call(const parser::MethodCallExpr& call, std::string type_name)
        -> std::optional<std::string>;
    auto gen_primitive_method(const parser::MethodCallExpr& call, const std::string& receiver,
                              const std::string& receiver_ptr, types::TypePtr receiver_type)
        -> std::optional<std::string>;
    auto gen_primitive_method_ext(const parser::MethodCallExpr& call, const std::string& receiver,
                                  const std::string& receiver_ptr, types::TypePtr receiver_type,
                                  types::TypePtr inner_type, types::PrimitiveKind kind,
                                  bool is_integer, bool is_signed, bool is_float,
                                  const std::string& llvm_ty) -> std::optional<std::string>;
    auto gen_collection_method(const parser::MethodCallExpr& call, const std::string& receiver,
                               const std::string& receiver_type_name, types::TypePtr receiver_type)
        -> std::optional<std::string>;
    auto gen_slice_method(const parser::MethodCallExpr& call, const std::string& receiver,
                          const std::string& receiver_type_name, types::TypePtr receiver_type)
        -> std::optional<std::string>;
    auto gen_maybe_method(const parser::MethodCallExpr& call, const std::string& receiver,
                          const std::string& enum_type_name, const std::string& tag_val,
                          const types::NamedType& named) -> std::optional<std::string>;
    auto gen_outcome_method(const parser::MethodCallExpr& call, const std::string& receiver,
                            const std::string& enum_type_name, const std::string& tag_val,
                            const types::NamedType& named) -> std::optional<std::string>;
    auto gen_array_method(const parser::MethodCallExpr& call, const std::string& method)
        -> std::optional<std::string>;
    auto gen_slice_type_method(const parser::MethodCallExpr& call, const std::string& method)
        -> std::optional<std::string>;

    // Static method dispatch (extracted from gen_method_call section 1)
    auto gen_method_static_dispatch(const parser::MethodCallExpr& call, const std::string& method)
        -> std::optional<std::string>;
    // Bounded generic dispatch (extracted from gen_method_call section 4b)
    auto gen_method_bounded_generic_dispatch(const parser::MethodCallExpr& call,
                                             const std::string& method, const std::string& receiver,
                                             const std::string& receiver_ptr,
                                             const types::TypePtr& receiver_type,
                                             const std::string& receiver_type_name,
                                             bool receiver_was_ref) -> std::optional<std::string>;
    // Fn trait method calls (extracted from gen_method_call section 13)
    auto gen_method_fn_trait_call(const parser::MethodCallExpr& call, const std::string& method,
                                  const std::string& receiver, const types::TypePtr& receiver_type)
        -> std::optional<std::string>;

    // Impl method helpers (extracted from gen_method_call)
    auto try_gen_impl_method_call(const parser::MethodCallExpr& call, const std::string& receiver,
                                  const std::string& receiver_ptr,
                                  const types::TypePtr& receiver_type)
        -> std::optional<std::string>;
    auto
    try_gen_module_impl_method_call(const parser::MethodCallExpr& call, const std::string& receiver,
                                    const std::string& receiver_ptr, types::TypePtr receiver_type)
        -> std::optional<std::string>;
    auto try_gen_dyn_dispatch_call(const parser::MethodCallExpr& call, const std::string& receiver,
                                   types::TypePtr receiver_type) -> std::optional<std::string>;
    auto try_gen_class_instance_call(const parser::MethodCallExpr& call,
                                     const std::string& receiver, const std::string& receiver_ptr,
                                     types::TypePtr receiver_type) -> std::optional<std::string>;
    auto try_gen_primitive_behavior_method(const parser::MethodCallExpr& call,
                                           const std::string& receiver,
                                           types::TypePtr receiver_type,
                                           const std::string& receiver_type_name,
                                           bool receiver_was_ref) -> std::optional<std::string>;

    auto gen_closure(const parser::ClosureExpr& closure) -> std::string;
    auto gen_lowlevel(const parser::LowlevelExpr& lowlevel) -> std::string;
    auto gen_interp_string(const parser::InterpolatedStringExpr& interp) -> std::string;
    auto gen_template_literal(const parser::TemplateLiteralExpr& tpl) -> std::string;
    auto gen_cast(const parser::CastExpr& cast) -> std::string;
    auto gen_is_check(const parser::IsExpr& is_expr) -> std::string;
    auto gen_class_safe_cast(const std::string& src_ptr, const std::string& src_class,
                             const std::string& target_name, const parser::TypePtr& target_type,
                             bool target_is_class) -> std::string;
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
