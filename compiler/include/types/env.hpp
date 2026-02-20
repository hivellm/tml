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

/// A single behavior bound with optional type parameters.
///
/// Represents a constraint like `FromIterator[T]` where `T` is a type argument.
/// For simple bounds like `Clone`, the `type_args` vector is empty.
struct BoundConstraint {
    std::string behavior_name;      ///< Behavior name (e.g., "FromIterator").
    std::vector<TypePtr> type_args; ///< Type arguments (e.g., [T] in FromIterator[T]).
};

/// A higher-ranked behavior bound: `for[T] Fn(T) -> T`
///
/// Represents universally quantified bounds where the bound type parameters
/// are scoped to the constraint itself, not the enclosing function.
struct HigherRankedBound {
    std::vector<std::string> bound_type_params; ///< The `for[T]` params.
    std::string behavior_name;                  ///< e.g., "Fn".
    std::vector<TypePtr> type_args;             ///< e.g., [T, T] for Fn(T) -> T.
};

/// A where clause constraint: type parameter -> required behaviors.
struct WhereConstraint {
    std::string type_param;                      ///< The constrained type parameter.
    std::vector<std::string> required_behaviors; ///< Required behavior implementations (simple).
    std::vector<BoundConstraint>
        parameterized_bounds; ///< Parameterized bounds (e.g., FromIterator[T]).
    std::vector<HigherRankedBound>
        higher_ranked_bounds; ///< Higher-ranked bounds (e.g., for[T] Fn(T) -> T).
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
    bool is_intrinsic = false;                           ///< True for @intrinsic compiler builtins.

    // FFI support (@extern and @link decorators)
    std::optional<std::string> extern_abi = std::nullopt;  ///< ABI: "c", "c++", "stdcall", etc.
    std::optional<std::string> extern_name = std::nullopt; ///< External symbol name if different.
    std::vector<std::string> link_libs = {};               ///< Libraries to link.
    std::optional<std::string> ffi_module = std::nullopt;  ///< FFI namespace from `@link`.

    // Const generic parameters (at end to not break existing code using positional init)
    std::vector<ConstGenericParam> const_params = {}; ///< Const generic parameters.

    // Lifetime bounds for type parameters (e.g., "T" -> "static" for [T: life static])
    std::unordered_map<std::string, std::string> lifetime_bounds = {};

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
/// A single field in a struct definition.
struct StructFieldDef {
    std::string name;         ///< Field name.
    TypePtr type;             ///< Field type.
    bool has_default = false; ///< True if this field has a default value.
};

struct StructDef {
    std::string name;                            ///< Struct name.
    std::vector<std::string> type_params;        ///< Generic type parameter names.
    std::vector<ConstGenericParam> const_params; ///< Const generic parameters.
    std::vector<StructFieldDef> fields;          ///< Field definitions with optional defaults.
    SourceSpan span;                             ///< Declaration location.

    /// Whether this type has interior mutability.
    ///
    /// Interior mutable types (like Cell[T], Mutex[T]) allow mutation through
    /// shared references. This bypasses normal borrow checking rules but is
    /// safe because the type itself enforces thread-safety or single-threaded
    /// access patterns.
    ///
    /// Types can be marked interior mutable with the `@interior_mutable` decorator.
    bool is_interior_mutable = false;

