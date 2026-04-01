// OOP, vtable, and optimization state for LLVMIRGen.
// This file is #included directly inside the LLVMIRGen class body in llvm_ir_gen.hpp.
// Do NOT add #pragma once, #include directives, or namespace declarations here.

    // ============ OOP Class Support (C#-style) ============
    // Tracks classes with single inheritance and virtual dispatch

    // Path element for multi-level inheritance access
    struct InheritancePathStep {
        std::string class_name; // Class to GEP into
        int index;              // Index within that class
    };

    // Class field info for field access
    struct ClassFieldInfo {
        std::string name;
        int index; // Field index in LLVM struct (-1 for inherited)
        std::string llvm_type;
        parser::MemberVisibility vis;
        // For inherited fields: full path through inheritance chain
        bool is_inherited = false;
        // Path from current class to the field (each step is a GEP)
        // Example: For Derived4.value (4 levels deep):
        // [{Derived3, 1}, {Derived2, 1}, {Derived1, 1}, {Base, 1}]
        std::vector<InheritancePathStep> inheritance_path;
    };

    // Virtual method info for vtable layout
    struct VirtualMethodInfo {
        std::string name;            // Method name
        std::string declaring_class; // Class that first declared this virtual
        std::string impl_class;      // Class that implements this slot (empty if abstract)
        size_t vtable_index;         // Slot in vtable
    };

    // Class type mapping (class_name -> LLVM type name)
    std::unordered_map<std::string, std::string> class_types_;

    // Value classes (classes with @value decorator - no vtable, direct dispatch)
    std::unordered_set<std::string> value_classes_;

    // Pool classes (classes with @pool decorator - use object pool allocation)
    std::unordered_set<std::string> pool_classes_;

    // Thread-local pool classes (classes with @pool(thread_local: true))
    std::unordered_set<std::string> tls_pool_classes_;

    // Emitted RTTI (class_name -> true if RTTI global has been emitted)
    std::unordered_set<std::string> emitted_rtti_;

    // TypeInfo type emitted flag (reset per compilation unit)
    bool typeinfo_type_emitted_ = false;

    // Class field info (class_name -> field info list)
    std::unordered_map<std::string, std::vector<ClassFieldInfo>> class_fields_;

    // Class metadata for OOP reflection intrinsics (class_name -> meta)
    struct ClassMeta {
        std::string base_class; // Base class name (empty if none)
        bool is_abstract = false;
        bool is_sealed = false;
        size_t method_count = 0;               // Total methods (including static)
        std::vector<std::string> method_names; // All method names in declaration order
        std::vector<bool> method_is_virtual;   // Per-method virtual flag
        std::vector<bool> method_is_override;  // Per-method override flag
        std::vector<bool> method_is_static;    // Per-method static flag
    };
    std::unordered_map<std::string, ClassMeta> class_meta_;

    // Static field info (ClassName.fieldName -> {global_name, type})
    struct StaticFieldInfo {
        std::string global_name; // LLVM global variable name
        std::string type;        // LLVM type
    };
    std::unordered_map<std::string, StaticFieldInfo> static_fields_;

    // Property info for classes (ClassName.propName -> property info)
    struct ClassPropertyInfo {
        std::string name;      // Property name
        std::string llvm_type; // LLVM type of the property
        bool has_getter;       // Has getter method
        bool has_setter;       // Has setter method
        bool is_static;        // Static property
    };
    std::unordered_map<std::string, ClassPropertyInfo> class_properties_;

    // Class vtable layout (class_name -> vtable method slots)
    std::unordered_map<std::string, std::vector<VirtualMethodInfo>> class_vtable_layout_;

    // Interface method order (interface_name -> method names)
    std::unordered_map<std::string, std::vector<std::string>> interface_method_order_;

    // Emitted interface vtable types (to avoid duplicates)
    std::set<std::string> emitted_interface_vtable_types_;

    // Interface vtables for class implementations (ClassName::InterfaceName -> vtable name)
    std::unordered_map<std::string, std::string> interface_vtables_;

    // ============ Vtable Deduplication (Phase 6.1) ============
    // Tracks vtable content for deduplication - identical vtables share storage

    // Vtable content key: sorted list of method pointers as string
    // Maps content key -> vtable global name
    std::unordered_map<std::string, std::string> vtable_content_to_name_;

    // Maps class name -> shared vtable name (when deduplicated)
    std::unordered_map<std::string, std::string> class_to_shared_vtable_;

    // Vtable deduplication statistics
    struct VtableDeduplicationStats {
        size_t total_vtables = 0;  ///< Total vtables generated
        size_t unique_vtables = 0; ///< Unique vtable layouts
        size_t deduplicated = 0;   ///< Vtables sharing storage with another
        size_t bytes_saved = 0;    ///< Estimated bytes saved
    };
    VtableDeduplicationStats vtable_dedup_stats_;

    // Interface vtable optimization statistics
    struct InterfaceVtableStats {
        size_t total_interface_vtables = 0; ///< Total interface vtables generated
        size_t deduplicated_interface = 0;  ///< Interface vtables sharing storage
        size_t compacted_slots = 0;         ///< Slots removed by compaction
    };
    InterfaceVtableStats interface_vtable_stats_;

    // Interface vtable content to name mapping (for deduplication)
    std::unordered_map<std::string, std::string> interface_vtable_content_to_name_;

    // Helper to compute vtable content key for deduplication
    auto compute_vtable_content_key(const std::vector<VirtualMethodInfo>& methods) -> std::string;

    // Helper to compute interface vtable content key
    auto compute_interface_vtable_key(const std::string& iface_name,
                                      const std::vector<std::pair<std::string, std::string>>& impls)
        -> std::string;

    // ============ Phase 6.3.4: Sparse Interface Layout Optimization ============
    // Removes gaps from sparse interface vtable layouts where methods have null implementations

    struct InterfaceLayoutInfo {
        std::string interface_name;
        std::vector<std::string> method_names; // All methods in original order
        std::vector<size_t> compacted_indices; // Mapping from original to compacted
        std::vector<bool> has_implementation;  // Which slots have non-null implementations
        size_t original_size = 0;
        size_t compacted_size = 0;
    };

    // Interface layout optimization info (interface_name -> layout info)
    std::unordered_map<std::string, InterfaceLayoutInfo> interface_layouts_;

    // Statistics for interface layout optimization
    struct InterfaceLayoutStats {
        size_t interfaces_analyzed = 0;  // Total interfaces analyzed
        size_t interfaces_compacted = 0; // Interfaces with gaps removed
        size_t slots_removed = 0;        // Total null slots removed
        size_t bytes_saved = 0;          // Estimated bytes saved
    };
    InterfaceLayoutStats interface_layout_stats_;

    // Analyze interface layout for gap removal
    auto analyze_interface_layout(const std::string& iface_name,
                                  const std::vector<std::pair<std::string, std::string>>& impls)
        -> InterfaceLayoutInfo;

    // Generate compacted interface vtable
    void
    gen_compacted_interface_vtable(const std::string& class_name, const std::string& iface_name,
                                   const InterfaceLayoutInfo& layout,
                                   const std::vector<std::pair<std::string, std::string>>& impls);

    // Get compacted vtable index for an interface method
    auto get_compacted_interface_index(const std::string& iface_name,
                                       const std::string& method_name) const -> size_t;

    // ============ Phase 6.2: Vtable Splitting (Hot/Cold) ============
    // Splits vtables into primary (hot) and secondary (cold) parts
    // to improve cache locality for frequently-called methods

    struct VtableSplitInfo {
        std::vector<std::string> hot_methods;  // Methods in primary vtable
        std::vector<std::string> cold_methods; // Methods in secondary vtable
        std::string primary_vtable_name;       // Name of hot vtable
        std::string secondary_vtable_name;     // Name of cold vtable (nullptr if empty)
    };

    // Method heat tracking: method_key -> call_count (heuristic-based)
    std::unordered_map<std::string, int> method_heat_;

    // Split vtable info per class
    std::unordered_map<std::string, VtableSplitInfo> vtable_splits_;

    // Vtable splitting statistics
    struct VtableSplitStats {
        size_t classes_with_split = 0; // Classes with split vtables
        size_t hot_methods_total = 0;  // Total methods in hot vtables
        size_t cold_methods_total = 0; // Total methods in cold vtables
    };
    VtableSplitStats vtable_split_stats_;

    // Analyze class methods and decide split
    void analyze_vtable_split(const parser::ClassDecl& c);

    // Generate split vtables (hot + cold)
    void gen_split_vtables(const parser::ClassDecl& c);

    // Check if a method is in the hot set
    bool is_hot_method(const std::string& class_name, const std::string& method_name) const;

    // Get vtable index for method (handles split vtables)
    auto get_split_vtable_index(const std::string& class_name, const std::string& method_name)
        -> std::pair<bool, size_t>; // (is_in_hot, index)

    // ============ Phase 10.3: Arena Allocation Integration ============
    // Tracks arena allocation context for skip destructor generation
    // and optimized bump-pointer allocation

    struct ArenaAllocContext {
        std::string arena_reg;        // Register holding arena pointer
        std::string arena_type;       // Arena type name
        bool skip_destructors = true; // Whether to skip destructors for arena objects
    };

    // Current arena context (set when allocating within arena)
    std::optional<ArenaAllocContext> current_arena_context_;

    // Arena classes (classes allocated via arena.alloc[T]())
    std::unordered_set<std::string> arena_allocated_values_;

    // Arena allocation statistics
    struct ArenaAllocStats {
        size_t arena_allocations = 0;   // Allocations via arena
        size_t destructors_skipped = 0; // Destructors skipped for arena objects
        size_t bump_ptr_ops = 0;        // Bump pointer operations generated
    };
    ArenaAllocStats arena_alloc_stats_;

    // Check if a value is arena-allocated (skip destructor)
    bool is_arena_allocated(const std::string& value_reg) const;

    // Generate arena bump-pointer allocation
    auto gen_arena_alloc(const std::string& arena_reg, const std::string& type_name, size_t size,
                         size_t align) -> std::string;

    // ============ Phase 11: Small Object Optimization (SOO) ============
    // Tracks small classes for inline storage optimization

    struct SooTypeInfo {
        std::string type_name;
        size_t computed_size = 0;     // Size in bytes
        size_t alignment = 8;         // Alignment requirement
        bool is_small = false;        // Eligible for SOO (size <= threshold)
        bool has_trivial_dtor = true; // Has trivial destructor
    };

    // SOO threshold (objects <= this size can be inlined)
    static constexpr size_t SOO_THRESHOLD = 64;

    // Computed type sizes (type_name -> size info)
    std::unordered_map<std::string, SooTypeInfo> type_size_cache_;

    // SOO statistics
    struct SooStats {
        size_t types_analyzed = 0;      // Total types analyzed
        size_t small_types = 0;         // Types eligible for SOO
        size_t inlined_allocations = 0; // Allocations that could be inlined
    };
    SooStats soo_stats_;

    // Calculate class/struct size at compile time
    auto calculate_type_size(const std::string& type_name) -> SooTypeInfo;

    // Check if type is eligible for SOO
    bool is_soo_eligible(const std::string& type_name);

    // ============ Phase 13: Cache-Friendly Layout ============
    // Field reordering and alignment optimization

    struct FieldLayoutInfo {
        std::string name;
        std::string llvm_type;
        size_t size;
        size_t alignment;
        int heat_score; // Higher = more frequently accessed
        bool is_hot;    // Mark for hot path placement
    };

    struct OptimizedLayout {
        std::vector<FieldLayoutInfo> fields; // Reordered fields
        size_t total_size = 0;
        size_t total_padding = 0;
        bool is_cache_aligned = false;
    };

    // Cache-friendly layout statistics
    struct CacheLayoutStats {
        size_t types_optimized = 0;     // Types with reordered fields
        size_t padding_saved = 0;       // Bytes of padding saved
        size_t hot_fields_promoted = 0; // Hot fields moved to start
    };
    CacheLayoutStats cache_layout_stats_;

    // Analyze and optimize field layout for cache efficiency
    auto optimize_field_layout(const std::string& type_name,
                               const std::vector<FieldLayoutInfo>& fields) -> OptimizedLayout;

    // Check if type should be cache-line aligned
    bool should_cache_align(const std::string& type_name) const;

    // ============ Phase 14: Class Monomorphization ============
    // Detection and specialization of generic functions with class parameters

    struct MonomorphizationCandidate {
        std::string func_name;
        std::string class_param;    // Class type parameter name
        std::string concrete_class; // Concrete class to specialize for
        bool benefits_from_devirt;  // Would devirtualization help?
    };

    // Pending monomorphization requests
    std::vector<MonomorphizationCandidate> pending_monomorphizations_;

    // Generated specialized functions
    std::unordered_set<std::string> specialized_functions_;

    // Monomorphization statistics
    struct MonomorphStats {
        size_t candidates_found = 0;
        size_t specializations_generated = 0;
        size_t devirt_opportunities = 0;
    };
    MonomorphStats monomorph_stats_;

    // Detect monomorphization opportunities
    void analyze_monomorphization_candidates(const parser::FuncDecl& func);

    // Generate specialized function for concrete class
    void gen_specialized_function(const MonomorphizationCandidate& candidate);

    // ============ Phase 3: Speculative Devirtualization ============
    // Inserts type guards for likely receiver types, enabling direct calls
    // with fallback to vtable dispatch for unexpected types

    struct SpeculativeDevirtInfo {
        std::string expected_type;      // Most likely concrete type
        std::string direct_call_target; // Direct function name for expected type
        float confidence;               // Probability estimate (0.0-1.0)
    };

    // Type frequency hints (class_name -> estimated frequency 0.0-1.0)
    // Used heuristically based on: sealed/final, leaf in hierarchy, @hot decorator
    std::unordered_map<std::string, float> type_frequency_hints_;

    // Speculative devirtualization statistics
    struct SpecDevirtStats {
        size_t guarded_calls = 0; // Calls with type guards inserted
        size_t direct_calls = 0;  // Calls converted to direct (no guard needed)
        size_t virtual_calls = 0; // Calls remaining as virtual dispatch
    };
    SpecDevirtStats spec_devirt_stats_;

    // Analyze if speculative devirtualization is profitable for a call
    auto analyze_spec_devirt(const std::string& receiver_class, const std::string& method_name)
        -> std::optional<SpeculativeDevirtInfo>;

    // Generate guarded call with fast path (direct) and slow path (vtable)
    auto gen_guarded_virtual_call(const std::string& obj_reg, const std::string& receiver_class,
                                  const SpeculativeDevirtInfo& spec_info,
                                  const std::string& method_name,
                                  const std::vector<std::string>& args,
                                  const std::vector<std::string>& arg_types) -> std::string;

    // Initialize type frequency hints from class hierarchy
    void init_type_frequency_hints();

    // Generate class declaration (type + vtable + methods)
    void gen_class_decl(const parser::ClassDecl& c);

    // Emit type definition for external class (from imported module)
    void emit_external_class_type(const std::string& name, const types::ClassDef& def);

    // Generate class vtable
    void gen_class_vtable(const parser::ClassDecl& c);

    // Generate class RTTI (Runtime Type Information)
    void gen_class_rtti(const parser::ClassDecl& c);

    // Generate interface vtables for implemented interfaces
    void gen_interface_vtables(const parser::ClassDecl& c);

    // Generate class constructor
    void gen_class_constructor(const parser::ClassDecl& c, const parser::ConstructorDecl& ctor);

    // Generate class method
    void gen_class_method(const parser::ClassDecl& c, const parser::ClassMethod& method);

    // Generate class constructor for generic class instantiation
    void gen_class_constructor_instantiation(
        const parser::ClassDecl& c, const parser::ConstructorDecl& ctor,
        const std::string& mangled_name,
        const std::unordered_map<std::string, types::TypePtr>& type_subs);

    // Generate class method for generic class instantiation
    void gen_class_method_instantiation(
        const parser::ClassDecl& c, const parser::ClassMethod& method,
        const std::string& mangled_name,
        const std::unordered_map<std::string, types::TypePtr>& type_subs);

    // Generate generic static method with method-level type parameters
    void gen_generic_class_static_method(
        const parser::ClassDecl& c, const parser::ClassMethod& method,
        const std::string& method_suffix,
        const std::unordered_map<std::string, types::TypePtr>& type_subs);

    // Generate class property getter/setter methods
    void gen_class_property(const parser::ClassDecl& c, const parser::PropertyDecl& prop);

    // Generate virtual method call dispatch
    auto gen_virtual_call(const std::string& obj_reg, const std::string& class_name,
                          const std::string& method_name, const std::vector<std::string>& args,
                          const std::vector<std::string>& arg_types) -> std::string;

    // Generate interface declaration
    void gen_interface_decl(const parser::InterfaceDecl& iface);

    // Generate base expression (base.method() or base.field)
    auto gen_base_expr(const parser::BaseExpr& base) -> std::string;

    // Generate new expression (new ClassName(args))
    auto gen_new_expr(const parser::NewExpr& new_expr) -> std::string;

    // Generate range expression (a..b or a..=b) as Range/RangeInclusive struct
    auto gen_range(const parser::RangeExpr& range) -> std::string;
