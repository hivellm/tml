//! # Type Environment
//!
//! This module defines the type environment used during type checking. The
//! environment tracks all type definitions, variable bindings, and behavior
//! implementations.
//!
//! ## Structure
//!
//! - **TypeEnv**: The global type environment for a module
//! - **Scope**: Nested scopes for local variable bindings
//! - **FuncSig**, **StructDef**, **EnumDef**, **BehaviorDef**: Type definitions
//!
//! ## Type Inference
//!
//! The environment manages type variables and unification. Call `fresh_type_var()`
//! to create an unknown type, `unify()` to add constraints, and `resolve()` to
//! get the final type.
//!
//! ## Module Integration
//!
//! The environment connects to the module registry for import resolution
//! and cross-module symbol lookup.

#ifndef TML_TYPES_ENV_HPP
#define TML_TYPES_ENV_HPP

#include "types/env_stability.hpp"
#include "types/module.hpp"
#include "types/type.hpp"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::types {

/// Information about a bound symbol (variable or parameter).
struct Symbol {
    std::string name; ///< Symbol name.
    TypePtr type;     ///< Symbol type.
    bool is_mutable;  ///< True for mutable bindings.
    SourceSpan span;  ///< Declaration location.
};

/// A where clause constraint: type parameter -> required behaviors.
struct WhereConstraint {
    std::string type_param;                      ///< The constrained type parameter.
    std::vector<std::string> required_behaviors; ///< Required behavior implementations.
};

/// A const generic parameter definition.
struct ConstGenericParam {
    std::string name;   ///< Parameter name (e.g., "N").
    TypePtr value_type; ///< Type of the const (e.g., `U64`).
};

/// Function signature with stability tracking and FFI support.
///
/// Represents a function's type signature including parameters, return type,
/// generic parameters, and metadata like stability level and FFI bindings.
///
/// # FFI Support
///
/// Functions can be marked as external with `@extern` and `@link` decorators:
///
/// ```tml
/// @extern("c")
/// @link("math")
/// func sin(x: F64) -> F64
/// ```
struct FuncSig {
    std::string name;                                    ///< Function name.
    std::vector<TypePtr> params;                         ///< Parameter types in order.
    TypePtr return_type;                                 ///< Return type (Unit if not specified).
    std::vector<std::string> type_params;                ///< Generic type parameter names.
    bool is_async;                                       ///< True for async functions.
    SourceSpan span;                                     ///< Declaration location.
    StabilityLevel stability = StabilityLevel::Unstable; ///< API stability level.
    std::string deprecated_message = {}; ///< Migration guide for deprecated functions.
    std::string since_version = {};      ///< Version when this status was assigned.
    std::vector<WhereConstraint> where_constraints = {}; ///< Generic constraints.
    bool is_lowlevel = false;                            ///< True for C runtime functions.

    // FFI support (@extern and @link decorators)
    std::optional<std::string> extern_abi = std::nullopt;  ///< ABI: "c", "c++", "stdcall", etc.
    std::optional<std::string> extern_name = std::nullopt; ///< External symbol name if different.
    std::vector<std::string> link_libs = {};               ///< Libraries to link.
    std::optional<std::string> ffi_module = std::nullopt;  ///< FFI namespace from `@link`.

    // Const generic parameters (at end to not break existing code using positional init)
    std::vector<ConstGenericParam> const_params = {}; ///< Const generic parameters.

    // Helper methods

    /// Returns true if this is an external (FFI) function.
    [[nodiscard]] bool is_extern() const {
        return extern_abi.has_value();
    }

    /// Returns true if this function has an FFI module namespace.
    [[nodiscard]] bool has_ffi_module() const {
        return ffi_module.has_value();
    }

    /// Returns true if this function is marked `@stable`.
    [[nodiscard]] bool is_stable() const {
        return stability == StabilityLevel::Stable;
    }

    /// Returns true if this function is marked `@deprecated`.
    [[nodiscard]] bool is_deprecated() const {
        return stability == StabilityLevel::Deprecated;
    }

    /// Returns true if this function has default (unstable) stability.
    [[nodiscard]] bool is_unstable() const {
        return stability == StabilityLevel::Unstable;
    }
};

