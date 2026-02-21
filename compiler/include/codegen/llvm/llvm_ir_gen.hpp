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

    // Current function context
    std::string current_func_;
    std::string current_ret_type_;        // Return type of current function
    bool current_func_is_async_ = false;  // Whether current function is async
    std::string current_poll_type_;       // Poll[T] type for async functions
    std::string current_poll_inner_type_; // Inner T type for Poll[T] in async functions

    // Inline closure return redirect: when set, `return` inside an inlined
    // closure body stores the value into this alloca and branches to the
    // end label instead of emitting a function-level `ret`.
    std::string closure_return_alloca_; // alloca for closure return value (empty = disabled)
    std::string closure_return_type_;   // LLVM type of the closure return value
    std::string closure_return_label_;  // label to branch to after storing

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

    // Current where clause constraints (for method dispatch on bounded generics)
    // Used to resolve methods like container.get() when C: Container[T]
    std::vector<types::WhereConstraint> current_where_constraints_;

    // Current module prefix (for generating imported module functions)
    std::string
        current_module_prefix_; // e.g., "algorithms" when generating functions from algorithms.tml

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
    void emit_scope_drops(); // Emit drops for current scope only
    void emit_all_drops();   // Emit drops for all scopes (for return)
    void emit_drop_call(const DropInfo& info);
    void emit_field_level_drops(const DropInfo& info);

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
                                 const parser::FuncDecl& trait_method,
                                 const parser::ImplDecl* impl);

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
        size_t method_count = 0; // Number of instance methods (non-static)
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

    // All struct declarations (for accessing default field values during codegen)
    std::unordered_map<std::string, const parser::StructDecl*> struct_decls_;
    std::unordered_map<std::string, const parser::FuncDecl*> pending_generic_funcs_;
    std::unordered_map<std::string, const parser::ClassDecl*> pending_generic_classes_;

    // Pending generic impl blocks (type_name -> impl block pointer)
    // These are registered and methods are instantiated when called on concrete types
    std::unordered_map<std::string, const parser::ImplDecl*> pending_generic_impls_;

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
        std::string submodule_name; // current_submodule_name_ when deferred
    };
    // Key: LLVM function name (e.g., "@tml_RawSocket_close")
    std::unordered_map<std::string, PendingLibraryMethod> pending_library_methods_;

    struct PendingLibraryFunc {
        const parser::FuncDecl* func;
        std::string module_prefix;  // current_module_prefix_ when deferred
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

    /// Emits coverage instrumentation for a function call.
    /// Only emits if coverage_enabled is true. Tracks function calls for coverage reporting.
    void emit_coverage(const std::string& func_name);

    /// Emits coverage report calls at program exit (print_coverage_report, write_coverage_html).
    /// @param coverage_output_str The string literal for coverage output file (empty if not set).
    /// @param check_quiet If true, only emits if coverage_quiet is false (suite mode suppression).
    void emit_coverage_report_calls(const std::string& coverage_output_str,
                                    bool check_quiet = false);

    /// Returns suite prefix (e.g., "s0_") when in suite mode, empty string otherwise.
    /// Used to avoid symbol collisions when multiple test files are linked into one DLL.
    auto get_suite_prefix() const -> std::string;

    /// Returns true if type_name::method is found in the module registry (library method).
    /// Used to avoid adding suite prefix to library method calls.
    auto is_library_method(const std::string& type_name, const std::string& method) const -> bool;

    // Type translation
    auto llvm_type(const parser::Type& type) -> std::string;
    auto llvm_type_ptr(const parser::TypePtr& type) -> std::string;
    auto llvm_type_name(const std::string& name) -> std::string;
    // for_data=true: use "{}" for Unit (when used as data field), false: use "void" (for return
    // types)
    auto llvm_type_from_semantic(const types::TypePtr& type, bool for_data = false) -> std::string;
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
    void pre_register_func(const parser::FuncDecl& func); // Pre-register without generating code
    void gen_impl_method(const std::string& type_name, const parser::FuncDecl& method);
    void gen_impl_method_instantiation(
        const std::string& mangled_type_name, const parser::FuncDecl& method,
        const std::unordered_map<std::string, types::TypePtr>& type_subs,
        const std::vector<parser::GenericParam>& impl_generics,
        const std::string& method_type_suffix = "", bool is_library_type = false,
        const std::string& base_type_name = "");
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
    auto try_gen_simd_vector_op(const parser::StructExpr& s, const SimdTypeInfo& info)
        -> std::string;
    auto gen_field(const parser::FieldExpr& field) -> std::string;
    auto gen_array(const parser::ArrayExpr& arr) -> std::string;
    auto gen_index(const parser::IndexExpr& idx) -> std::string;
    auto gen_path(const parser::PathExpr& path) -> std::string;
    auto gen_method_call(const parser::MethodCallExpr& call) -> std::string;

    // Method call helpers - split into separate files for maintainability
    auto gen_static_method_call(const parser::MethodCallExpr& call, const std::string& type_name)
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
                                  const std::string& receiver_ptr, types::TypePtr receiver_type)
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
    auto try_gen_intrinsic_extended(const std::string& intrinsic_name, const parser::CallExpr& call,
                                    const std::string& fn_name) -> std::optional<std::string>;

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
    // Continuation of infer_expr_type for method calls, tuples, arrays, index, cast
    auto infer_expr_type_continued(const parser::Expr& expr) -> types::TypePtr;

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
