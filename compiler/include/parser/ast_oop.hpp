//! # OOP AST Nodes (C#-style)
//!
//! This module defines the AST nodes for object-oriented programming constructs.
//! TML supports C#-style OOP with classes, interfaces, inheritance, and polymorphism.
//!
//! ## OOP Features
//!
//! - **Classes**: Single inheritance, fields, methods, properties, constructors
//! - **Interfaces**: Multiple interface implementation, default methods
//! - **Virtual methods**: Runtime polymorphism via vtables
//! - **Abstract classes**: Cannot be instantiated, can have abstract methods
//! - **Sealed classes**: Cannot be inherited from
//! - **Namespaces**: Hierarchical organization (like C# namespaces)
//!
//! ## Syntax Overview
//!
//! ```tml
//! interface Drawable {
//!     func draw(this, canvas: ref Canvas)
//! }
//!
//! class Animal {
//!     private name: Str
//!
//!     func new(name: Str) { this.name = name }
//!
//!     virtual func speak(this) -> Str { "..." }
//! }
//!
//! class Dog extends Animal implements Drawable {
//!     override func speak(this) -> Str { "Woof!" }
//!
//!     func draw(this, canvas: ref Canvas) { ... }
//! }
//! ```
//!
//! ## Member Visibility
//!
//! - `private` - Only accessible within the class
//! - `protected` - Accessible within the class and subclasses
//! - `pub` - Accessible everywhere

#ifndef TML_PARSER_AST_OOP_HPP
#define TML_PARSER_AST_OOP_HPP

#include "ast_common.hpp"
#include "ast_decls.hpp"
#include "ast_types.hpp"