/// Struct type definition.
///
/// Represents a struct declaration with its name, generic parameters,
/// and fields. Used for both nominal structs and tuple structs.
///
/// # Example
///
/// ```tml
/// struct Point[T] {
///     x: T,
///     y: T,
/// }
/// ```
struct StructDef {
    std::string name;                                    ///< Struct name.
    std::vector<std::string> type_params;                ///< Generic type parameter names.
    std::vector<ConstGenericParam> const_params;         ///< Const generic parameters.
    std::vector<std::pair<std::string, TypePtr>> fields; ///< Field name-type pairs.
    SourceSpan span;                                     ///< Declaration location.
};

/// Enum (algebraic data type) definition.
///
/// Represents an enum declaration with its variants. Each variant can
/// carry data (like Rust's enums or ML's sum types).
///
/// # Example
///
/// ```tml
/// enum Maybe[T] {
///     Just(T),
///     Nothing,
/// }
/// ```
struct EnumDef {
    std::string name;                            ///< Enum name.
    std::vector<std::string> type_params;        ///< Generic type parameter names.
    std::vector<ConstGenericParam> const_params; ///< Const generic parameters.
    std::vector<std::pair<std::string, std::vector<TypePtr>>>
        variants;    ///< Variant name and payload types.
    SourceSpan span; ///< Declaration location.
};

/// Associated type declaration in a behavior.
///
/// Associated types allow behaviors to define placeholder types that
/// implementors must specify. Supports GATs (Generic Associated Types)
/// with their own type parameters.
///
/// # Example
///
/// ```tml
/// behavior Iterator {
///     type Item                    // Simple associated type
///     type Mapped[U]               // GAT with type parameter
///     func next(mut ref this) -> Maybe[This::Item]
/// }
/// ```
struct AssociatedTypeDef {
    std::string name;                     ///< Associated type name.
    std::vector<std::string> type_params; ///< GAT type parameters (e.g., `type Item[T]`).
    std::vector<std::string> bounds;      ///< Behavior bounds (e.g., `Item: Clone`).
    std::optional<TypePtr> default_type;  ///< Optional default type.
};

/// Behavior (trait) definition.
///
/// Behaviors define shared interfaces that types can implement. They can
/// have associated types, required methods, default method implementations,
/// and super-behavior requirements.
///
/// # Example
///
/// ```tml
/// behavior Eq {
///     func eq(ref this, other: ref This) -> Bool
///
///     // Default implementation
///     func ne(ref this, other: ref This) -> Bool {
///         not this.eq(other)
///     }
/// }
/// ```
struct BehaviorDef {
    std::string name;                                ///< Behavior name.
    std::vector<std::string> type_params;            ///< Generic type parameter names.
    std::vector<ConstGenericParam> const_params;     ///< Const generic parameters.
    std::vector<AssociatedTypeDef> associated_types; ///< Associated type declarations.
    std::vector<FuncSig> methods;                    ///< Required and default method signatures.
    std::vector<std::string> super_behaviors;        ///< Super-behaviors this extends.
    std::set<std::string> methods_with_defaults;     ///< Methods that have default implementations.
    SourceSpan span;                                 ///< Declaration location.
};

/// Lexical scope for local variable bindings.
///
/// Scopes form a hierarchy where each scope can access its own symbols
/// and those of parent scopes. Used for blocks, functions, and loops.
class Scope {
public:
    /// Creates a root scope with no parent.
    Scope() = default;

    /// Creates a child scope with the given parent.
    explicit Scope(std::shared_ptr<Scope> parent);

    /// Defines a new symbol in this scope.
    void define(const std::string& name, TypePtr type, bool is_mutable, SourceSpan span);

    /// Looks up a symbol in this scope or any parent scope.
    [[nodiscard]] auto lookup(const std::string& name) const -> std::optional<Symbol>;

    /// Looks up a symbol only in this scope (not parents).
    [[nodiscard]] auto lookup_local(const std::string& name) const -> std::optional<Symbol>;

    /// Returns the parent scope, or nullptr for root scopes.
    [[nodiscard]] auto parent() const -> std::shared_ptr<Scope>;

    /// Returns all symbols defined in this scope.
    [[nodiscard]] auto symbols() const -> const std::unordered_map<std::string, Symbol>& {
        return symbols_;
    }

private:
    std::unordered_map<std::string, Symbol> symbols_; ///< Symbols in this scope.
    std::shared_ptr<Scope> parent_;                   ///< Parent scope.
};

