//! # Type System
//!
//! This module defines the semantic type representation for TML. These types
//! are used during type checking and code generation, distinct from the AST
//! type nodes produced by the parser.
//!
//! ## Type Categories
//!
//! - **Primitives**: `I8`-`I128`, `U8`-`U128`, `F32`, `F64`, `Bool`, `Char`, `Str`, `Unit`, `Never`
//! - **Compound**: Tuples, arrays, slices, functions, closures
//! - **User-defined**: Structs, enums via `NamedType`
//! - **References**: `ref T`, `mut ref T`
//! - **Pointers**: `*T`, `*mut T`
//! - **Generics**: Type variables, generic parameters, const generics
//! - **Behaviors**: Dynamic trait objects, impl returns
//!
//! ## Type Sharing
//!
//! Types are shared via `TypePtr` (`std::shared_ptr<Type>`) to enable
//! efficient comparison and avoid deep copying during type inference.
//!
//! ## Type Variables
//!
//! During type inference, unknown types are represented as `TypeVar` nodes.
//! These are resolved via unification in the `TypeEnv`.

#ifndef TML_TYPES_TYPE_HPP
#define TML_TYPES_TYPE_HPP

#include "common.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tml::types {

// Forward declarations
struct Type;

/// Shared pointer to a type (enables efficient sharing and comparison).
using TypePtr = std::shared_ptr<Type>;

/// Primitive type kinds.
///
/// TML uses explicit-width integers for clarity and portability.
enum class PrimitiveKind {
    // Signed integers
    I8,   ///< 8-bit signed integer
    I16,  ///< 16-bit signed integer
    I32,  ///< 32-bit signed integer
    I64,  ///< 64-bit signed integer
    I128, ///< 128-bit signed integer
    // Unsigned integers
    U8,   ///< 8-bit unsigned integer
    U16,  ///< 16-bit unsigned integer
    U32,  ///< 32-bit unsigned integer
    U64,  ///< 64-bit unsigned integer
    U128, ///< 128-bit unsigned integer
    // Floating point
    F32, ///< 32-bit float (IEEE 754)
    F64, ///< 64-bit float (IEEE 754)
    // Other primitives
    Bool,  ///< Boolean (`true`/`false`)
    Char,  ///< Unicode scalar value (32-bit)
    Str,   ///< String slice (`str`)
    Unit,  ///< Unit type `()`
    Never, ///< Never type `!` (function never returns)
};

/// A primitive type.
struct PrimitiveType {
    PrimitiveKind kind; ///< The primitive kind.
};

/// A user-defined named type (struct, enum, etc.).
struct NamedType {
    std::string name;               ///< Type name.
    std::string module_path;        ///< Fully qualified module path.
    std::vector<TypePtr> type_args; ///< Generic type arguments.
};

/// Reference type: `ref T`, `mut ref T`, `ref[a] T`, or `mut ref[a] T`.
struct RefType {
    bool is_mut;                          ///< True for mutable reference.
    TypePtr inner;                        ///< Referenced type.
    std::optional<std::string> lifetime;  ///< Optional explicit lifetime annotation.
};

/// Raw pointer type: `*T` or `*mut T`.
struct PtrType {
    bool is_mut;   ///< True for mutable pointer.
    TypePtr inner; ///< Pointed-to type.
};

/// Fixed-size array type: `[T; N]`.
struct ArrayType {
    TypePtr element; ///< Element type.
    size_t size;     ///< Array size (known at compile time).
};

/// Slice type: `[T]`.
struct SliceType {
    TypePtr element; ///< Element type.
};

/// Tuple type: `(T, U, V)`.
struct TupleType {
    std::vector<TypePtr> elements; ///< Element types.
};

/// Function type: `func(A, B) -> R`.
struct FuncType {
    std::vector<TypePtr> params; ///< Parameter types.
    TypePtr return_type;         ///< Return type.
    bool is_async;               ///< True for async functions.
};

/// A captured variable in a closure environment.
struct CapturedVar {
    std::string name; ///< Variable name.
    TypePtr type;     ///< Variable type.
    bool is_mut;      ///< True if captured mutably.
};

/// Closure type with captured environment.
struct ClosureType {
    std::vector<TypePtr> params;       ///< Parameter types.
    TypePtr return_type;               ///< Return type.
    std::vector<CapturedVar> captures; ///< Captured variables.
};

/// Type variable for inference.
///
/// During type checking, unknown types are represented as type variables.
/// These are resolved via unification.
struct TypeVar {
    uint32_t id;                  ///< Unique identifier.
    std::optional<TypePtr> bound; ///< Optional upper bound.
};

