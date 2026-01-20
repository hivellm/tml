//! # Type AST Nodes
//!
//! This module defines the AST nodes for type annotations and expressions.
//! Types are used throughout the AST for variable declarations, function
//! signatures, struct fields, and generic constraints.
//!
//! ## Type Categories
//!
//! - **Named types**: `I32`, `Vec[T]`, `std::io::File`
//! - **Reference types**: `ref T`, `mut ref T`
//! - **Pointer types**: `*const T`, `*mut T`
//! - **Composite types**: Arrays `[T; N]`, slices `[T]`, tuples `(T, U)`
//! - **Function types**: `func(A, B) -> R`
//! - **Trait objects**: `dyn Behavior`, `impl Behavior`
//!
//! ## TML vs Rust Syntax
//!
//! | Rust | TML | Description |
//! |------|-----|-------------|
//! | `<T>` | `[T]` | Generic parameters |
//! | `&T` | `ref T` | Immutable reference |
//! | `&mut T` | `mut ref T` | Mutable reference |
//! | `dyn Trait` | `dyn Behavior` | Trait object |
//! | `impl Trait` | `impl Behavior` | Opaque return type |

#ifndef TML_PARSER_AST_TYPES_HPP
#define TML_PARSER_AST_TYPES_HPP

#include "ast_common.hpp"

namespace tml::parser {

// ============================================================================
// Type Path
// ============================================================================

/// A qualified type path like `Vec`, `std::io::File`, or `core::Option`.
///
/// Used for named types, trait bounds, and path expressions. Paths can be
/// simple identifiers or qualified with module/namespace prefixes.
///
/// # Examples
///
/// - Simple: `Vec` -> segments = ["Vec"]
/// - Qualified: `std::io::File` -> segments = ["std", "io", "File"]
/// - Nested: `core::option::Option` -> segments = ["core", "option", "Option"]
struct TypePath {
    std::vector<std::string> segments; ///< Path segments (e.g., ["std", "io", "File"]).
    SourceSpan span;                   ///< Source location.
};

// ============================================================================
// Generic Arguments
// ============================================================================

/// A generic argument, which can be a type, const expression, or binding.
///
/// Generic arguments appear in square brackets after type names in TML.
/// They can be:
/// - Type arguments: `Vec[I32]` - a concrete type
/// - Const arguments: `Array[I32, 100]` - a compile-time constant
/// - Associated type bindings: `Iterator[Item=I32]` - binding for associated types
///
/// # Examples
///
/// ```tml
/// let v: Vec[I32] = Vec::new()           // Type argument
/// let arr: Array[U8, 256] = [0; 256]     // Const argument
/// func process[I: Iterator[Item=I32]]()  // Associated type binding
/// ```
struct GenericArg {
    std::variant<TypePtr, ExprPtr> value; ///< The type or const expression value.
    bool is_const = false;                ///< True if this is a const generic argument.
    std::optional<std::string> name;      ///< Binding name for associated types (e.g., "Item").
    SourceSpan span;                      ///< Source location.

    /// Creates a type argument.
    ///
    /// # Parameters
    /// - `type`: The type to use as argument
    /// - `sp`: Source location span
    static GenericArg from_type(TypePtr type, SourceSpan sp) {
        return GenericArg{std::move(type), false, std::nullopt, sp};
    }

    /// Creates a const generic argument.
    ///
    /// # Parameters
    /// - `expr`: The constant expression value
    /// - `sp`: Source location span
    static GenericArg from_const(ExprPtr expr, SourceSpan sp) {
        return GenericArg{std::move(expr), true, std::nullopt, sp};
    }

    /// Creates an associated type binding.
    ///
    /// # Parameters
    /// - `binding_name`: The associated type name (e.g., "Item")
    /// - `type`: The concrete type to bind
    /// - `sp`: Source location span
    static GenericArg from_binding(std::string binding_name, TypePtr type, SourceSpan sp) {
        return GenericArg{std::move(type), false, std::move(binding_name), sp};
    }

    /// Returns true if this is a type argument (not const, not binding).
    [[nodiscard]] bool is_type() const {
        return std::holds_alternative<TypePtr>(value);
    }

    /// Returns true if this is a const expression argument.
    [[nodiscard]] bool is_expr() const {
        return std::holds_alternative<ExprPtr>(value);
    }

    /// Returns true if this is an associated type binding (has a name).
    [[nodiscard]] bool is_binding() const {
        return name.has_value();
    }