    /// Whether this is a C-style union rather than a struct.
    ///
    /// Unions have all fields sharing the same memory location. Only one field
    /// can be meaningfully accessed at a time. Field access is `lowlevel` (unsafe)
    /// as there's no runtime type checking.
    bool is_union = false;
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

// ============================================================================
// OOP Definitions (C#-style)
// ============================================================================

/// Member visibility for class/interface members.
enum class MemberVisibility {
    Private,   ///< `private` - only accessible within this class.
    Protected, ///< `protected` - accessible within class and subclasses.
    Public,    ///< `pub` - accessible everywhere.
};

/// Class field definition.
struct ClassFieldDef {
    std::string name;                 ///< Field name.
    TypePtr type;                     ///< Field type.
    MemberVisibility vis;             ///< Field visibility.
    bool is_static;                   ///< True for static fields.
    std::optional<TypePtr> init_type; ///< Type of initializer expression (if any).
};

/// Class method definition.
struct ClassMethodDef {
    FuncSig sig;          ///< Method signature.
    MemberVisibility vis; ///< Method visibility.
    bool is_static;       ///< True for static methods.
    bool is_virtual;      ///< True for virtual methods.
    bool is_override;     ///< True for override methods.
    bool is_abstract;     ///< True for abstract methods.
    bool is_final;        ///< True for final methods (cannot be overridden).
    size_t vtable_index;  ///< Index in vtable (for virtual methods).
};

/// Property definition with optional getter/setter.
struct PropertyDef {
    std::string name;     ///< Property name.
    TypePtr type;         ///< Property type.
    MemberVisibility vis; ///< Property visibility.
    bool is_static;       ///< True for static properties.
    bool has_getter;      ///< True if has getter.
    bool has_setter;      ///< True if has setter.
};

/// Constructor definition.
struct ConstructorDef {
    std::vector<TypePtr> params; ///< Parameter types.
    MemberVisibility vis;        ///< Constructor visibility.
    bool calls_base;             ///< True if calls base constructor.
};

/// Class (OOP) definition.
///
/// Represents a class declaration with single inheritance, multiple
/// interface implementation, and support for virtual dispatch.
///
/// # Example
///
/// ```tml
/// class Dog extends Animal implements Friendly {
///     private name: Str
///     func new(name: Str) { this.name = name }
///     override func speak(this) -> Str { "Woof!" }
/// }
/// ```
struct ClassDef {
    std::string name;                            ///< Class name.
    std::vector<std::string> type_params;        ///< Generic type parameter names.
    std::vector<ConstGenericParam> const_params; ///< Const generic parameters.
    std::optional<std::string> base_class;       ///< Base class name (single inheritance).
    std::vector<std::string> interfaces;         ///< Implemented interfaces.
    std::vector<ClassFieldDef> fields;           ///< Class fields.
    std::vector<ClassMethodDef> methods;         ///< Class methods.
    std::vector<PropertyDef> properties;         ///< Class properties.
    std::vector<ConstructorDef> constructors;    ///< Constructors.
    bool is_abstract;                            ///< True for abstract classes.
    bool is_sealed;                              ///< True for sealed classes.
    bool is_value;                               ///< True for @value classes (no vtable).
    bool is_pooled;                              ///< True for @pool classes (uses object pool).
    SourceSpan span;                             ///< Declaration location.

    // Stack allocation eligibility metadata
    bool stack_allocatable = false; ///< True if class instances can be stack-allocated.
    size_t estimated_size = 0;      ///< Estimated size in bytes (includes vtable ptr + fields).
    size_t inheritance_depth = 0;   ///< Depth in inheritance hierarchy (0 = no base class).
};

/// Interface method definition.
struct InterfaceMethodDef {
    FuncSig sig;      ///< Method signature.
    bool is_static;   ///< True for static interface methods.
    bool has_default; ///< True if has default implementation.
};

/// Interface (OOP) definition.
///
/// Represents an interface declaration that classes can implement.
/// Supports multiple inheritance (extends).
///
/// # Example
///
/// ```tml
/// interface Drawable {
///     func draw(this, canvas: ref Canvas)
/// }
/// ```
struct InterfaceDef {
    std::string name;                            ///< Interface name.
    std::vector<std::string> type_params;        ///< Generic type parameter names.
    std::vector<ConstGenericParam> const_params; ///< Const generic parameters.
    std::vector<std::string> extends;            ///< Extended interfaces.
    std::vector<InterfaceMethodDef> methods;     ///< Interface methods.
    SourceSpan span;                             ///< Declaration location.
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

    /// Registers a type alias (with optional generic parameter names).
    void define_type_alias(const std::string& name, TypePtr type,
                           std::vector<std::string> generic_params = {});

    /// Looks up a struct by name.
    [[nodiscard]] auto lookup_struct(const std::string& name) const -> std::optional<StructDef>;

    /// Looks up an enum by name.
    [[nodiscard]] auto lookup_enum(const std::string& name) const -> std::optional<EnumDef>;

    /// Looks up a behavior by name.
    [[nodiscard]] auto lookup_behavior(const std::string& name) const -> std::optional<BehaviorDef>;

    /// Returns a read-only reference to all registered behaviors.
    [[nodiscard]] auto get_behavior_list() const
        -> const std::unordered_map<std::string, BehaviorDef>* {
        return &behaviors_;
    }

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