/// A generic type parameter.
struct GenericType {
    std::string name;            ///< Parameter name.
    std::vector<TypePtr> bounds; ///< Behavior bounds.
};

/// A compile-time constant value for const generics.
///
/// # Examples
///
/// - `[T; 10]` - array with const size 10
/// - `Array[I32, 5]` - generic type with const argument
struct ConstValue {
    std::variant<int64_t,  ///< Signed integer constant.
                 uint64_t, ///< Unsigned integer constant.
                 bool,     ///< Boolean constant.
                 char      ///< Character constant.
                 >
        value;    ///< The constant value.
    TypePtr type; ///< Type of this const value (`I32`, `U64`, etc.).

    /// Creates a signed integer const value.
    static ConstValue from_i64(int64_t v, TypePtr t) {
        return ConstValue{v, std::move(t)};
    }

    /// Creates an unsigned integer const value.
    static ConstValue from_u64(uint64_t v, TypePtr t) {
        return ConstValue{v, std::move(t)};
    }

    /// Creates a boolean const value.
    static ConstValue from_bool(bool v, TypePtr t) {
        return ConstValue{v, std::move(t)};
    }

    /// Creates a character const value.
    static ConstValue from_char(char v, TypePtr t) {
        return ConstValue{v, std::move(t)};
    }

    /// Returns value as signed integer (for array sizes, etc.).
    [[nodiscard]] int64_t as_i64() const {
        if (std::holds_alternative<int64_t>(value)) {
            return std::get<int64_t>(value);
        } else if (std::holds_alternative<uint64_t>(value)) {
            return static_cast<int64_t>(std::get<uint64_t>(value));
        } else if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? 1 : 0;
        } else {
            return static_cast<int64_t>(std::get<char>(value));
        }
    }

    /// Returns value as unsigned integer.
    [[nodiscard]] uint64_t as_u64() const {
        if (std::holds_alternative<uint64_t>(value)) {
            return std::get<uint64_t>(value);
        } else if (std::holds_alternative<int64_t>(value)) {
            return static_cast<uint64_t>(std::get<int64_t>(value));
        } else if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value) ? 1 : 0;
        } else {
            return static_cast<uint64_t>(std::get<char>(value));
        }
    }
};

/// A const generic type parameter.
///
/// Used in generic definitions: `func foo[const N: U64](arr: Array[T, N])`
struct ConstGenericType {
    std::string name;   ///< Parameter name (e.g., "N").
    TypePtr value_type; ///< Type of the const value (e.g., `U64`).
};

/// Dynamic behavior (trait object): `dyn Behavior[T]`.
///
/// Represents a type-erased value that implements a behavior.
struct DynBehaviorType {
    std::string behavior_name;      ///< The behavior for this trait object.
    std::vector<TypePtr> type_args; ///< Generic parameters.
    bool is_mut;                    ///< True for `dyn mut Behavior`.
};

/// Opaque impl behavior return type: `impl Behavior[T]`.
///
/// Represents "some type that implements Behavior" without revealing
/// the concrete type. Used for return types.
struct ImplBehaviorType {
    std::string behavior_name;      ///< The behavior being implemented.
    std::vector<TypePtr> type_args; ///< Generic parameters.
};

// ============================================================================
// OOP Types (C#-style)
// ============================================================================

/// Class type for OOP-style classes.
///
/// Represents an instance of a class. Classes support:
/// - Single inheritance (extends)
/// - Multiple interface implementation (implements)
/// - Virtual dispatch via vtables
/// - Fields, methods, properties, constructors
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
struct ClassType {
    std::string name;               ///< Class name.
    std::string module_path;        ///< Fully qualified module path.
    std::vector<TypePtr> type_args; ///< Generic type arguments.
};

/// Interface type for OOP-style interfaces.
///
/// Represents an interface that classes can implement.
/// Interfaces support:
/// - Multiple inheritance (extends)
/// - Method signatures with optional default implementations
///
/// # Example
///
/// ```tml
/// interface Drawable {
///     func draw(this, canvas: ref Canvas)
/// }
/// ```
struct InterfaceType {
    std::string name;               ///< Interface name.
    std::string module_path;        ///< Fully qualified module path.
    std::vector<TypePtr> type_args; ///< Generic type arguments.
};

/// A semantic type.
///
/// This is the unified type representation used throughout the compiler after
/// parsing. All type information flows through this structure.
struct Type {
    std::variant<PrimitiveType, NamedType, RefType, PtrType, ArrayType, SliceType, TupleType,
                 FuncType, ClosureType, TypeVar, GenericType, ConstGenericType, DynBehaviorType,
                 ImplBehaviorType, ClassType, InterfaceType>
        kind;        ///< The type variant.
    uint64_t id = 0; ///< Unique ID for fast comparison.

