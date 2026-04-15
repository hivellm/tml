//! # LLVM IR Code Generator
//!
//! This module is the primary code generator for TML, producing LLVM IR
//! text format (`.ll` files) from the typed AST. The IR is then compiled
//! to native code using LLVM's toolchain.
//!
//! ## Features
//!
//! - Full AST-to-LLVM IR translation
//! - Generic instantiation and monomorphization
//! - Trait object vtable generation
//! - Closure capture and environment management
//! - DWARF debug information generation
//! - Code coverage instrumentation
//! - FFI support with `@extern` and `@link`
//!
//! ## Architecture
//!
//! The generator maintains several internal registries:
//!
//! - **locals_**: Variable bindings in current scope
//! - **struct_types_**: Registered struct LLVM types
//! - **functions_**: Function signatures for call resolution
//! - **vtables_**: Behavior implementation vtables
//! - **pending_generic_***: Deferred generic instantiations
//!
//! ## Usage
//!
//! ```cpp
//! LLVMIRGen gen(type_env, options);
//! auto result = gen.generate(module);
//! if (result.is_ok()) {
//!     std::string llvm_ir = result.value();
//! }
//! ```

#pragma once

#include "common.hpp"
#include "parser/ast.hpp"
#include "types/checker.hpp"

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::codegen {

// ============================================================================
// Global AST Cache
// ============================================================================
// Thread-safe global cache for pre-parsed library module ASTs.
// This cache persists across all test file compilations to avoid re-parsing
// the same library modules during codegen for every test file.

/// Global cache for pre-parsed module ASTs.
/// Thread-safe singleton that stores parser::Module structs for library modules.
class GlobalASTCache {
public:
    /// Get the singleton instance.
    static GlobalASTCache& instance();

    /// Check if a module AST is cached.
    bool has(const std::string& module_path) const;

    /// Get a cached module AST (returns nullptr if not cached).
    /// The returned pointer is valid for the lifetime of the cache.
    const parser::Module* get(const std::string& module_path) const;

    /// Cache a module AST (only caches library modules: core::*, std::*, test).
    /// Takes ownership of the module via move.
    void put(const std::string& module_path, parser::Module module);

    /// Clear the cache.
    void clear();

    /// Get cache statistics.
    struct Stats {
        size_t total_entries = 0;
        size_t cache_hits = 0;
        size_t cache_misses = 0;
    };
    Stats get_stats() const;

    /// Check if a module path should be cached (library modules only).
    static bool should_cache(const std::string& module_path);

    /// Get all cached modules (for searching by struct name).
    const std::unordered_map<std::string, parser::Module>& get_all() const;

private:
    GlobalASTCache() = default;
    ~GlobalASTCache() = default;

    // Non-copyable
    GlobalASTCache(const GlobalASTCache&) = delete;
    GlobalASTCache& operator=(const GlobalASTCache&) = delete;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, parser::Module> cache_;
    mutable std::atomic<size_t> hits_ = 0;
    mutable std::atomic<size_t> misses_ = 0;
};

// ============================================================================
// Global Library IR Cache
// ============================================================================
// Thread-safe global cache for pre-generated library LLVM IR.
// This cache persists across all test file compilations in a suite to avoid
// regenerating the same library definitions for every test file.
//
// Caches:
// - Struct type definitions (e.g., %struct.List__I32 = type { ... })
// - Enum type definitions (e.g., %struct.Maybe__I32 = type { ... })
// - Function implementations (library functions)
// - Impl method implementations (e.g., tml_I32_try_from__I64)
// - Generic instantiations (e.g., List[I32], HashMap[Str, I64])
//
// Usage:
// 1. Before compiling test files, optionally pre-load common library IR
// 2. When compiling a test file, check cache before generating
// 3. If cached, emit declaration only; cache provides implementation
// 4. At suite end, emit a single file with all cached implementations

/// Type of cached IR entry.
enum class CachedIRType {
    StructDef,   ///< Struct type definition
    EnumDef,     ///< Enum type definition
    Function,    ///< Function implementation
    ImplMethod,  ///< Impl method (behavior implementation)
    GenericInst, ///< Generic type instantiation
};

/// Cached IR entry information.
struct CachedIREntry {
    std::string key;                       ///< Unique key (e.g., "tml_I32_try_from__I64")
    CachedIRType type;                     ///< Type of entry
    std::string declaration;               ///< LLVM IR declaration (for extern refs)
    std::string type_definition;           ///< LLVM IR type definition (for structs/enums)
    std::string implementation;            ///< Full LLVM IR implementation (for functions)
    bool is_library;                       ///< True if from library (no suite prefix)
    std::vector<std::string> dependencies; ///< Other entries this depends on
};

/// Global cache for pre-generated library LLVM IR.
/// Thread-safe singleton that stores library IR for reuse across test files.
class GlobalLibraryIRCache {
public:
    /// Get the singleton instance.
    static GlobalLibraryIRCache& instance();

    /// Check if an entry is cached.
    bool has(const std::string& key) const;

    /// Get a cached entry (returns nullptr if not cached).
    const CachedIREntry* get(const std::string& key) const;

    /// Cache an IR entry.
    void put(const std::string& key, CachedIREntry entry);

    /// Get all cached entries of a specific type.
    std::vector<const CachedIREntry*> get_by_type(CachedIRType type) const;

    /// Get all cached entries (for emitting shared library file).
    std::vector<const CachedIREntry*> get_all() const;

    /// Clear the cache (e.g., for --no-cache flag or between suites).
    void clear();

    /// Get cache statistics.
    struct Stats {
        size_t total_entries = 0;
        size_t struct_defs = 0;
        size_t enum_defs = 0;
        size_t functions = 0;
        size_t impl_methods = 0;
        size_t generic_insts = 0;
        size_t cache_hits = 0;
        size_t cache_misses = 0;
    };
    Stats get_stats() const;

    /// Mark an entry as "in progress" to avoid duplicate generation in parallel.
    /// Returns true if this thread should generate it, false if another thread is.
    bool try_claim(const std::string& key);

    /// Mark an entry as fully generated (release the claim).
    void release_claim(const std::string& key);

    /// Pre-load common library definitions.
    /// This scans library modules and pre-generates common instantiations.
    /// Should be called once before compiling test suites.
    void preload_library_definitions();

private:
    GlobalLibraryIRCache() = default;
    ~GlobalLibraryIRCache() = default;

    // Non-copyable
    GlobalLibraryIRCache(const GlobalLibraryIRCache&) = delete;
    GlobalLibraryIRCache& operator=(const GlobalLibraryIRCache&) = delete;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, CachedIREntry> cache_;
    std::unordered_set<std::string> in_progress_; ///< Entries being generated
    mutable std::atomic<size_t> hits_ = 0;
    mutable std::atomic<size_t> misses_ = 0;
};

/// Error during LLVM IR generation.
struct LLVMGenError {
    std::string message;            ///< Error description.
    SourceSpan span;                ///< Source location.
    std::vector<std::string> notes; ///< Additional context.
    std::string code;               ///< Error code (e.g., "C001"). Empty uses default.
};

/// Captured codegen library state from emit_module_pure_tml_functions().
/// This allows worker threads to skip the expensive library IR generation
/// by restoring pre-computed state from the shared lib codegen pass.
struct CodegenLibraryState {
    // IR text output from library codegen
    std::string imported_func_code;  ///< Full function definitions (for library_ir_only)
    std::string imported_func_decls; ///< Declaration-only IR (for library_decls_only workers)
    std::string imported_type_defs;  ///< Type definition IR text

    // Internal registries populated by the function
    std::unordered_map<std::string, std::string> struct_types;
    std::unordered_set<std::string> union_types;
    std::unordered_map<std::string, int> enum_variants;
    std::unordered_map<std::string, std::pair<std::string, std::string>>
        global_constants; // name -> {value, llvm_type}

    // Struct field info
    struct FieldInfoData {
        std::string name;
        int index;
        std::string llvm_type;
        types::TypePtr semantic_type; ///< Semantic type for proper type inference
    };
    std::unordered_map<std::string, std::vector<FieldInfoData>> struct_fields;

    // Function signatures
    struct FuncInfoData {
        std::string llvm_name;
        std::string llvm_func_type;
        std::string ret_type;
        std::vector<std::string> param_types;
        bool is_extern = false;
    };
    std::unordered_map<std::string, FuncInfoData> functions;

    // Function return types for type inference
    std::unordered_map<std::string, types::TypePtr> func_return_types;

    // Trait/behavior declarations — stored as names only (AST pointers are in GlobalASTCache)
    std::unordered_set<std::string> trait_decl_names;

    // Generated function names (to avoid duplicates)
    std::unordered_set<std::string> generated_functions;

    // String literals collected during library codegen (name -> value)
    std::vector<std::pair<std::string, std::string>> string_literals;

    // External function names declared during library codegen (prevents duplicate declarations)
    std::set<std::string> declared_externals;

    // Class type mapping (class_name -> LLVM type name, e.g. "Exception" -> "%class.Exception")
    std::unordered_map<std::string, std::string> class_types;

    // Class field info (class_name -> field info list)
    struct ClassFieldInfoData {
        std::string name;
        int index;
        std::string llvm_type;
        int vis; // parser::MemberVisibility as int
        bool is_inherited = false;
        struct PathStep {
            std::string class_name;
            int index;
        };
        std::vector<PathStep> inheritance_path;
    };
    std::unordered_map<std::string, std::vector<ClassFieldInfoData>> class_fields;

    // Value classes (classes with @value decorator - no vtable)
    std::unordered_set<std::string> value_classes;

    // Dyn types already emitted (prevents duplicate %dyn.X type definitions)
    std::set<std::string> emitted_dyn_types;

    // Loop optimization metadata (generated by library functions with loops)
    std::vector<std::string> loop_metadata;
    int loop_metadata_counter = 1000;

    // SIMD type info for @simd annotated structs (struct_name -> {elem_type, lane_count})
    struct SimdTypeInfoData {
        std::string element_llvm_type;
        int lane_count;
    };
    std::unordered_map<std::string, SimdTypeInfoData> simd_types;

    // Module paths processed during the bootstrap codegen pass.
    // Used to detect when a test imports a module not covered by the cached state.
    std::unordered_set<std::string> processed_module_paths;

    bool valid = false; ///< True if state has been captured
};

/// Options for LLVM IR generation.
struct LLVMGenOptions {
    bool emit_comments = true;           ///< Include source comments in IR.
    bool coverage_enabled = false;       ///< Inject coverage instrumentation (TML runtime).
    bool coverage_quiet = false;         ///< Suppress coverage console output (suite mode).
    bool llvm_source_coverage = false;   ///< LLVM source-based coverage (instrprof).
    bool dll_export = false;             ///< Add dllexport for Windows DLLs.
    bool emit_debug_info = false;        ///< Generate DWARF debug information.
    bool generate_dll_entry = false;     ///< Generate tml_test_entry (no main).
    bool generate_fuzz_entry = false;    ///< Generate tml_fuzz_target (no main).
    bool force_internal_linkage = false; ///< Force internal linkage (suite mode).
    bool library_decls_only = false;     ///< Only emit declarations for library functions.
    bool library_ir_only = false;        ///< Generate ONLY library IR (no user code).
    bool lazy_library_defs = false;      ///< Defer library definitions, emit only when referenced.
    int debug_level = 2;                 ///< Debug level: 1=minimal, 2=standard, 3=full.
    int suite_test_index = -1;           ///< Suite test index (-1 = tml_test_entry).
    int suite_total_tests = -1;          ///< Total tests in suite (for coverage aggregation).
    std::string target_triple = "x86_64-pc-windows-msvc"; ///< LLVM target triple.
    std::string source_file;                              ///< Source file path for debug info.
    std::string coverage_output_file;                     ///< Coverage output path.

    /// Pre-computed library state to restore instead of calling emit_module_pure_tml_functions().
    /// When set, the generate() function restores this state and skips the expensive codegen.
    std::shared_ptr<const CodegenLibraryState> cached_library_state;
};

/// LLVM IR text generator.
///
/// The primary code generator for TML. Produces LLVM IR in text format
/// (`.ll` files) that can be compiled to native code with `llc` or `clang`.
///
/// Supports full TML feature set including generics, closures, trait objects,
/// async/await, and FFI.
class LLVMIRGen {
public:
    /// Creates an LLVM IR generator with the given type environment.
    explicit LLVMIRGen(const types::TypeEnv& env, LLVMGenOptions options = {});

    /// Generates LLVM IR for a complete module.
    auto generate(const parser::Module& module) -> Result<std::string, std::vector<LLVMGenError>>;

    /// Captures the library state after generate() with library_ir_only=true.
    /// The returned state can be passed to other LLVMIRGen instances via
    /// LLVMGenOptions::cached_library_state to skip emit_module_pure_tml_functions().
    auto capture_library_state(const std::string& full_ir = "",
                               const std::string& preamble_headers = "") const
        -> std::shared_ptr<CodegenLibraryState>;

    /// Returns external libraries to link (from `@link` decorators).
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

    // Cached library IR text (saved during generate() for capture_library_state())
    std::string cached_imported_func_code_;
    std::string cached_imported_type_defs_;
    std::string cached_preamble_headers_; ///< Preamble IR (for filtering declarations)

    // Module paths processed during emit_module_pure_tml_functions().
    // Used by capture_library_state() to record which modules are covered by the cached state.
    std::unordered_set<std::string> processed_module_paths_;

    // ======== Dead Declaration Elimination (Phase 3) ========
    // Runtime declarations are registered in a catalog during init, then only
    // declarations actually referenced during codegen are emitted into the final IR.
    struct RuntimeDecl {
        std::string name;              ///< Symbol name (e.g., "printf")
        std::string ir_text;           ///< Full IR text (may be multi-line for define)
        std::vector<std::string> deps; ///< Dependencies (other catalog entries needed)
    };
    std::vector<RuntimeDecl> runtime_catalog_;
    std::unordered_map<std::string, size_t> runtime_catalog_index_;
    std::unordered_set<std::string> needed_runtime_decls_;
    std::string deferred_runtime_decls_; ///< Built by finalize_runtime_decls()

    // Current function context
    std::string current_func_;
    std::string
        current_ret_type_;      // Return type of current function (may be temporarily overridden)
    std::string func_ret_type_; // True function return type (never overridden by let hints)
    bool current_func_is_async_ = false;  // Whether current function is async
    std::string current_poll_type_;       // Poll[T] type for async functions
    std::string current_poll_inner_type_; // Inner T type for Poll[T] in async functions

    // Inline closure return redirect: when set, `return` inside an inlined
    // closure body stores the value into this alloca and branches to the
    // end label instead of emitting a function-level `ret`.
    std::string closure_return_alloca_; // alloca for closure return value (empty = disabled)
    std::string closure_return_type_;   // LLVM type of the closure return value
    std::string closure_return_label_;  // label to branch to after storing

    // When true, gen_block treats the last ExprStmt as a trailing expression
    // (preserving its value). Used by closure codegen for implicit return.
    bool closure_wants_implicit_return_ = false;

    // Precomputed iterator value for IntoIterator dispatch (set by gen_for)
    std::string precomputed_iter_val_;
    std::string precomputed_iter_type_;
    bool use_precomputed_iter_ = false;

    // Current namespace context for qualified names
    std::vector<std::string> current_namespace_;
    auto qualified_name(const std::string& name) const -> std::string;
    std::string current_block_;
    bool block_terminated_ = false;

    // Current impl self type (for resolving 'this' in impl methods)
    std::string current_impl_type_; // e.g., "Counter" when in impl Describable for Counter

    // Current associated type bindings (for resolving This::Item in impl blocks)
    // Maps associated type names to their concrete types (e.g., "Item" -> I32)
    std::unordered_map<std::string, types::TypePtr> current_associated_types_;

    // Persistent per-type associated type registry (populated from concrete impl blocks)
    // Maps "TypeName::AssocName" to the resolved type (e.g., "Counter::Item" -> I32)
    // Unlike current_associated_types_ which is scope-local and can be overwritten,
    // this map persists across all impl blocks and allows lookup by type.
    std::unordered_map<std::string, types::TypePtr> type_associated_types_;

    // Current generic type parameter substitutions (for resolving T in impl[T] blocks)
    // Maps type parameter names to their concrete types (e.g., "T" -> I64)
    std::unordered_map<std::string, types::TypePtr> current_type_subs_;

    // Current const generic parameter values (for resolving N in [T; N] array types)
    // Maps const param names to their concrete integer values (e.g., "N" -> 3)
    std::unordered_map<std::string, int64_t> current_const_generic_values_;

    // Current where clause constraints (for method dispatch on bounded generics)
    // Used to resolve methods like container.get() when C: Container[T]
    std::vector<types::WhereConstraint> current_where_constraints_;

    // Current module prefix (for generating imported module functions)
    std::string
        current_module_prefix_; // e.g., "algorithms" when generating functions from algorithms.tml

    // Current module name with :: separators (for hierarchical name mangling)
    // e.g., "core::str" — matches the module registry key, set alongside current_module_prefix_
    std::string current_module_name_; // e.g., "core::str" (with :: separators)

    // Current submodule name (file stem) for cross-module function lookups
    // e.g., "unicode_data" when processing unicode_data.tml within core::unicode module
    std::string current_submodule_name_;

    // Current loop context for break/continue
    std::string current_loop_start_;
    std::string current_loop_end_;
    std::string current_loop_stack_save_; // For stacksave/stackrestore in loops
    int current_loop_metadata_id_ = -1;   // Metadata ID for current loop (-1 = none)

    // Compile-time loop context for field iteration unrolling
    std::string comptime_loop_var_;   // Name of the compile-time loop variable
    std::string comptime_loop_type_;  // Type name for field intrinsics (e.g., "Point")
    int64_t comptime_loop_value_ = 0; // Current iteration value

    // Track last expression type for type-aware codegen
    std::string last_expr_type_ = "i32";
    bool last_expr_is_unsigned_ = false;          // Track if last expression was unsigned type
    bool suppress_mut_ref_auto_deref_ = false;    // Suppress auto-deref in gen_ident for mut ref
    types::TypePtr last_semantic_type_ = nullptr; // Semantic type for deref assignments

    // Expected type context for enum constructors (used in gen_call_expr)
    // When set, enum constructors will use this type instead of inferring
    std::string expected_enum_type_; // e.g., "%struct.Outcome__I32__I32"

    // Expected type context for numeric literals (used in gen_literal)
    // When set, unsuffixed literals use this type instead of defaulting to i32
    // e.g., "i8" for U8, "i16" for I16, etc.
    std::string expected_literal_type_;
    bool expected_literal_is_unsigned_ = false;

public:
    /// Information about captured variables in a closure.
    struct ClosureCaptureInfo {
        std::vector<std::string> captured_names; ///< Names of captured variables.
        std::vector<std::string> captured_types; ///< LLVM types of captured variables.
    };

    /// Variable binding information.
    ///
    /// Tracks the LLVM register, type, and semantic type for each variable
    /// in scope. Used for variable lookup during code generation.
    struct VarInfo {
        std::string reg;              ///< LLVM register holding the value.
        std::string type;             ///< LLVM type string.
        types::TypePtr semantic_type; ///< Full semantic type (for complex types).
        std::optional<ClosureCaptureInfo> closure_captures; ///< Capture info if closure.
        bool is_ptr_to_value = false; ///< True if reg is a pointer to the value (needs loading).
        bool is_direct_param = false; ///< True if reg is a direct parameter (not an alloca).
        bool is_capturing_closure =
            false; ///< True if this is a capturing closure (fat ptr with env).
    };

    /// Drop tracking information for RAII.
    ///
    /// Tracks variables that need `drop()` called when their scope exits.
    /// Used to implement automatic resource cleanup.
    struct DropInfo {
        std::string var_name;           ///< Variable name.
        std::string var_reg;            ///< LLVM register for the value.
        std::string type_name;          ///< TML type name (e.g., "DroppableResource").
        std::string llvm_type;          ///< LLVM type (e.g., "%struct.DroppableResource").
        bool is_heap_str = false;       ///< True if this is a heap-allocated Str needing free().
        bool needs_field_drops = false; ///< True if type needs recursive field-level drops.
        bool needs_enum_drop = false;   ///< True if this is an enum with droppable variant fields.
    };

private:
    std::unordered_map<std::string, VarInfo> locals_;

    // Drop scope tracking for RAII
    // Each scope level contains variables that need drop() called when scope exits
    std::vector<std::vector<DropInfo>> drop_scopes_;

    // Track variables that have been consumed (moved into struct fields, function args, etc.)
    // These should not be dropped when going out of scope
    std::unordered_set<std::string> consumed_vars_;

    // Mark a variable as consumed (moved)
    void mark_var_consumed(const std::string& var_name);

    // Mark a specific field of a variable as consumed (partial move)
    void mark_field_consumed(const std::string& var_name, const std::string& field_name);

    // Check if any field of this variable has been consumed (partial move)
    [[nodiscard]] bool has_consumed_fields(const std::string& var_name) const;

    // Drop scope management
    void push_drop_scope();
    void pop_drop_scope();
    void register_for_drop(const std::string& var_name, const std::string& var_reg,
                           const std::string& type_name, const std::string& llvm_type);
    void register_heap_str_for_drop(const std::string& var_name, const std::string& var_reg);
    bool is_heap_str_producer(const parser::Expr& expr) const;
    void emit_scope_drops(); // Emit drops for current scope only
    void emit_all_drops();   // Emit drops for all scopes (for return)
    void emit_drop_call(const DropInfo& info);
    void emit_field_level_drops(const DropInfo& info);
    void emit_partial_field_drops(const DropInfo& info);
    void emit_enum_variant_drops(const DropInfo& info);
    void ensure_enum_drop_function(const std::string& enum_type_name);
    std::stringstream enum_drop_output_;
    std::unordered_set<std::string> generated_enum_drop_functions_;

    // Temporary value drop tracking
    // Tracks droppable values from function/method returns that aren't bound to variables.
    // These are dropped at the end of the enclosing expression statement.
    std::vector<DropInfo> temp_drops_;
    // Register a temporary value for drop. If existing_alloca is non-empty, uses it
    // instead of creating a new alloca (avoids redundant spills when method dispatch
    // already spilled the receiver to stack).
    std::string register_temp_for_drop(const std::string& value, const std::string& type_name,
                                       const std::string& llvm_type,
                                       const std::string& existing_alloca = "");
    void emit_temp_drops();

    // Str temporary tracking (Phase 4b)
    // Tracks heap-allocated Str values from call/method/binary/interpolated expressions.
    // These are freed at statement end via tml_str_free, unless consumed by a let/var binding.
    std::vector<std::string> pending_str_temps_;
    void flush_str_temps();       // Free all pending Str temps
    void consume_last_str_temp(); // Remove last temp (consumed by let/var/assign)
    void
    consume_str_temp_if_arg(const std::string& reg); // Remove specific temp (passed as call arg)

    // @allocates decorator tracking
    // Functions/methods marked @allocates are known to return freshly heap-allocated values.
    // Used by Phase 4b to auto-free Str temporaries at statement end.
    std::unordered_set<std::string> allocating_functions_;

    // Library body context flag — when true, Phase 4b Str temp tracking is disabled.
    // Library functions manage their own allocations (e.g., split() stores substring()
    // results in a List). Auto-freeing those temps causes use-after-free.
    bool in_library_body_ = false;

    // Type mapping
    std::unordered_map<std::string, std::string> struct_types_;
    std::unordered_set<std::string> union_types_; // Track which types are unions (for field access)
    std::unordered_set<std::string> not_found_struct_types_; // Negative cache for struct lookups

    // SIMD vector type info — @simd annotated structs use LLVM vector types (<N x T>)
    struct SimdTypeInfo {
        std::string element_llvm_type; // "i32", "float", "i8", etc.
        int lane_count;                // 4, 2, 16
    };
    std::unordered_map<std::string, SimdTypeInfo> simd_types_;
    bool is_simd_type(const std::string& struct_name) const {
        return simd_types_.find(struct_name) != simd_types_.end();
    }
    std::string simd_vec_type_str(const SimdTypeInfo& info) const {
        return "<" + std::to_string(info.lane_count) + " x " + info.element_llvm_type + ">";
    }

    // Enum variant values (EnumName::VariantName -> tag value)
    std::unordered_map<std::string, int> enum_variants_;

    // Enum payload type info for compact layout optimization
    // Maps enum LLVM type name (e.g. "%struct.Maybe__I32") to payload field type
    // "i32" = compact 4-byte, "i64" = compact 8-byte, "" = uses [N x i64] union
    std::unordered_map<std::string, std::string> enum_payload_type_;

    // Nullable Maybe optimization: set of mangled type names (e.g. "Maybe__ref_I32")
    // where Maybe[ptr-type] is represented as bare ptr (null = Nothing)
    std::unordered_set<std::string> nullable_maybe_types_;

    // @flags enum metadata
    struct FlagsEnumInfo {
        std::string underlying_llvm_type; ///< "i8", "i16", "i32", "i64"
        uint64_t all_bits_mask;           ///< OR of all variant values
        std::vector<std::pair<std::string, uint64_t>> variant_values;
    };
    std::unordered_map<std::string, FlagsEnumInfo> flags_enums_;

    /// Generate built-in methods for @flags enums.
    void gen_flags_enum_methods(const parser::EnumDecl& e, const FlagsEnumInfo& info);

    // Struct field info for dynamic field access
    struct FieldInfo {
        std::string name;
        int index;
        std::string llvm_type;
        types::TypePtr
            semantic_type; // Semantic type for proper type inference (especially for ptr fields)
    };
    std::unordered_map<std::string, std::vector<FieldInfo>> struct_fields_; // struct_name -> fields

    // Function registry for first-class functions (name -> LLVM function info)
    struct FuncInfo {
        std::string llvm_name;                // e.g., "@tml_double"
        std::string llvm_func_type;           // e.g., "i32 (i32)"
        std::string ret_type;                 // e.g., "i32" (C ABI type for externs)
        std::vector<std::string> param_types; // e.g., {"i32", "%struct.Layout"}
        bool is_extern = false;               // true for @extern FFI functions
        std::string tml_name;                 // Original TML name for coverage tracking
        bool bool_ret_promoted = false;       // true if Bool return was promoted i1->i32 for C ABI
        bool has_sret = false; // true if return type uses sret (large struct on Win x64)
        std::string sret_type; // original return type for sret (e.g., "%struct.X509Name")
    };
    std::unordered_map<std::string, FuncInfo> functions_;

    // Global constants (name -> {value, llvm_type})
    struct ConstInfo {
        std::string value;     // The constant value as string
        std::string llvm_type; // The LLVM type (e.g., "i32", "i64")
    };
    std::unordered_map<std::string, ConstInfo> global_constants_;

    // FFI support - external libraries to link (from @link decorator)
    std::set<std::string> extern_link_libs_;

    // Closure support
    std::vector<std::string> module_functions_; // Generated closure functions
    uint32_t closure_counter_ = 0;              // For unique closure names
    std::optional<ClosureCaptureInfo>
        last_closure_captures_;              // Legacy: capture info from last gen_closure call
    bool last_closure_is_capturing_ = false; // Whether last closure had captures (fat ptr)

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

    // Vtables already emitted (to prevent duplicates in test suites)
    std::set<std::string> emitted_vtables_;

    // External function declarations already emitted (for default implementations)
    std::set<std::string> declared_externals_;

    // Late-discovered @extern declarations encountered during user-code generation.
    // Keyed by symbol name; value is the full "declare ... @symbol(...)" IR text.
    // These are emitted at module level (not inline) to avoid injecting declarations
    // inside function bodies when using cached library state.
    std::map<std::string, std::string> pending_late_extern_decls_;

    // Register an impl block for vtable generation
    void register_impl(const parser::ImplDecl* impl);

    // Generate all vtables from registered impls
    void emit_vtables();

    // Emit dyn type definition if not already emitted
    void emit_dyn_type(const std::string& behavior_name);

    // Get vtable global name for a type/behavior pair
    auto get_vtable(const std::string& type_name, const std::string& behavior_name) -> std::string;

    // Generate a default behavior method implementation for a given type
    // Returns true if generation succeeded, false if skipped
    bool generate_default_method(const std::string& type_name, const parser::TraitDecl* trait_decl,
                                 const parser::FuncDecl& trait_method, const parser::ImplDecl* impl,
                                 const std::string& method_type_suffix = "");

    // OOP, vtable, and optimization state — see llvm_ir_gen_oop.inc
#include "codegen/llvm/llvm_ir_gen_oop.inc"

    // Generic instantiation, lazy library, loop metadata, alloca hoisting, and lifetime state — see
    // llvm_ir_gen_generics.inc
#include "codegen/llvm/llvm_ir_gen_generics.inc"

    // ============ Debug Info Support ============
    // LLVM debug metadata for DWARF generation
    int debug_metadata_counter_ = 0;          // Counter for unique metadata IDs
    int current_scope_id_ = 0;                // Current debug scope (function or lexical block)
    int current_debug_loc_id_ = 0;            // Current debug location ID for instructions
    int file_id_ = 0;                         // File metadata ID
    int compile_unit_id_ = 0;                 // Compile unit metadata ID
    std::vector<std::string> debug_metadata_; // Pending debug metadata to emit at end
    std::unordered_map<std::string, int> func_debug_scope_; // function name -> scope ID
    std::unordered_map<std::string, int> var_debug_info_;   // var name -> debug info ID
    std::unordered_map<std::string, int> type_debug_info_;  // type name -> debug info ID
    std::vector<int> debug_scope_stack_; // Stack of debug scopes for lexical block nesting

    // Debug info generation helpers
    int fresh_debug_id();
    void emit_debug_info_header();
    void emit_debug_info_footer();
    int create_function_debug_scope(const std::string& func_name, uint32_t line, uint32_t column);
    std::string get_debug_location(uint32_t line, uint32_t column);
    std::string get_debug_loc_suffix(); // Returns ", !dbg !N" if in debug scope, else ""
    int create_debug_location(uint32_t line,
                              uint32_t column); // Create and register a debug location

    /// Create a lexical block scope for nested variable visibility in debugger.
    /// Pushes the current scope onto the stack and sets the new block as current scope.
    int create_lexical_block(uint32_t line, uint32_t column);

    /// Restore the parent scope after exiting a lexical block.
    void pop_debug_scope();

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
    /// Computes the byte size of an LLVM type string using struct_fields_ for recursion.
    /// Used for correct enum payload sizing. Returns 8 for unknown types (conservative).
    auto compute_llvm_type_byte_size(const std::string& ty,
                                     const std::string& self_guard = "") const -> size_t;

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
    void gen_derive_duplicate_instantiation(const std::string& mangled_name);
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
    auto gen_for_iterator_with_value(const parser::ForExpr& for_expr,
                                     const std::string& type_name,
                                     const std::string& precomputed_iter_val,
                                     const std::string& precomputed_iter_type,
                                     const std::vector<types::TypePtr>& collection_type_args)
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
    // try_gen_builtin_time removed (Phase 41) — stub since Phase 25, zero callers
    auto try_gen_builtin_math(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_builtin_string(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_builtin_assert(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_builtin_async(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_intrinsic(const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_intrinsic_slice_simd(const std::string& intrinsic_name, const std::string& fn_name,
                                      const parser::CallExpr& call) -> std::optional<std::string>;
    auto try_gen_simd_vector_intrinsic(const std::string& intrinsic_name,
                                       const std::string& fn_name, const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_simd_sse_intrinsic(const std::string& intrinsic_name, const std::string& fn_name,
                                    const parser::CallExpr& call) -> std::optional<std::string>;
    auto try_gen_simd_avx_intrinsic(const std::string& intrinsic_name, const std::string& fn_name,
                                    const parser::CallExpr& call) -> std::optional<std::string>;
    auto try_gen_intrinsic_extended(const std::string& intrinsic_name, const parser::CallExpr& call,
                                    const std::string& fn_name) -> std::optional<std::string>;
    auto try_gen_intrinsic_extended_reflect(const std::string& intrinsic_name,
                                            const std::string& fn_name,
                                            const parser::CallExpr& call)
        -> std::optional<std::string>;
    auto try_gen_intrinsic_extended_dyncall(const std::string& intrinsic_name,
                                            const std::string& fn_name,
                                            const parser::CallExpr& call)
        -> std::optional<std::string>;

    // Utility
    void report_error(const std::string& msg, const SourceSpan& span);
    void report_error(const std::string& msg, const SourceSpan& span, const std::string& code);

    // Closure fat pointer helpers
    // If last_expr_type_ == "{ ptr, ptr }", extract fn_ptr (index 0) and return it.
    // Also sets last_expr_type_ to "ptr". If not a fat pointer, returns the value unchanged.
    auto coerce_closure_to_fn_ptr(const std::string& val) -> std::string;

    // Struct field access helpers
    auto get_field_index(const std::string& struct_name, const std::string& field_name) -> int;
    auto get_field_type(const std::string& struct_name, const std::string& field_name)
        -> std::string;
    auto get_field_semantic_type(const std::string& struct_name, const std::string& field_name)
        -> types::TypePtr;
    auto get_class_field_info(const std::string& class_name, const std::string& field_name)
        -> std::optional<ClassFieldInfo>;

    // Type inference for generics instantiation
    auto infer_expr_type(const parser::Expr& expr) -> types::TypePtr;
    // Continuation of infer_expr_type for field, block, closure, conditional, call expressions
    auto infer_expr_type_extended(const parser::Expr& expr) -> std::optional<types::TypePtr>;
    // Continuation of infer_expr_type for method calls, tuples, arrays, index, cast
    auto infer_expr_type_continued(const parser::Expr& expr) -> types::TypePtr;
    // Extract a generic parameter from a parser field type by matching against
    // an inferred types::Type. Walks into nested generic args recursively.
    // Also tries unwrapping constructor calls when direct matching fails.
    auto extract_generic_from_type(const parser::TypePtr& field_type,
                                   const std::string& generic_name, const types::TypePtr& arg_type)
        -> types::TypePtr;

    // Deref coercion helpers - for auto-deref on field access
    // Returns the Deref target type for smart pointers like Arc[T], Box[T], etc.
    // Returns nullptr if the type doesn't implement Deref or is not a known smart pointer.
    auto get_deref_target_type(const types::TypePtr& type) -> types::TypePtr;

    // Checks if a struct has a specific field
    auto struct_has_field(const std::string& struct_name, const std::string& field_name) -> bool;

    // String literal handling
    std::vector<std::pair<std::string, std::string>> string_literals_;
    std::unordered_map<std::string, std::string> string_literal_dedup_;
    auto add_string_literal(const std::string& value) -> std::string;

public:
    /// Inferred type for print format specifier selection.
    enum class PrintArgType {
        Int,    ///< 32-bit integer (%d).
        I64,    ///< 64-bit integer (%lld).
        Float,  ///< Floating point (%f).
        Bool,   ///< Boolean (prints "true"/"false").
        Str,    ///< String (%s).
        Unknown ///< Unknown type.
    };

    /// Infers the print type for an expression.
    static PrintArgType infer_print_type(const parser::Expr& expr);
};

} // namespace tml::codegen