    /// Looks up generic parameter names for a type alias.
    [[nodiscard]] auto lookup_type_alias_generics(const std::string& name) const
        -> std::optional<std::vector<std::string>>;

    // ========================================================================
    // OOP Type Definitions (C#-style)
    // ========================================================================

    /// Registers a class definition.
    void define_class(ClassDef def);

    /// Registers an interface definition.
    void define_interface(InterfaceDef def);

    /// Looks up a class by name.
    [[nodiscard]] auto lookup_class(const std::string& name) const -> std::optional<ClassDef>;

    /// Looks up an interface by name.
    [[nodiscard]] auto lookup_interface(const std::string& name) const
        -> std::optional<InterfaceDef>;

    /// Returns all registered classes.
    [[nodiscard]] auto all_classes() const -> const std::unordered_map<std::string, ClassDef>&;

    /// Returns all registered interfaces.
    [[nodiscard]] auto all_interfaces() const
        -> const std::unordered_map<std::string, InterfaceDef>&;

    /// Records that a class implements an interface.
    void register_class_interface(const std::string& class_name, const std::string& interface_name);

    /// Returns true if a class implements an interface.
    [[nodiscard]] bool class_implements_interface(const std::string& class_name,
                                                  const std::string& interface_name) const;

    /// Returns true if a class is a subclass of another class.
    [[nodiscard]] bool is_subclass_of(const std::string& derived, const std::string& base) const;

    // ========================================================================
    // Behavior Implementation Tracking
    // ========================================================================

    /// Records that a type implements a behavior.
    void register_impl(const std::string& type_name, const std::string& behavior_name);

    /// Returns true if the type implements the behavior.
    [[nodiscard]] bool type_implements(const std::string& type_name,
                                       const std::string& behavior_name) const;

    /// Returns true if the type implements the behavior (TypePtr overload).
    /// This overload handles special cases like closures implementing Fn traits.
    [[nodiscard]] bool type_implements(const TypePtr& type, const std::string& behavior_name) const;

    /// Returns true if the type implements Drop.
    [[nodiscard]] bool type_needs_drop(const std::string& type_name) const;

    /// Returns true if the type implements Drop.
    [[nodiscard]] bool type_needs_drop(const TypePtr& type) const;

    /// Returns true if the type is trivially destructible.
    /// A type is trivially destructible if:
    /// - It doesn't implement a custom Drop
    /// - All its fields (if any) are trivially destructible
    /// This allows eliding destructor calls for such types.
    [[nodiscard]] bool is_trivially_destructible(const std::string& type_name) const;

    /// Returns true if the type is trivially destructible (TypePtr overload).
    [[nodiscard]] bool is_trivially_destructible(const TypePtr& type) const;

    /// Returns true if the type has interior mutability.
    ///
    /// Interior mutable types allow mutation through shared references.
    /// This includes:
    /// - Types marked with `@interior_mutable` decorator
    /// - Built-in types: Cell[T], Mutex[T], Shared[T], Sync[T]
    ///
    /// @param type_name The name of the type to check
    /// @return true if the type has interior mutability
    [[nodiscard]] bool is_interior_mutable(const std::string& type_name) const;

    /// Returns true if the type has interior mutability (TypePtr overload).
    [[nodiscard]] bool is_interior_mutable(const TypePtr& type) const;

    /// Returns true if a class can be treated as a value class (no vtable needed).
    ///
    /// A class is a value class candidate if:
    /// - It is sealed (no subclasses)
    /// - It has no virtual methods
    /// - It does not extend an abstract class
    /// - Its base class (if any) is also a value class candidate
    ///
    /// Value classes can be optimized by:
    /// - Omitting the vtable pointer
    /// - Using direct method calls instead of virtual dispatch
    [[nodiscard]] bool is_value_class_candidate(const std::string& class_name) const;

    /// Checks if a class can be stack-allocated when the exact type is known.
    /// Unlike is_value_class_candidate(), this allows classes with virtual methods
    /// as long as they are sealed (no subclasses). The vtable pointer is still included.
    ///
    /// This is useful for escape analysis: when we know the exact type at the
    /// allocation site and the object doesn't escape, we can stack-allocate it.
    [[nodiscard]] bool can_stack_allocate_class(const std::string& class_name) const;

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