    /// Gets the type value. Requires `is_type()` to be true.
    [[nodiscard]] TypePtr& as_type() {
        return std::get<TypePtr>(value);
    }

    /// Gets the type value (const version). Requires `is_type()` to be true.
    [[nodiscard]] const TypePtr& as_type() const {
        return std::get<TypePtr>(value);
    }

    /// Gets the expression value. Requires `is_expr()` to be true.
    [[nodiscard]] ExprPtr& as_expr() {
        return std::get<ExprPtr>(value);
    }

    /// Gets the expression value (const version). Requires `is_expr()` to be true.
    [[nodiscard]] const ExprPtr& as_expr() const {
        return std::get<ExprPtr>(value);
    }
};

/// A list of generic arguments: `[T, U]` or `[I32, 100]`.
///
/// Represents the arguments between square brackets in generic type
/// instantiations. Can contain mixed type and const arguments.
///
/// # Example
///
/// ```tml
/// HashMap[Str, Vec[I32]]  // Two type arguments, second is itself generic
/// Array[F64, 3]           // One type argument, one const argument
/// ```
struct GenericArgs {
    std::vector<GenericArg> args; ///< The generic arguments.
    SourceSpan span;              ///< Source location.

    /// Counts the number of type arguments (for validation).
    [[nodiscard]] size_t type_arg_count() const {
        size_t count = 0;
        for (const auto& arg : args) {
            if (arg.is_type()) {
                count++;
            }
        }
        return count;
    }

