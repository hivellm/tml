//! # Declaration AST Nodes
//!
//! This module defines the AST nodes for declarations (top-level and nested definitions).
//! Declarations introduce named items: functions, types, behaviors, constants, modules.
//!
//! ## Declaration Types
//!
//! - **Functions**: `func`, `async func`, `lowlevel func`
//! - **Types**: `type` (struct/enum), type aliases
//! - **Behaviors**: TML's traits (interfaces for types)
//! - **Implementations**: `impl` blocks
//! - **Constants**: `const`
//! - **Imports**: `use`
//! - **Modules**: `mod`
//!
//! ## Visibility
//!
//! Declarations can have visibility modifiers:
//! - `pub` - public, visible everywhere
//! - `pub(crate)` - visible within the current crate
//! - (default) - private to the current module
//!
//! ## Generics
//!
//! Many declarations support generic parameters with bounds:
//! ```tml
//! func sort[T: Ord](items: mut ref [T])
//! type Pair[A, B] { first: A, second: B }
//! behavior Container[Item] { ... }
//! ```

#ifndef TML_PARSER_AST_DECLS_HPP
#define TML_PARSER_AST_DECLS_HPP

#include "ast_common.hpp"
#include "ast_exprs.hpp"
#include "ast_patterns.hpp"
#include "ast_stmts.hpp"
#include "ast_types.hpp"

namespace tml::parser {

// ============================================================================
// Visibility and Decorators
// ============================================================================

/// Item visibility modifier.
///
/// Controls where a declaration can be accessed from.
enum class Visibility {
    Private,  ///< Default - private to the current module.
    Public,   ///< `pub` - visible everywhere.
    PubCrate, ///< `pub(crate)` - visible within the current crate only.
};

/// A decorator/attribute: `@derive(Clone, Debug)`, `@test`, `@inline`.
///
/// Decorators provide metadata and compiler directives for declarations.
/// TML uses `@name` syntax instead of Rust's `#[name]`.
///
/// # Examples
///
/// ```tml
/// @derive(Debug, Clone)
/// type Point { x: I32, y: I32 }
///
/// @test
/// func test_add() { assert_eq(1 + 1, 2) }
///
/// @inline
/// func hot_path() { ... }
/// ```
struct Decorator {
    std::string name;          ///< Decorator name.
    std::vector<ExprPtr> args; ///< Optional arguments.
    SourceSpan span;           ///< Source location.
};

// ============================================================================
// Generic Parameters and Constraints
// ============================================================================

/// A generic parameter: `T`, `T: Behavior`, `T: Behavior[U]`, `const N: U64`, or `life a`.
///
/// Generic parameters can be:
/// - Type parameters: `T`
/// - Bounded type parameters: `T: Clone + Debug`
/// - Const parameters: `const N: U64`
/// - Defaulted parameters: `T = I32`
/// - Lifetime parameters: `life a`, `life static`
///
/// # Examples
///
/// ```tml
/// func identity[T](x: T) -> T { x }
/// func sort[T: Ord](items: mut ref [T])
/// type Array[T, const N: U64] { ... }
/// type Container[T = I32] { value: T }
/// func longest[life a](x: ref[a] Str, y: ref[a] Str) -> ref[a] Str
/// func static_only[T: life static](x: T) -> T  // Lifetime bound
/// ```
struct GenericParam {
    std::string name; ///< Parameter name.
    std::vector<TypePtr>
        bounds; ///< Behavior bounds (supports parameterized bounds like `Iterator[Item=T]`).
    bool is_const = false;               ///< True for const generic params.
    bool is_lifetime = false;            ///< True for lifetime params (`life a`).
    std::optional<TypePtr> const_type;   ///< Type of const param (e.g., `U64`).
    std::optional<TypePtr> default_type; ///< Default type (e.g., `T = This`).
    std::optional<std::string>
        lifetime_bound; ///< Lifetime bound (e.g., "static" for `T: life static`).
    SourceSpan span;    ///< Source location.
};

/// Where clause: `where T: Clone, U: Hash, T = U`.
///
/// Specifies additional constraints on generic parameters.
/// Supports both behavior bounds and type equalities.
///
/// # Example
///
/// ```tml
/// func process[T, U](a: T, b: U) -> T
/// where
///     T: Clone + Debug,
///     U: Into[T]
/// { ... }
/// ```
struct WhereClause {
    /// Behavior bounds: `T: Behavior1 + Behavior2`.
    std::vector<std::pair<TypePtr, std::vector<TypePtr>>> constraints;
    /// Type equalities: `T = U`.
    std::vector<std::pair<TypePtr, TypePtr>> type_equalities;
    SourceSpan span; ///< Source location.
};

// ============================================================================
// Function Declarations
// ============================================================================

/// A function parameter.
///
/// Parameters have a pattern (usually a simple name) and a type.
struct FuncParam {
    PatternPtr pattern; ///< Parameter pattern (e.g., `x`, `(a, b)`).
    TypePtr type;       ///< Parameter type.
    SourceSpan span;    ///< Source location.
};

/// Function declaration.
///
/// Functions are the primary unit of code in TML. They can be:
/// - Regular functions
/// - Async functions (`async func`)
/// - Unsafe functions (`lowlevel func`)
/// - External functions (`@extern("c")`)
///
/// # Examples
///
/// ```tml
/// func add(a: I32, b: I32) -> I32 { a + b }
///
/// pub async func fetch[T](url: Str) -> Outcome[T, Error] { ... }
///
/// @extern("c") func printf(fmt: *const I8, ...) -> I32
/// ```
struct FuncDecl {
    std::optional<std::string> doc;          ///< Documentation comment (from `///`).
    std::vector<Decorator> decorators;       ///< Decorators.
    Visibility vis;                          ///< Visibility.
    std::string name;                        ///< Function name.
    std::vector<GenericParam> generics;      ///< Generic parameters.
    std::vector<FuncParam> params;           ///< Parameters.
    std::optional<TypePtr> return_type;      ///< Return type (unit if omitted).
    std::optional<WhereClause> where_clause; ///< Where clause.
    std::optional<BlockExpr> body;           ///< Body (none for signatures/extern).
    bool is_async;                           ///< True for `async func`.
    bool is_unsafe;                          ///< True for `lowlevel func`.
    SourceSpan span;                         ///< Source location.

