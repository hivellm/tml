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
                return "%enum." + t.name;

            } else if constexpr (std::is_same_v<T, mir::MirFunctionType>) {
                std::string result = mir_type_to_llvm(t.return_type) + " (";
                for (size_t i = 0; i < t.params.size(); ++i) {
                    if (i > 0) {
                        result += ", ";
                    }
                    result += mir_type_to_llvm(t.params[i]);
                }
                result += ")*";
                return result;
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

} // namespace tml::codegen
