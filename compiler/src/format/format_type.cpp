//! # Type Formatting
//!
//! This file implements formatting for type annotations.
//!
//! ## Type Kinds
//!
//! | Type       | Example                    |
//! |------------|----------------------------|
//! | Named      | `I32`, `Vec[T]`            |
//! | Reference  | `ref T`, `mut ref T`       |
//! | Pointer    | `ptr T`, `mut ptr T`       |
//! | Array      | `[I32; 10]`                |
//! | Slice      | `[I32]`                    |
//! | Tuple      | `(I32, String)`            |
//! | Function   | `func(I32) -> Bool`        |
//! | Inferred   | `_`                        |

#include "format/formatter.hpp"

namespace tml::format {

auto Formatter::format_type(const parser::Type& type) -> std::string {
    if (type.is<parser::NamedType>()) {
        const auto& named = type.as<parser::NamedType>();
        std::string result = format_type_path(named.path);
        if (named.generics.has_value()) {
            result += "[";
            for (size_t i = 0; i < named.generics->args.size(); ++i) {
                if (i > 0)
                    result += ", ";
                const auto& arg = named.generics->args[i];
                if (arg.is_type()) {
                    result += format_type_ptr(arg.as_type());
                } else {
                    // Const generic argument - format as expression
                    result += format_expr(*arg.as_expr());
                }
            }
            result += "]";
        }
        return result;
    } else if (type.is<parser::RefType>()) {
        const auto& ref = type.as<parser::RefType>();
        return (ref.is_mut ? "mut ref " : "ref ") + format_type_ptr(ref.inner);
    } else if (type.is<parser::PtrType>()) {
        const auto& ptr = type.as<parser::PtrType>();
        return (ptr.is_mut ? "mut ptr " : "ptr ") + format_type_ptr(ptr.inner);
    } else if (type.is<parser::ArrayType>()) {
        const auto& arr = type.as<parser::ArrayType>();
        return "[" + format_type_ptr(arr.element) + "; " + format_expr(*arr.size) + "]";
    } else if (type.is<parser::SliceType>()) {
        const auto& slice = type.as<parser::SliceType>();
        return "[" + format_type_ptr(slice.element) + "]";
    } else if (type.is<parser::TupleType>()) {
        const auto& tuple = type.as<parser::TupleType>();
        std::string result = "(";
        for (size_t i = 0; i < tuple.elements.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += format_type_ptr(tuple.elements[i]);
        }
        result += ")";
        return result;
    } else if (type.is<parser::FuncType>()) {
        const auto& func = type.as<parser::FuncType>();
        std::string result = "func(";
        for (size_t i = 0; i < func.params.size(); ++i) {
            if (i > 0)
                result += ", ";
            result += format_type_ptr(func.params[i]);
        }
        result += ")";
        if (func.return_type) {
            result += " -> " + format_type_ptr(func.return_type);
        }
        return result;
    } else if (type.is<parser::InferType>()) {
        return "_";
    }

    return "?";
}

auto Formatter::format_type_ptr(const parser::TypePtr& type) -> std::string {
    if (!type)
        return "_";
    return format_type(*type);
}

auto Formatter::format_type_path(const parser::TypePath& path) -> std::string {
    std::string result;
    for (size_t i = 0; i < path.segments.size(); ++i) {
        if (i > 0)
            result += "::";
        result += path.segments[i];
    }
    return result;
}

} // namespace tml::format