/// Type environment for semantic analysis.
///
/// The TypeEnv is the central repository for all type information during
/// compilation. It tracks type definitions, manages scopes, performs type
/// inference with unification, and connects to the module system.
///
/// # Usage
///
/// ```cpp
/// TypeEnv env;
/// env.define_struct(my_struct);
/// env.push_scope();
/// // ... type checking ...
/// env.pop_scope();
/// ```
class TypeEnv {
public:
    /// Constructs a type environment with builtin types initialized.
    TypeEnv();

    // ========================================================================
    // Type Definitions
    // ========================================================================

    /// Registers a struct definition.
    void define_struct(StructDef def);

    /// Registers an enum definition.
    void define_enum(EnumDef def);

    /// Registers a behavior definition.
    void define_behavior(BehaviorDef def);

    /// Registers a function signature (supports overloading).
    void define_func(FuncSig sig);

    /// Registers a type alias.
    void define_type_alias(const std::string& name, TypePtr type);

    /// Looks up a struct by name.
    [[nodiscard]] auto lookup_struct(const std::string& name) const -> std::optional<StructDef>;

    /// Looks up an enum by name.
    [[nodiscard]] auto lookup_enum(const std::string& name) const -> std::optional<EnumDef>;

    /// Looks up a behavior by name.
    [[nodiscard]] auto lookup_behavior(const std::string& name) const -> std::optional<BehaviorDef>;

    /// Looks up a function by name (returns first overload).
    [[nodiscard]] auto lookup_func(const std::string& name) const -> std::optional<FuncSig>;

    /// Selects a function overload based on argument types.
    [[nodiscard]] auto lookup_func_overload(const std::string& name,
                                            const std::vector<TypePtr>& arg_types) const
        -> std::optional<FuncSig>;

    /// Returns all overloads for a function name.
    [[nodiscard]] auto get_all_overloads(const std::string& name) const -> std::vector<FuncSig>;

    /// Looks up a type alias by name.
    [[nodiscard]] auto lookup_type_alias(const std::string& name) const -> std::optional<TypePtr>;

    // ========================================================================
    // Behavior Implementation Tracking
    // ========================================================================

    /// Records that a type implements a behavior.
    void register_impl(const std::string& type_name, const std::string& behavior_name);

    /// Returns true if the type implements the behavior.
    [[nodiscard]] bool type_implements(const std::string& type_name,
                                       const std::string& behavior_name) const;

    /// Returns true if the type implements Drop.
    [[nodiscard]] bool type_needs_drop(const std::string& type_name) const;

    /// Returns true if the type implements Drop.
    [[nodiscard]] bool type_needs_drop(const TypePtr& type) const;

    // ========================================================================
    // Definition Enumeration
    // ========================================================================

    /// Returns all registered enums.
    [[nodiscard]] auto all_enums() const -> const std::unordered_map<std::string, EnumDef>&;

    /// Returns all registered structs.
    [[nodiscard]] auto all_structs() const -> const std::unordered_map<std::string, StructDef>&;

    /// Returns all registered behaviors.
    [[nodiscard]] auto all_behaviors() const -> const std::unordered_map<std::string, BehaviorDef>&;

    /// Returns all registered function names.
    [[nodiscard]] auto all_func_names() const -> std::vector<std::string>;

    // ========================================================================
    // Scope Management
    // ========================================================================

    /// Pushes a new child scope.
    void push_scope();

    /// Pops the current scope, returning to the parent.
    void pop_scope();

    /// Returns the current scope.
    [[nodiscard]] auto current_scope() -> std::shared_ptr<Scope>;

    // ========================================================================
    // Type Inference
    // ========================================================================

    /// Creates a fresh type variable for inference.
    [[nodiscard]] auto fresh_type_var() -> TypePtr;

    /// Unifies two types, adding constraints for type variables.
    void unify(TypePtr a, TypePtr b);

    /// Resolves a type by following type variable substitutions.
    [[nodiscard]] auto resolve(TypePtr type) -> TypePtr;

    // ========================================================================
    // Builtin Types
    // ========================================================================

    /// Returns the map of builtin type names to types.
    [[nodiscard]] auto builtin_types() const -> const std::unordered_map<std::string, TypePtr>&;

    // ========================================================================
    // Module System
    // ========================================================================

    /// Sets the module registry for cross-module lookups.
    void set_module_registry(std::shared_ptr<ModuleRegistry> registry);

