TML_MODULE("codegen_x86")

//! MIR Codegen Type Conversion
//!
//! This file contains type conversion methods for the MIR-based code generator:
//! - mir_type_to_llvm: Converts MIR types to LLVM IR type strings
//! - mir_primitive_to_llvm: Converts primitive types to LLVM type strings

#include "codegen/mir_codegen.hpp"

namespace tml::codegen {

auto MirCodegen::mir_type_to_llvm(const mir::MirTypePtr& type) -> std::string {
    if (!type) {
        return "void";
    }

    return std::visit(
        [this](const auto& t) -> std::string {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, mir::MirPrimitiveType>) {
                return mir_primitive_to_llvm(t.kind);

            } else if constexpr (std::is_same_v<T, mir::MirPointerType>) {
                return "ptr";

            } else if constexpr (std::is_same_v<T, mir::MirArrayType>) {
                return "[" + std::to_string(t.size) + " x " + mir_type_to_llvm(t.element) + "]";

            } else if constexpr (std::is_same_v<T, mir::MirSliceType>) {
                // Slice is { ptr, i64 }
                return "{ ptr, i64 }";

            } else if constexpr (std::is_same_v<T, mir::MirTupleType>) {
                std::string result = "{ ";
                for (size_t i = 0; i < t.elements.size(); ++i) {
                    if (i > 0) {
                        result += ", ";
                    }
                    result += mir_type_to_llvm(t.elements[i]);
                }
                result += " }";
                return result;

            } else if constexpr (std::is_same_v<T, mir::MirStructType>) {
                return "%struct." + t.name;

            } else if constexpr (std::is_same_v<T, mir::MirEnumType>) {
                // Mangle name with type args to match legacy codegen convention
                // e.g., Maybe + [Str] -> Maybe__Str, Outcome + [I32, Str] -> Outcome__I32_Str
                std::string mangled = t.name;
                if (!t.type_args.empty()) {
                    mangled += "__";
                    for (size_t i = 0; i < t.type_args.size(); ++i) {
                        if (i > 0)
                            mangled += "_";
                        mangled += mangle_mir_type_arg(t.type_args[i]);
                    }
                }
                // Use %struct. prefix for consistency with legacy codegen
                return "%struct." + mangled;

            } else if constexpr (std::is_same_v<T, mir::MirFunctionType>) {
                // Function types are fat pointers: { func_ptr, env_ptr }
                // to support both plain function pointers and capturing closures
                return "{ ptr, ptr }";
            } else {
                // Should not be reached if all variant types are handled
                return "void";
            }
        },
        type->kind);
}

auto MirCodegen::mir_primitive_to_llvm(mir::PrimitiveType kind) -> std::string {
    switch (kind) {
    case mir::PrimitiveType::Unit:
        return "void";
    case mir::PrimitiveType::Bool:
        return "i1";
    case mir::PrimitiveType::I8:
    case mir::PrimitiveType::U8:
        return "i8";
    case mir::PrimitiveType::I16:
    case mir::PrimitiveType::U16:
        return "i16";
    case mir::PrimitiveType::I32:
    case mir::PrimitiveType::U32:
        return "i32";
    case mir::PrimitiveType::I64:
    case mir::PrimitiveType::U64:
        return "i64";
    case mir::PrimitiveType::I128:
    case mir::PrimitiveType::U128:
        return "i128";
    case mir::PrimitiveType::F32:
        return "float";
    case mir::PrimitiveType::F64:
        return "double";
    case mir::PrimitiveType::Ptr:
        return "ptr";
    case mir::PrimitiveType::Str:
        return "ptr"; // Strings are represented as pointers
    default:
        return "void";
    }
}