namespace tml::parser {

// ============================================================================
// Member Visibility
// ============================================================================

/// Member visibility for class/interface members.
///
/// Controls access to fields, methods, and properties within class hierarchies.
enum class MemberVisibility {
    Private,   ///< `private` - only accessible within this class.
    Protected, ///< `protected` - accessible within class and subclasses.
    Public,    ///< `pub` - accessible everywhere.
};

// ============================================================================
// Class Members
// ============================================================================

/// Class field declaration.
///
/// Fields store data within class instances. Can be static (shared across
/// all instances) or instance-level.
///
/// # Examples
///
/// ```tml
/// class Person {
///     private name: Str                    // Private instance field
///     protected age: I32                   // Protected instance field
///     pub static count: I32 = 0            // Public static field with initializer
/// }
/// ```
struct ClassField {
    std::optional<std::string> doc; ///< Documentation comment.
    MemberVisibility vis;           ///< Visibility modifier.
    bool is_static;                 ///< True for `static` fields.
    std::string name;               ///< Field name.
    TypePtr type;                   ///< Field type.
    std::optional<ExprPtr> init;    ///< Default value initializer.
    SourceSpan span;                ///< Source location.
};

/// Class method declaration.
///
/// Methods define behavior for class instances. Support modifiers for
/// polymorphism and static dispatch.
///
/// # Modifiers
///
/// - `static` - Class-level method, no `this` parameter
/// - `virtual` - Can be overridden by subclasses
/// - `override` - Overrides a parent's virtual method
/// - `abstract` - Must be implemented by subclasses (no body)
///
/// # Examples
///
/// ```tml
/// class Animal {
///     virtual func speak(this) -> Str { "..." }
///     static func create() -> Animal { new Animal() }
/// }
///
/// class Dog extends Animal {
///     override func speak(this) -> Str { "Woof!" }
/// }
/// ```
struct ClassMethod {
    std::optional<std::string> doc;          ///< Documentation comment.
    std::vector<Decorator> decorators;       ///< Decorators.
    MemberVisibility vis;                    ///< Visibility modifier.
    bool is_static;                          ///< True for `static` methods.
    bool is_virtual;                         ///< True for `virtual` methods.
    bool is_override;                        ///< True for `override` methods.
    bool is_abstract;                        ///< True for `abstract` methods.
    bool is_final;                           ///< True for `final` methods.
    std::string name;                        ///< Method name.
    std::vector<GenericParam> generics;      ///< Generic parameters.
    std::vector<FuncParam> params;           ///< Parameters.
    std::optional<TypePtr> return_type;      ///< Return type.
    std::optional<WhereClause> where_clause; ///< Where clause.
    std::optional<BlockExpr> body;           ///< Body (none for abstract methods).
    SourceSpan span;                         ///< Source location.
};

/// Property declaration.
///
/// Properties provide controlled access to data with optional getters/setters.
/// Similar to C# properties.
///
/// # Examples
///
/// ```tml
/// class Rectangle {
///     private _width: F64
///     private _height: F64
///
///     // Read-only property
///     pub prop area: F64 {
///         get { this._width * this._height }
///     }
///
///     // Read-write property with validation
///     pub prop width: F64 {
///         get { this._width }
///         set {
///             if value > 0 { this._width = value }
///         }
///     }
/// }
/// ```
struct PropertyDecl {
    std::optional<std::string> doc; ///< Documentation comment.
    MemberVisibility vis;           ///< Visibility modifier.
    bool is_static;                 ///< True for `static` properties.
    std::string name;               ///< Property name.
    TypePtr type;                   ///< Property type.
    std::optional<ExprPtr> getter;  ///< Getter expression body.
    std::optional<ExprPtr> setter;  ///< Setter expression body (uses implicit `value`).
    bool has_getter;                ///< True if has getter.
    bool has_setter;                ///< True if has setter.
    SourceSpan span;                ///< Source location.
};

/// Constructor declaration.
///
/// Constructors initialize new class instances. Can call parent constructor
/// with `base(args)`.
///
/// # Examples
///
/// ```tml
/// class Animal {
///     private name: Str
///
///     func new(name: Str) {
///         this.name = name
///     }
/// }
///
/// class Dog extends Animal {
///     private breed: Str
///
///     func new(name: Str, breed: Str) : base(name) {
///         this.breed = breed
///     }
/// }
/// ```
struct ConstructorDecl {
    std::optional<std::string> doc;                ///< Documentation comment.
    MemberVisibility vis;                          ///< Visibility modifier.
    std::vector<FuncParam> params;                 ///< Constructor parameters.
    std::optional<std::vector<ExprPtr>> base_args; ///< Arguments for base constructor call.
    std::optional<BlockExpr> body;                 ///< Constructor body.
    SourceSpan span;                               ///< Source location.
};

// ============================================================================
// Interface Declaration
// ============================================================================

/// Interface method signature.
///
/// Interface methods define the contract that implementing classes must fulfill.
/// Can have default implementations.
///
/// # Examples
///
/// ```tml
/// interface Comparable[T] {
///     func compare(this, other: ref T) -> I32
///
///     // Default implementation using compare
///     func less_than(this, other: ref T) -> Bool {
///         this.compare(other) < 0
///     }
/// }
/// ```
struct InterfaceMethod {
    std::optional<std::string> doc;          ///< Documentation comment.
    std::string name;                        ///< Method name.
    std::vector<GenericParam> generics;      ///< Generic parameters.
    std::vector<FuncParam> params;           ///< Parameters.
    std::optional<TypePtr> return_type;      ///< Return type.
    std::optional<WhereClause> where_clause; ///< Where clause.
    std::optional<BlockExpr> default_body;   ///< Default implementation (optional).
    bool is_static;                          ///< True for static interface methods.
    SourceSpan span;                         ///< Source location.
};

/// Interface declaration.
///
/// Interfaces define contracts that classes can implement.
/// Support multiple inheritance and default method implementations.
///
/// # Examples
///
/// ```tml
/// interface Drawable {
///     func draw(this, canvas: ref Canvas)
/// }
///
/// interface Clickable extends Drawable {
///     func on_click(mut this, event: ClickEvent)
/// }
///
/// interface Serializable[Format] {
///     func serialize(this) -> Format
///     func deserialize(data: Format) -> This
/// }
/// ```
struct InterfaceDecl {
    std::optional<std::string> doc;          ///< Documentation comment.
    std::vector<Decorator> decorators;       ///< Decorators.
    Visibility vis;                          ///< Visibility.
    std::string name;                        ///< Interface name.
    std::vector<GenericParam> generics;      ///< Generic parameters.
    std::vector<TypePath> extends;           ///< Extended interfaces (multiple inheritance).
    std::vector<InterfaceMethod> methods;    ///< Method signatures.
    std::optional<WhereClause> where_clause; ///< Where clause.
    SourceSpan span;                         ///< Source location.
};

// ============================================================================
// Class Declaration
// ============================================================================

/// Class declaration.
///
/// Classes are the primary OOP construct in TML. Support:
/// - Single inheritance (`extends`)
/// - Multiple interface implementation (`implements`)
/// - Virtual methods and polymorphism
/// - Abstract and sealed modifiers
///
/// # Modifiers
///
/// - `abstract` - Cannot be instantiated, can have abstract methods
/// - `sealed` - Cannot be inherited from
///
/// # Examples
///
/// ```tml
/// class Animal {
///     private name: Str
///
///     func new(name: Str) { this.name = name }
///     virtual func speak(this) -> Str { "..." }
/// }
///
/// class Dog extends Animal implements Friendly {
///     override func speak(this) -> Str { "Woof!" }
///     func greet(this) -> Str { "Hello!" }
/// }
///
/// abstract class Shape {
///     abstract func area(this) -> F64
/// }
///
/// sealed class FinalClass { ... }
/// ```
struct ClassDecl {
    std::optional<std::string> doc;            ///< Documentation comment.
    std::vector<Decorator> decorators;         ///< Decorators.
    Visibility vis;                            ///< Visibility.
    bool is_abstract;                          ///< True for `abstract class`.
    bool is_sealed;                            ///< True for `sealed class`.
    std::string name;                          ///< Class name.
    std::vector<GenericParam> generics;        ///< Generic parameters.
    std::optional<TypePath> extends;           ///< Parent class (single inheritance).
    std::vector<Box<Type>> implements;         ///< Implemented interfaces (supports generics).
    std::vector<ClassField> fields;            ///< Fields.
    std::vector<ClassMethod> methods;          ///< Methods.
    std::vector<PropertyDecl> properties;      ///< Properties.
    std::vector<ConstructorDecl> constructors; ///< Constructors.
    std::optional<WhereClause> where_clause;   ///< Where clause.
    SourceSpan span;                           ///< Source location.
};

// ============================================================================
// Namespace Declaration
// ============================================================================

/// Namespace declaration.
///
/// Namespaces provide hierarchical organization for declarations.
/// Similar to C# namespaces or Java packages.
///
/// # Examples
///
/// ```tml
/// namespace MyApp.Core {
///     class Engine { ... }
///     interface IService { ... }
/// }
///
/// namespace MyApp.Core.Utilities {
///     func helper() { ... }
/// }
/// ```
struct NamespaceDecl {
    std::vector<std::string> path; ///< Namespace path (e.g., ["MyApp", "Core"]).
    std::vector<DeclPtr> items;    ///< Items in the namespace.
    SourceSpan span;               ///< Source location.
};

} // namespace tml::parser

#endif // TML_PARSER_AST_OOP_HPP