    /// Sets the current module path being compiled.
    void set_current_module(const std::string& module_path);

    /// Sets the source directory for local module resolution.
    void set_source_directory(const std::string& dir_path);

    /// Returns the module registry.
    [[nodiscard]] auto module_registry() const -> std::shared_ptr<ModuleRegistry>;

    /// Returns the current module path.
    [[nodiscard]] auto current_module() const -> const std::string&;

    /// Returns the source directory.
    [[nodiscard]] auto source_directory() const -> const std::string&;

    // ========================================================================
    // Import Management
    // ========================================================================

    /// Imports a symbol from another module, optionally with an alias.
    void import_symbol(const std::string& module_path, const std::string& symbol_name,
                       std::optional<std::string> alias = std::nullopt);

    /// Imports all public symbols from a module (`use foo::*`).
    void import_all_from(const std::string& module_path);

    /// Resolves an imported symbol name to its full module path.
    [[nodiscard]] auto resolve_imported_symbol(const std::string& name) const
        -> std::optional<std::string /* full path */>;

    /// Returns all imported symbols.
    [[nodiscard]] auto all_imports() const
        -> const std::unordered_map<std::string, ImportedSymbol>&;

    // ========================================================================
    // Module Lookup
    // ========================================================================

    /// Gets a module by path.
    [[nodiscard]] auto get_module(const std::string& module_path) const -> std::optional<Module>;

    /// Returns all registered modules.
    [[nodiscard]] auto get_all_modules() const -> std::vector<std::pair<std::string, Module>>;

    /// Loads a native (builtin) module on demand.
    bool load_native_module(const std::string& module_path);

    /// Loads and registers a module from a TML source file.
    bool load_module_from_file(const std::string& module_path, const std::string& file_path);

    // ========================================================================
    // Type Utilities
    // ========================================================================

    /// Returns true if two types are structurally equal.
    [[nodiscard]] static bool types_match(const TypePtr& a, const TypePtr& b);

private:
    /// Internal resolve helper with cycle detection.
    [[nodiscard]] auto resolve_impl(TypePtr type, std::unordered_set<uint64_t>& visited) -> TypePtr;

    // Type definition tables
    std::unordered_map<std::string, StructDef> structs_;     ///< Registered structs.
    std::unordered_map<std::string, EnumDef> enums_;         ///< Registered enums.
    std::unordered_map<std::string, BehaviorDef> behaviors_; ///< Registered behaviors.
    std::unordered_map<std::string, std::vector<FuncSig>>
        functions_; ///< Functions (with overloads).
    std::unordered_map<std::string, std::vector<std::string>>
        behavior_impls_;                                    ///< Type -> behaviors.
    std::unordered_map<std::string, TypePtr> type_aliases_; ///< Type aliases.
    std::unordered_map<std::string, TypePtr> builtins_;     ///< Builtin types.

    // Scope and inference state
    std::shared_ptr<Scope> current_scope_;                ///< Current lexical scope.
    uint32_t type_var_counter_ = 0;                       ///< Counter for fresh type variables.
    std::unordered_map<uint32_t, TypePtr> substitutions_; ///< Type variable substitutions.

    // Module system
    std::shared_ptr<ModuleRegistry> module_registry_; ///< Registry for all modules.
    std::string current_module_path_;                 ///< Path of module being compiled.
    std::string source_directory_;                    ///< Source directory for local imports.
    std::unordered_map<std::string, ImportedSymbol> imported_symbols_; ///< Imported symbols.
    bool abort_on_module_error_ = true; ///< Abort on module load errors.

    // Builtin initialization
    void init_builtins();            ///< Initialize all builtins.
    void init_builtin_types();       ///< Initialize primitive types.
    void init_builtin_io();          ///< Initialize I/O functions.
    void init_builtin_string();      ///< Initialize string functions.
    void init_builtin_time();        ///< Initialize time functions.
    void init_builtin_mem();         ///< Initialize memory functions.
    void init_builtin_atomic();      ///< Initialize atomic operations.
    void init_builtin_sync();        ///< Initialize synchronization primitives.
    void init_builtin_math();        ///< Initialize math functions.
    void init_builtin_collections(); ///< Initialize collection types.
    void init_builtin_async();       ///< Initialize async runtime.
};

} // namespace tml::types

#endif // TML_TYPES_ENV_HPP