    /// Sets whether to abort (exit) on module parse errors.
    /// Use false for best-effort pre-loading (e.g., warmup).
    void set_abort_on_module_error(bool abort) {
        abort_on_module_error_ = abort;
    }

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

    /// Checks if a symbol has import conflicts (same local name from different sources).
    [[nodiscard]] auto has_import_conflict(const std::string& name) const -> bool;

    /// Gets the conflicting import sources for a symbol, if any.
    [[nodiscard]] auto get_import_conflict_sources(const std::string& name) const
        -> std::optional<std::set<std::string>>;

    // ========================================================================
    // Module Lookup
    // ========================================================================

    /// Gets a module by path.
    [[nodiscard]] auto get_module(const std::string& module_path) const -> std::optional<Module>;

    /// Returns all registered modules.
    [[nodiscard]] auto get_all_modules() const -> std::vector<std::pair<std::string, Module>>;

    /// Loads a native (builtin) module on demand.
    bool load_native_module(const std::string& module_path, bool silent = false);

    /// Loads and registers a module from a TML source file.
    bool load_module_from_file(const std::string& module_path, const std::string& file_path);

    // ========================================================================
    // Type Utilities
    // ========================================================================

    /// Returns true if two types are structurally equal.
    [[nodiscard]] static bool types_match(const TypePtr& a, const TypePtr& b);

    // ========================================================================
    // Snapshot Support
    // ========================================================================

    /// Creates a snapshot of the current type definitions.
    /// The snapshot contains all registered types, behaviors, and behavior
    /// implementations, but resets per-file state (scope, inference, imports).
    /// Used to avoid re-running init_builtins() for every compilation unit.
    [[nodiscard]] TypeEnv snapshot() const;

private:
    /// Tag type for snapshot constructor.
    struct SnapshotTag {};

    /// Private constructor used by snapshot() - copies type tables, resets per-file state.
    TypeEnv(SnapshotTag, const TypeEnv& source);
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
    std::unordered_map<std::string, std::vector<std::string>>
        type_alias_generics_;                           ///< Generic params for type aliases.
    std::unordered_map<std::string, TypePtr> builtins_; ///< Builtin types.

    // OOP type definition tables (C#-style)
    std::unordered_map<std::string, ClassDef> classes_;        ///< Registered classes.
    std::unordered_map<std::string, InterfaceDef> interfaces_; ///< Registered interfaces.
    std::unordered_map<std::string, std::vector<std::string>>
        class_interfaces_; ///< Class -> implemented interfaces.

    // Scope and inference state
    std::shared_ptr<Scope> current_scope_;                ///< Current lexical scope.
    uint32_t type_var_counter_ = 0;                       ///< Counter for fresh type variables.
    std::unordered_map<uint32_t, TypePtr> substitutions_; ///< Type variable substitutions.

    // Module system
    std::shared_ptr<ModuleRegistry> module_registry_; ///< Registry for all modules.
    std::string current_module_path_;                 ///< Path of module being compiled.
    std::string source_directory_;                    ///< Source directory for local imports.
    std::unordered_map<std::string, ImportedSymbol> imported_symbols_; ///< Imported symbols.
    std::unordered_map<std::string, std::set<std::string>>
        import_conflicts_;              ///< Tracks import name conflicts for error reporting.
    bool abort_on_module_error_ = true; ///< Abort on module load errors.
    std::unordered_set<std::string>
        loading_modules_; ///< Modules currently being loaded (cycle detection).

    // Builtin initialization
    void init_builtins();      ///< Initialize all builtins.
    void init_builtin_types(); ///< Initialize primitive types.
    void init_builtin_io();    ///< Initialize I/O functions.
    // init_builtin_string removed (Phase 29) — 29 dead FuncSig entries
    // init_builtin_time removed (Phase 39) — 8 dead FuncSig entries
    void init_builtin_mem();    ///< Initialize memory functions.
    void init_builtin_atomic(); ///< Initialize atomic operations.
    void init_builtin_sync();   ///< Initialize synchronization primitives.
    void init_builtin_math();   ///< Initialize math functions.
    void init_builtin_async();  ///< Initialize async runtime.
};

} // namespace tml::types

#endif // TML_TYPES_ENV_HPP
