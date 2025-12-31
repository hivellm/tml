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
using TypePtr = std::shared_ptr<Type>;

// Primitive types
enum class PrimitiveKind {
    // Integers
    I8,
    I16,
    I32,
    I64,
    I128,
    U8,
    U16,
    U32,
    U64,
    U128,
    // Floats
    F32,
    F64,
    // Other primitives
    Bool,
    Char,  // Unicode scalar
    Str,   // String slice
    Unit,  // ()
    Never, // ! (never returns)
};

// Primitive type
struct PrimitiveType {
    PrimitiveKind kind;
};

// Named type (user-defined struct, enum, etc.)
struct NamedType {
    std::string name;
    std::string module_path;
    std::vector<TypePtr> type_args;
};

// Reference type: ref T, mut ref T
struct RefType {
    bool is_mut;
    TypePtr inner;
};

// Pointer type: *T, *mut T
struct PtrType {
    bool is_mut;
    TypePtr inner;
};

// Array type: [T; N]
struct ArrayType {
    TypePtr element;
    size_t size;
};

// Slice type: [T]
struct SliceType {
    TypePtr element;
};

// Tuple type: (T, U, V)
struct TupleType {
    std::vector<TypePtr> elements;
};

// Function type: func(A, B) -> R
struct FuncType {
    std::vector<TypePtr> params;
    TypePtr return_type;
    bool is_async;
};

// Captured variable in closure environment
struct CapturedVar {
    std::string name;
    TypePtr type;
    bool is_mut;
};

// Closure type: closure with environment capture
struct ClosureType {
    std::vector<TypePtr> params;
    TypePtr return_type;
    std::vector<CapturedVar> captures;
};

// Type variable (for inference)
struct TypeVar {
    uint32_t id;
    std::optional<TypePtr> bound; // Optional upper bound
};

// Generic parameter
struct GenericType {
    std::string name;
    std::vector<TypePtr> bounds; // Trait bounds
};

// Const generic value - a compile-time constant value used in const generics
// Examples: [T; 10], Array[I32, 5], etc.
struct ConstValue {
    std::variant<int64_t,  // Integer constant
                 uint64_t, // Unsigned integer constant
                 bool,     // Boolean constant
                 char      // Char constant
                 >
        value;
    TypePtr type; // The type of this const value (I32, U64, Bool, etc.)

    // Helper constructors
    static ConstValue from_i64(int64_t v, TypePtr t) {
        return ConstValue{v, std::move(t)};
    }
    static ConstValue from_u64(uint64_t v, TypePtr t) {
        return ConstValue{v, std::move(t)};
    }
    static ConstValue from_bool(bool v, TypePtr t) {
        return ConstValue{v, std::move(t)};
    }
    static ConstValue from_char(char v, TypePtr t) {
        return ConstValue{v, std::move(t)};
    }

    // Get value as integer (for array sizes, etc.)
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

// Const generic parameter - a type-level const value that is not yet known
// Used in generic definitions: func foo[const N: U64](arr: Array[T, N])
struct ConstGenericType {
    std::string name;   // Parameter name (e.g., "N")
    TypePtr value_type; // Type of the const value (e.g., U64, I32)
};

// Dynamic behavior (trait object): dyn Behavior[T]
struct DynBehaviorType {
    std::string behavior_name;      // The behavior this is a trait object of
    std::vector<TypePtr> type_args; // Generic parameters
    bool is_mut;                    // dyn mut Behavior
};

// Impl behavior return type: impl Behavior[T]
// Represents an opaque type that implements a behavior, used for return types
// The concrete type is inferred from the function body
struct ImplBehaviorType {
    std::string behavior_name;      // The behavior being implemented
    std::vector<TypePtr> type_args; // Generic parameters
};

// Type variant
struct Type {
    std::variant<PrimitiveType, NamedType, RefType, PtrType, ArrayType, SliceType, TupleType,
                 FuncType, ClosureType, TypeVar, GenericType, ConstGenericType, DynBehaviorType,
                 ImplBehaviorType>
        kind;

    // Type ID for fast comparison
    uint64_t id = 0;

    template <typename T> [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    template <typename T> [[nodiscard]] auto as() -> T& {
        return std::get<T>(kind);
    }

    template <typename T> [[nodiscard]] auto as() const -> const T& {
        return std::get<T>(kind);
    }
};

// Helper functions
[[nodiscard]] auto make_primitive(PrimitiveKind kind) -> TypePtr;
[[nodiscard]] auto make_unit() -> TypePtr;
[[nodiscard]] auto make_bool() -> TypePtr;
[[nodiscard]] auto make_i32() -> TypePtr;
[[nodiscard]] auto make_i64() -> TypePtr;
[[nodiscard]] auto make_f64() -> TypePtr;
[[nodiscard]] auto make_str() -> TypePtr;
[[nodiscard]] auto make_never() -> TypePtr;
[[nodiscard]] auto make_tuple(std::vector<TypePtr> elements) -> TypePtr;
[[nodiscard]] auto make_func(std::vector<TypePtr> params, TypePtr ret) -> TypePtr;
[[nodiscard]] auto make_closure(std::vector<TypePtr> params, TypePtr ret,
                                std::vector<CapturedVar> captures = {}) -> TypePtr;
[[nodiscard]] auto make_ref(TypePtr inner, bool is_mut = false) -> TypePtr;
[[nodiscard]] auto make_ptr(TypePtr inner, bool is_mut = false) -> TypePtr;
[[nodiscard]] auto make_array(TypePtr element, size_t size) -> TypePtr;
[[nodiscard]] auto make_slice(TypePtr element) -> TypePtr;

// Type comparison
[[nodiscard]] auto types_equal(const TypePtr& a, const TypePtr& b) -> bool;
[[nodiscard]] auto type_to_string(const TypePtr& type) -> std::string;

// Const value comparison and string conversion
[[nodiscard]] auto const_values_equal(const ConstValue& a, const ConstValue& b) -> bool;
[[nodiscard]] auto const_value_to_string(const ConstValue& value) -> std::string;

// Make const generic type
[[nodiscard]] auto make_const_generic(std::string name, TypePtr value_type) -> TypePtr;

// Make impl behavior return type
[[nodiscard]] auto make_impl_behavior(std::string behavior_name,
                                      std::vector<TypePtr> type_args = {}) -> TypePtr;

// Generic type substitution
// Replaces GenericType instances with concrete types from the substitution map
// e.g., substitute_type(List[T], {T -> I32}) returns List[I32]
[[nodiscard]] auto substitute_type(const TypePtr& type,
                                   const std::unordered_map<std::string, TypePtr>& substitutions)
    -> TypePtr;

// Generic type substitution with const generics support
// Replaces both GenericType and ConstGenericType instances
// type_substitutions: maps type param names to types (e.g., T -> I32)
// const_substitutions: maps const param names to values (e.g., N -> 10)
[[nodiscard]] auto substitute_type_with_consts(
    const TypePtr& type, const std::unordered_map<std::string, TypePtr>& type_substitutions,
    const std::unordered_map<std::string, ConstValue>& const_substitutions) -> TypePtr;

// Helper to convert primitive kind to string name
[[nodiscard]] auto primitive_kind_to_string(PrimitiveKind kind) -> std::string;

} // namespace tml::types

#endif // TML_TYPES_TYPE_HPP