    // FFI support
    std::optional<std::string> extern_abi;  ///< ABI: "c", "c++", etc.
    std::optional<std::string> extern_name; ///< Symbol name if different.
    std::vector<std::string> link_libs;     ///< Libraries to link.
};

// ============================================================================
// Type Declarations
// ============================================================================

/// A struct field.
///
/// Fields can have visibility modifiers, documentation, and default values.
///
/// # Example
///
/// ```tml
/// type Config {
///     name: Str,
///     port: I32 = 8080,      // default value
///     debug: Bool = false,   // default value
/// }
/// ```
struct StructField {
    std::optional<std::string> doc;       ///< Documentation comment (from `///`).
    Visibility vis;                       ///< Field visibility.
    std::string name;                     ///< Field name.
    TypePtr type;                         ///< Field type.
    std::optional<ExprPtr> default_value; ///< Optional default value expression.
    SourceSpan span;                      ///< Source location.
};

/// Struct declaration.
///
/// Defines a named product type with fields.
///
/// # Example
///
/// ```tml
/// type Point[T] {
///     x: T,
///     y: T,
/// }
/// ```
struct StructDecl {
    std::optional<std::string> doc;          ///< Documentation comment (from `///`).
    std::vector<Decorator> decorators;       ///< Decorators.
    Visibility vis;                          ///< Visibility.
    std::string name;                        ///< Struct name.
    std::vector<GenericParam> generics;      ///< Generic parameters.
    std::vector<StructField> fields;         ///< Fields.
    std::optional<WhereClause> where_clause; ///< Where clause.
    SourceSpan span;                         ///< Source location.
};

/// Union declaration (C-style).
///
/// Defines a type where all fields share the same memory location.
/// Only one field can be meaningfully accessed at a time.
/// Field access is `lowlevel` (unsafe) as there's no runtime type checking.
///
/// # Example
///
/// ```tml
/// union Value {
///     int_val: I32,
///     float_val: F32,
///     ptr_val: *Unit,
/// }
/// ```
struct UnionDecl {
    std::optional<std::string> doc;    ///< Documentation comment.
    std::vector<Decorator> decorators; ///< Decorators.
    Visibility vis;                    ///< Visibility.
    std::string name;                  ///< Union name.
    std::vector<StructField> fields;   ///< Fields (share same memory).
    SourceSpan span;                   ///< Source location.
};

/// An enum variant.
///
/// Variants can be:
/// - Unit variants: `Nothing`
/// - Tuple variants: `Just(T)`
/// - Struct variants: `Error { code: I32, message: Str }`
struct EnumVariant {
    std::optional<std::string> doc;                        ///< Documentation comment.
    std::string name;                                      ///< Variant name.
    std::optional<std::vector<TypePtr>> tuple_fields;      ///< Tuple variant fields.
    std::optional<std::vector<StructField>> struct_fields; ///< Struct variant fields.
    SourceSpan span;                                       ///< Source location.
};

/// Enum declaration.
///
/// Defines a sum type with variants.
///
/// # Example
///
/// ```tml
/// type Maybe[T] {
///     Just(T),
///     Nothing,
/// }
///
/// type Result[T, E] {
///     Ok(T),
///     Err(E),
/// }
/// ```
struct EnumDecl {
    std::optional<std::string> doc;          ///< Documentation comment.
    std::vector<Decorator> decorators;       ///< Decorators.
    Visibility vis;                          ///< Visibility.
    std::string name;                        ///< Enum name.
    std::vector<GenericParam> generics;      ///< Generic parameters.
    std::vector<EnumVariant> variants;       ///< Variants.
    std::optional<WhereClause> where_clause; ///< Where clause.
    SourceSpan span;                         ///< Source location.
};

/// Type alias: `type Alias = OriginalType`.
///
/// Creates an alias for an existing type.
///
/// # Examples
///
/// ```tml
/// type Result[T] = Outcome[T, Error]
/// type Callback = func(I32) -> Bool
/// ```
struct TypeAliasDecl {
    std::optional<std::string> doc;     ///< Documentation comment.
    Visibility vis;                     ///< Visibility.
    std::string name;                   ///< Alias name.
    std::vector<GenericParam> generics; ///< Generic parameters.
    TypePtr type;                       ///< Aliased type.
    SourceSpan span;                    ///< Source location.
};

// ============================================================================
// Behavior (Trait) Declarations
// ============================================================================

/// Associated type declaration in a behavior.
///
/// Associated types are type parameters that implementors must specify.
///
/// # Examples
///
/// ```tml
/// behavior Iterator {
///     type Item                    // Basic
///     type Item = I32              // With default
///     type Item[T]                 // GAT (Generic Associated Type)
///     type Item: Display           // With bounds
/// }
/// ```
struct AssociatedType {
    std::string name;                    ///< Type name.
    std::vector<GenericParam> generics;  ///< GAT generic parameters.
    std::vector<TypePtr> bounds;         ///< Behavior bounds.
    std::optional<TypePtr> default_type; ///< Optional default.
    SourceSpan span;                     ///< Source location.
};

/// Associated type binding in an impl block.
///
/// Specifies the concrete type for an associated type.
///
/// # Example
///
/// ```tml
/// impl Iterator for MyIter {
///     type Item = I32
/// }
/// ```
struct AssociatedTypeBinding {
    std::string name;                   ///< Type name.
    std::vector<GenericParam> generics; ///< GAT parameters in binding.
    TypePtr type;                       ///< The concrete type.
    SourceSpan span;                    ///< Source location.
};

/// Behavior (trait) declaration.
///
/// TML uses `behavior` instead of Rust's `trait`. Behaviors define
/// shared functionality that types can implement.
///
/// # Example
///
/// ```tml
/// behavior Iterator {
///     type Item
///     func next(mut this) -> Maybe[This::Item]
/// }
///
/// behavior Display {
///     func fmt(this, f: mut Formatter) -> Outcome[(), Error]
/// }
/// ```
struct TraitDecl {
    std::optional<std::string> doc;               ///< Documentation comment.
    std::vector<Decorator> decorators;            ///< Decorators.
    Visibility vis;                               ///< Visibility.
    std::string name;                             ///< Behavior name.
    std::vector<GenericParam> generics;           ///< Generic parameters.
    std::vector<TypePtr> super_traits;            ///< Super-behaviors (inheritance).
    std::vector<AssociatedType> associated_types; ///< Associated types.
    std::vector<FuncDecl> methods;                ///< Method signatures/defaults.
    std::optional<WhereClause> where_clause;      ///< Where clause.
    SourceSpan span;                              ///< Source location.
};

// ============================================================================
// Implementation Blocks
// ============================================================================

// Forward declaration for ConstDecl (used in ImplDecl)
struct ConstDecl;

/// Implementation block.
///
/// Provides method implementations for types. Can be:
/// - Inherent impl: methods on a type
/// - Trait impl: implementing a behavior for a type
///
/// # Examples
///
/// ```tml
/// impl Point {                    // Inherent impl
///     func new(x: I32, y: I32) -> This { ... }
/// }
///
/// impl Display for Point {        // Behavior impl
///     func fmt(this, f: mut Formatter) -> Outcome[(), Error] { ... }
/// }
/// ```
struct ImplDecl {
    std::optional<std::string> doc;     ///< Documentation comment.
    std::vector<GenericParam> generics; ///< Generic parameters.
    TypePtr trait_type;                 ///< Behavior being implemented (nullptr for inherent).
    TypePtr self_type;                  ///< Type being implemented.
    std::vector<AssociatedTypeBinding> type_bindings; ///< Associated type bindings.
    std::vector<ConstDecl> constants;                 ///< Associated constants.
    std::vector<FuncDecl> methods;                    ///< Method implementations.
    std::optional<WhereClause> where_clause;          ///< Where clause.
    SourceSpan span;                                  ///< Source location.
};

// ============================================================================
// Constants and Imports
// ============================================================================

/// Constant declaration: `const PI: F64 = 3.14159`.
///
/// Defines a compile-time constant value.
///
/// # Examples
///
/// ```tml
/// const MAX_SIZE: U64 = 1024
/// const PI: F64 = 3.14159265358979
/// pub const VERSION: Str = "1.0.0"
/// ```
struct ConstDecl {
    std::optional<std::string> doc; ///< Documentation comment.
    Visibility vis;                 ///< Visibility.
    std::string name;               ///< Constant name.
    TypePtr type;                   ///< Constant type.
    ExprPtr value;                  ///< Constant value (must be const-evaluable).
    SourceSpan span;                ///< Source location.
};

/// Use declaration for imports.
///
/// Imports items into the current scope.
///
/// # Examples
///
/// ```tml
/// use std::io::Read               // Single import
/// use std::math::{abs, sqrt}      // Grouped import
/// use std::collections::*         // Glob import
/// use std::io::Read as IoRead     // Aliased import
/// ```
struct UseDecl {
    Visibility vis;                                  ///< Visibility.
    TypePath path;                                   ///< Import path.
    std::optional<std::string> alias;                ///< Alias (`as Name`).
    std::optional<std::vector<std::string>> symbols; ///< Grouped symbols.
    bool is_glob = false;                            ///< True for `*` imports.
    SourceSpan span;                                 ///< Source location.
};

// ============================================================================
// Modules
// ============================================================================

/// Module declaration.
///
/// Organizes code into namespaces.
///
/// # Examples
///
/// ```tml
/// mod foo;                        // External file (foo.tml)
/// mod bar { ... }                 // Inline module
/// pub mod utils { ... }           // Public module
/// ```
struct ModDecl {
    Visibility vis;                            ///< Visibility.
    std::string name;                          ///< Module name.
    std::optional<std::vector<DeclPtr>> items; ///< Items (none for file modules).
    SourceSpan span;                           ///< Source location.
};

} // namespace tml::parser

#endif // TML_PARSER_AST_DECLS_HPP