    /// Counts the number of const arguments (for validation).
    [[nodiscard]] size_t const_arg_count() const {
        size_t count = 0;
        for (const auto& arg : args) {
            if (arg.is_const) {
                count++;
            }
        }
        return count;
    }
};

// ============================================================================
// Reference and Pointer Types
// ============================================================================

/// Reference type: `ref T`, `mut ref T`, `ref[a] T`, or `mut ref[a] T`.
///
/// TML uses keyword-based syntax instead of Rust's `&T` / `&mut T`.
/// References are non-owning borrows of values. Optional explicit lifetime
/// annotation can be provided using `ref[lifetime] T` syntax.
///
/// # Examples
///
/// ```tml
/// func print(s: ref Str)              // Immutable reference (lifetime inferred)
/// func append(v: mut ref Vec[I32])    // Mutable reference (lifetime inferred)
/// func longest[life a](x: ref[a] Str, y: ref[a] Str) -> ref[a] Str  // Explicit lifetime
/// func static_ref() -> ref[static] Str { ref "hello" }  // Static lifetime
/// ```
struct RefType {
    bool is_mut;                          ///< True for mutable reference (`mut ref T`).
    TypePtr inner;                        ///< The referenced type.
    std::optional<std::string> lifetime;  ///< Optional lifetime annotation (e.g., "a", "static").
    SourceSpan span;                      ///< Source location.
};

/// Raw pointer type: `*const T` or `*mut T`.
///
/// Raw pointers are used in lowlevel (unsafe) code for FFI and
/// manual memory management. They bypass borrow checking.
///
/// # Examples
///
/// ```tml
/// @extern("c") func malloc(size: U64) -> *mut U8
/// func strlen(s: *const I8) -> U64
/// ```
struct PtrType {
    bool is_mut;     ///< True for mutable pointer (`*mut T`).
    TypePtr inner;   ///< The pointed-to type.
    SourceSpan span; ///< Source location.
};

// ============================================================================
// Composite Types
// ============================================================================

/// Fixed-size array type: `[T; N]`.
///
/// Arrays have a compile-time known size. The size must be a const expression.
///
/// # Examples
///
/// ```tml
/// let buffer: [U8; 1024] = [0; 1024]
/// let matrix: [[F64; 3]; 3] = [[0.0; 3]; 3]
/// ```
struct ArrayType {
    TypePtr element; ///< Element type.
    ExprPtr size;    ///< Size expression (must be const).
    SourceSpan span; ///< Source location.
};

/// Slice type: `[T]`.
///
/// Slices are dynamically-sized views into contiguous sequences.
/// They're typically used behind references: `ref [T]`.
///
/// # Example
///
/// ```tml
/// func sum(numbers: ref [I32]) -> I32
/// ```
struct SliceType {
    TypePtr element; ///< Element type.
    SourceSpan span; ///< Source location.
};

/// Tuple type: `(T, U, V)`.
///
/// Tuples are fixed-size collections of heterogeneous types.
/// The unit type `()` is represented as an empty tuple.
///
/// # Examples
///
/// ```tml
/// let point: (I32, I32) = (10, 20)
/// let result: (Bool, Str, I32) = (true, "ok", 42)
/// ```
struct TupleType {
    std::vector<TypePtr> elements; ///< Element types (empty for unit type).
    SourceSpan span;               ///< Source location.
};

/// Function type: `func(A, B) -> R`.
///
/// Represents the type of a function pointer or closure.
/// Used for higher-order functions and callbacks.
///
/// # Example
///
/// ```tml
/// func map[T, U](items: ref [T], f: func(T) -> U) -> Vec[U]
/// ```
struct FuncType {
    std::vector<TypePtr> params; ///< Parameter types.
    TypePtr return_type;         ///< Return type (unit type if nullptr).
    SourceSpan span;             ///< Source location.
};

// ============================================================================
// Named Types
// ============================================================================

/// Named type with optional generics: `Vec[T]`, `HashMap[K, V]`.
///
/// The most common type form. Refers to a type by name, optionally
/// with generic arguments.
///
/// # Examples
///
/// ```tml
/// let x: I32 = 42                      // Simple named type
/// let v: Vec[Str] = Vec::new()         // Generic type
/// let m: HashMap[Str, I32] = ...       // Multiple type params
/// ```
struct NamedType {
    TypePath path;                       ///< The type path (e.g., `std::collections::Vec`).
    std::optional<GenericArgs> generics; ///< Generic arguments (e.g., `[I32]`).
    SourceSpan span;                     ///< Source location.
};

/// Inferred type: `_` (let compiler infer).
///
/// Placeholder that asks the compiler to infer the type from context.
/// Useful in generic contexts where the type is obvious.
///
/// # Example
///
/// ```tml
/// let numbers: Vec[_] = vec![1, 2, 3]  // Infers Vec[I32]
/// ```
struct InferType {
    SourceSpan span; ///< Source location.
};

// ============================================================================
// Trait Object Types
// ============================================================================

/// Dynamic trait object type: `dyn Behavior[T]`.
///
/// Represents a type-erased value that implements a behavior.
/// Used for runtime polymorphism through vtables.
///
/// # Examples
///
/// ```tml
/// func draw_all(shapes: ref [ref dyn Drawable])
/// let writer: Heap[dyn Write] = Heap::new(file)
/// ```
struct DynType {
    TypePath behavior;                   ///< The behavior being used as trait object.
    std::optional<GenericArgs> generics; ///< Generic parameters (e.g., `dyn Iterator[Item=I32]`).
    bool is_mut;                         ///< True for `dyn mut Behavior`.
    SourceSpan span;                     ///< Source location.
};

/// Opaque impl return type: `impl Behavior[T]`.
///
/// Represents "some type that implements Behavior" without revealing
/// the concrete type. Used for return types to enable optimizations
/// while hiding implementation details.
///
/// # Example
///
/// ```tml
/// func make_iterator() -> impl Iterator[Item=I32] {
///     (0 to 100).filter(do(x) x % 2 == 0)
/// }
/// ```
struct ImplBehaviorType {
    TypePath behavior;                   ///< The behavior being implemented.
    std::optional<GenericArgs> generics; ///< Generic parameters.
    SourceSpan span;                     ///< Source location.
};

// ============================================================================
// Type Variant
// ============================================================================

/// A type expression.
///
/// Encompasses all type constructs in TML: named types, references,
/// pointers, arrays, slices, tuples, functions, and trait objects.
///
/// Uses `std::variant` for type-safe sum types with helper methods
/// for type checking and casting.
struct Type {
    std::variant<NamedType, RefType, PtrType, ArrayType, SliceType, TupleType, FuncType, InferType,
                 DynType, ImplBehaviorType>
        kind;        ///< The type variant.
    SourceSpan span; ///< Source location.

    /// Checks if this type is of kind `T`.
    ///
    /// # Example
    /// ```cpp
    /// if (type.is<RefType>()) { ... }
    /// ```
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Gets this type as kind `T`. Throws `std::bad_variant_access` if wrong kind.
    ///
    /// # Example
    /// ```cpp
    /// auto& ref = type.as<RefType>();
    /// ```
    template <typename T> [[nodiscard]] auto as() -> T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }

    /// Gets this type as kind `T` (const). Throws `std::bad_variant_access` if wrong kind.
    template <typename T> [[nodiscard]] auto as() const -> const T& {
        if (!is<T>()) {
            throw std::bad_variant_access();
        }
        return std::get<T>(kind);
    }
};

} // namespace tml::parser

#endif // TML_PARSER_AST_TYPES_HPP
