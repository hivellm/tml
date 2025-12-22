#ifndef TML_TYPES_TYPE_HPP
#define TML_TYPES_TYPE_HPP

#include "tml/common.hpp"
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace tml::types {

// Forward declarations
struct Type;
using TypePtr = std::shared_ptr<Type>;

// Primitive types
enum class PrimitiveKind {
    // Integers
    I8, I16, I32, I64, I128,
    U8, U16, U32, U64, U128,
    // Floats
    F32, F64,
    // Other primitives
    Bool,
    Char,   // Unicode scalar
    Str,    // String slice
    Unit,   // ()
    Never,  // ! (never returns)
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

// Type variant
struct Type {
    std::variant<
        PrimitiveType,
        NamedType,
        RefType,
        PtrType,
        ArrayType,
        SliceType,
        TupleType,
        FuncType,
        TypeVar,
        GenericType
    > kind;

    // Type ID for fast comparison
    uint64_t id = 0;

    template<typename T>
    [[nodiscard]] auto is() const -> bool {
        return std::holds_alternative<T>(kind);
    }

    template<typename T>
    [[nodiscard]] auto as() -> T& {
        return std::get<T>(kind);
    }

    template<typename T>
    [[nodiscard]] auto as() const -> const T& {
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
[[nodiscard]] auto make_ref(TypePtr inner, bool is_mut = false) -> TypePtr;
[[nodiscard]] auto make_array(TypePtr element, size_t size) -> TypePtr;
[[nodiscard]] auto make_slice(TypePtr element) -> TypePtr;

// Type comparison
[[nodiscard]] auto types_equal(const TypePtr& a, const TypePtr& b) -> bool;
[[nodiscard]] auto type_to_string(const TypePtr& type) -> std::string;

} // namespace tml::types

#endif // TML_TYPES_TYPE_HPP