    /// Checks if this type is of kind `T`.
    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    /// Gets this type as kind `T`.
    template <typename T> [[nodiscard]] auto as() -> T& {
        return std::get<T>(kind);
    }

    /// Gets this type as kind `T` (const).
    template <typename T> [[nodiscard]] auto as() const -> const T& {
        return std::get<T>(kind);
    }
};

// ============================================================================
// Type Factory Functions
// ============================================================================

/// Creates a primitive type.
[[nodiscard]] auto make_primitive(PrimitiveKind kind) -> TypePtr;

/// Creates the unit type `()`.
[[nodiscard]] auto make_unit() -> TypePtr;

/// Creates the `Bool` type.
[[nodiscard]] auto make_bool() -> TypePtr;

/// Creates the `I32` type.
[[nodiscard]] auto make_i32() -> TypePtr;

/// Creates the `I64` type.
[[nodiscard]] auto make_i64() -> TypePtr;

/// Creates the `F64` type.
[[nodiscard]] auto make_f64() -> TypePtr;

/// Creates the `Str` type.
[[nodiscard]] auto make_str() -> TypePtr;

/// Creates the `Never` type `!`.
[[nodiscard]] auto make_never() -> TypePtr;

/// Creates a tuple type.
[[nodiscard]] auto make_tuple(std::vector<TypePtr> elements) -> TypePtr;

/// Creates a function type.
[[nodiscard]] auto make_func(std::vector<TypePtr> params, TypePtr ret) -> TypePtr;

/// Creates a closure type with optional captures.
[[nodiscard]] auto make_closure(std::vector<TypePtr> params, TypePtr ret,
                                std::vector<CapturedVar> captures = {}) -> TypePtr;

/// Creates a reference type.
[[nodiscard]] auto make_ref(TypePtr inner, bool is_mut = false) -> TypePtr;

/// Creates a pointer type.
[[nodiscard]] auto make_ptr(TypePtr inner, bool is_mut = false) -> TypePtr;

/// Creates an array type.
[[nodiscard]] auto make_array(TypePtr element, size_t size) -> TypePtr;

/// Creates a slice type.
[[nodiscard]] auto make_slice(TypePtr element) -> TypePtr;

/// Creates a const generic type parameter.
[[nodiscard]] auto make_const_generic(std::string name, TypePtr value_type) -> TypePtr;

/// Creates an impl behavior return type.
[[nodiscard]] auto make_impl_behavior(std::string behavior_name,
                                      std::vector<TypePtr> type_args = {}) -> TypePtr;

/// Creates a class type.
[[nodiscard]] auto make_class(std::string name, std::string module_path = "",
                              std::vector<TypePtr> type_args = {}) -> TypePtr;

/// Creates an interface type.
[[nodiscard]] auto make_interface(std::string name, std::string module_path = "",
                                  std::vector<TypePtr> type_args = {}) -> TypePtr;

// ============================================================================
// Type Comparison and Conversion
// ============================================================================

/// Checks if two types are structurally equal.
[[nodiscard]] auto types_equal(const TypePtr& a, const TypePtr& b) -> bool;

/// Converts a type to its string representation.
[[nodiscard]] auto type_to_string(const TypePtr& type) -> std::string;

/// Checks if two const values are equal.
[[nodiscard]] auto const_values_equal(const ConstValue& a, const ConstValue& b) -> bool;

/// Converts a const value to string.
[[nodiscard]] auto const_value_to_string(const ConstValue& value) -> std::string;

/// Converts a primitive kind to its string name.
[[nodiscard]] auto primitive_kind_to_string(PrimitiveKind kind) -> std::string;

// ============================================================================
// Generic Substitution
// ============================================================================

/// Substitutes generic type parameters with concrete types.
///
/// Example: `substitute_type(List[T], {T -> I32})` returns `List[I32]`
[[nodiscard]] auto substitute_type(const TypePtr& type,
                                   const std::unordered_map<std::string, TypePtr>& substitutions)
    -> TypePtr;

/// Substitutes both type and const generic parameters.
///
/// - `type_substitutions`: maps type param names to types (e.g., `T -> I32`)
/// - `const_substitutions`: maps const param names to values (e.g., `N -> 10`)
[[nodiscard]] auto substitute_type_with_consts(
    const TypePtr& type, const std::unordered_map<std::string, TypePtr>& type_substitutions,
    const std::unordered_map<std::string, ConstValue>& const_substitutions) -> TypePtr;

} // namespace tml::types

#endif // TML_TYPES_TYPE_HPP