auto MirCodegen::mangle_mir_type_arg(const mir::MirTypePtr& type) -> std::string {
    if (!type)
        return "void";

    return std::visit(
        [this](const auto& t) -> std::string {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, mir::MirPrimitiveType>) {
                switch (t.kind) {
                case mir::PrimitiveType::Bool:
                    return "Bool";
                case mir::PrimitiveType::I8:
                    return "I8";
                case mir::PrimitiveType::U8:
                    return "U8";
                case mir::PrimitiveType::I16:
                    return "I16";
                case mir::PrimitiveType::U16:
                    return "U16";
                case mir::PrimitiveType::I32:
                    return "I32";
                case mir::PrimitiveType::U32:
                    return "U32";
                case mir::PrimitiveType::I64:
                    return "I64";
                case mir::PrimitiveType::U64:
                    return "U64";
                case mir::PrimitiveType::I128:
                    return "I128";
                case mir::PrimitiveType::U128:
                    return "U128";
                case mir::PrimitiveType::F32:
                    return "F32";
                case mir::PrimitiveType::F64:
                    return "F64";
                case mir::PrimitiveType::Str:
                    return "Str";
                case mir::PrimitiveType::Ptr:
                    return "Ptr";
                default:
                    return "Unknown";
                }
            } else if constexpr (std::is_same_v<T, mir::MirPointerType>) {
                return "ref_" + mangle_mir_type_arg(t.pointee);
            } else if constexpr (std::is_same_v<T, mir::MirStructType>) {
                std::string result = t.name;
                if (!t.type_args.empty()) {
                    result += "__";
                    for (size_t i = 0; i < t.type_args.size(); ++i) {
                        if (i > 0)
                            result += "_";
                        result += mangle_mir_type_arg(t.type_args[i]);
                    }
                }
                return result;
            } else if constexpr (std::is_same_v<T, mir::MirEnumType>) {
                std::string result = t.name;
                if (!t.type_args.empty()) {
                    result += "__";
                    for (size_t i = 0; i < t.type_args.size(); ++i) {
                        if (i > 0)
                            result += "_";
                        result += mangle_mir_type_arg(t.type_args[i]);
                    }
                }
                return result;
            } else {
                return "Unknown";
            }
        },
        type->kind);
}

void MirCodegen::collect_enum_types_from_type(const mir::MirTypePtr& type) {
    if (!type)
        return;

    std::visit(
        [this](const auto& t) {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, mir::MirEnumType>) {
                // Build mangled name
                std::string mangled = t.name;
                if (!t.type_args.empty()) {
                    mangled += "__";
                    for (size_t i = 0; i < t.type_args.size(); ++i) {
                        if (i > 0)
                            mangled += "_";
                        mangled += mangle_mir_type_arg(t.type_args[i]);
                    }
                }

                // Calculate max payload size from type args
                size_t payload_size = 0;
                for (const auto& arg : t.type_args) {
                    if (arg) {
                        if (arg->is_integer()) {
                            payload_size += arg->bit_width() / 8;
                        } else if (arg->is_float()) {
                            payload_size += arg->bit_width() / 8;
                        } else if (arg->is_bool()) {
                            payload_size += 1;
                        } else {
                            payload_size += 8; // Default (ptr, str, etc.)
                        }
                    }
                }
                if (payload_size < 8)
                    payload_size = 8;

                generic_enum_defs_[mangled] = payload_size;

                // Recurse into type args
                for (const auto& arg : t.type_args) {
                    collect_enum_types_from_type(arg);
                }
            } else if constexpr (std::is_same_v<T, mir::MirPointerType>) {
                collect_enum_types_from_type(t.pointee);
            } else if constexpr (std::is_same_v<T, mir::MirArrayType>) {
                collect_enum_types_from_type(t.element);
            } else if constexpr (std::is_same_v<T, mir::MirSliceType>) {
                collect_enum_types_from_type(t.element);
            } else if constexpr (std::is_same_v<T, mir::MirTupleType>) {
                for (const auto& elem : t.elements) {
                    collect_enum_types_from_type(elem);
                }
            } else if constexpr (std::is_same_v<T, mir::MirFunctionType>) {
                collect_enum_types_from_type(t.return_type);
                for (const auto& param : t.params) {
                    collect_enum_types_from_type(param);
                }
            }
        },
        type->kind);
}

} // namespace tml::codegen
