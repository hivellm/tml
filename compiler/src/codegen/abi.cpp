TML_MODULE("compiler")

//! Centralized ABI Classification
//!
//! Implements Win64 calling convention classification for MIR types.
//! This module is the single source of truth for how types are passed
//! across function boundaries (Direct, Indirect, Ignore, Pair).
//!
//! The classification is based on the MIR type variant, not on string
//! matching of LLVM IR type names. This eliminates the fragile
//! `starts_with("%struct.")` pattern scattered throughout codegen.

#include "codegen/abi.hpp"

#include "plugin/module.hpp"

#include <variant>

namespace tml::codegen {

// ============================================================================
// Helpers
// ============================================================================

bool is_power_of_two(size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

/// Map a MIR primitive type to its LLVM IR type string.
/// Mirrors MirCodegen::mir_primitive_to_llvm but is standalone (no MirCodegen dependency).
static auto primitive_to_llvm(mir::PrimitiveType kind) -> std::string {
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

/// Get the size in bytes of a MIR primitive type.
static auto primitive_size(mir::PrimitiveType kind) -> size_t {
    switch (kind) {
    case mir::PrimitiveType::Unit:
        return 0;
    case mir::PrimitiveType::Bool:
        return 1;
    case mir::PrimitiveType::I8:
    case mir::PrimitiveType::U8:
        return 1;
    case mir::PrimitiveType::I16:
    case mir::PrimitiveType::U16:
        return 2;
    case mir::PrimitiveType::I32:
    case mir::PrimitiveType::U32:
        return 4;
    case mir::PrimitiveType::I64:
    case mir::PrimitiveType::U64:
        return 8;
    case mir::PrimitiveType::I128:
    case mir::PrimitiveType::U128:
        return 16;
    case mir::PrimitiveType::F32:
        return 4;
    case mir::PrimitiveType::F64:
        return 8;
    case mir::PrimitiveType::Ptr:
        return 8; // 64-bit target
    case mir::PrimitiveType::Str:
        return 8; // pointer-sized
    default:
        return 0;
    }
}

// ============================================================================
// LLVM Type String Classification (transitional)
// ============================================================================

bool is_aggregate_llvm_type(const std::string& llvm_type) {
    // Check all aggregate type prefixes used in TML's LLVM IR generation.
    // These correspond to named struct types emitted by emit_struct_def / emit_enum_def.
    return llvm_type.starts_with("%struct.") || llvm_type.starts_with("%enum.") ||
           llvm_type.starts_with("%class.") || llvm_type.starts_with("%union.") ||
           llvm_type.starts_with("%tuple.");
}

// ============================================================================
// Type Size Computation
// ============================================================================

size_t compute_type_size(const mir::MirTypePtr& type) {
    if (!type) {
        return 0;
    }

    return std::visit(
        [](const auto& t) -> size_t {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, mir::MirPrimitiveType>) {
                return primitive_size(t.kind);

            } else if constexpr (std::is_same_v<T, mir::MirPointerType>) {
                // ref dyn Behavior -> fat pointer { ptr, ptr } = 16 bytes
                if (t.pointee && std::holds_alternative<mir::MirDynType>(t.pointee->kind)) {
                    return 16;
                }
                return 8; // 64-bit pointer

            } else if constexpr (std::is_same_v<T, mir::MirArrayType>) {
                // Array size = element_size * count
                size_t elem_size = compute_type_size(t.element);
                return elem_size * t.size;

            } else if constexpr (std::is_same_v<T, mir::MirSliceType>) {
                // Slice = { ptr, i64 } = 16 bytes
                return 16;

            } else if constexpr (std::is_same_v<T, mir::MirTupleType>) {
                // Tuple size = sum of element sizes (simplified — no padding)
                // This is an approximation; real layout may have padding.
                // For ABI classification, we use this as a lower bound.
                size_t total = 0;
                for (const auto& elem : t.elements) {
                    total += compute_type_size(elem);
                }
                return total;

            } else if constexpr (std::is_same_v<T, mir::MirStructType>) {
                // Struct types are opaque at MIR level — we don't have field info.
                // Return 0 to signal "unknown size" — caller should use Indirect.
                return 0;

            } else if constexpr (std::is_same_v<T, mir::MirEnumType>) {
                // Enum types are opaque at MIR level — we don't have variant info.
                // Return 0 to signal "unknown size" — caller should use Indirect.
                return 0;

            } else if constexpr (std::is_same_v<T, mir::MirFunctionType>) {
                // Function types are fat pointers: { func_ptr, env_ptr } = 16 bytes
                return 16;

            } else if constexpr (std::is_same_v<T, mir::MirDynType>) {
                // Dyn trait objects are fat pointers: { data_ptr, vtable_ptr } = 16 bytes
                return 16;

            } else if constexpr (std::is_same_v<T, mir::MirVectorType>) {
                // SIMD vector: element_size * width
                size_t elem_size = compute_type_size(t.element);
                return elem_size * t.width;

            } else {
                return 0;
            }
        },
        type->kind);
}

// ============================================================================
// Type Classification
// ============================================================================

ArgABI classify_type(const mir::MirTypePtr& type) {
    if (!type) {
        return ArgABI{PassMode::Ignore, "void", false};
    }

    return std::visit(
        [&type](const auto& t) -> ArgABI {
            using T = std::decay_t<decltype(t)>;

            if constexpr (std::is_same_v<T, mir::MirPrimitiveType>) {
                // Unit -> Ignore
                if (t.kind == mir::PrimitiveType::Unit) {
                    return ArgABI{PassMode::Ignore, "void", false};
                }
                // All other primitives (i8-i128, f32, f64, ptr, str, bool) -> Direct
                return ArgABI{PassMode::Direct, primitive_to_llvm(t.kind), false};

            } else if constexpr (std::is_same_v<T, mir::MirPointerType>) {
                // ref dyn Behavior -> Pair (fat pointer: data + vtable)
                if (t.pointee && std::holds_alternative<mir::MirDynType>(t.pointee->kind)) {
                    return ArgABI{PassMode::Pair, "{ ptr, ptr }", false};
                }
                // Regular pointer -> Direct
                return ArgABI{PassMode::Direct, "ptr", false};

            } else if constexpr (std::is_same_v<T, mir::MirArrayType>) {
                // Arrays are always passed indirectly (by pointer).
                // Even small arrays — consistent with Rust/clang behavior.
                // The LLVM type string uses the bracket notation: [N x T]
                return ArgABI{PassMode::Indirect, "ptr", false};

            } else if constexpr (std::is_same_v<T, mir::MirSliceType>) {
                // Slice = fat pointer { ptr, i64 } -> Pair
                return ArgABI{PassMode::Pair, "{ ptr, i64 }", false};

            } else if constexpr (std::is_same_v<T, mir::MirTupleType>) {
                // Tuples: check aggregate size for Win64 register passing.
                size_t size = compute_type_size(type);
                if (size > 0 && size <= 8 && is_power_of_two(size)) {
                    // Small tuple that fits in a register.
                    // Build the LLVM type string.
                    std::string llvm_str = "{ ";
                    for (size_t i = 0; i < t.elements.size(); ++i) {
                        if (i > 0)
                            llvm_str += ", ";
                        // Recursively get the LLVM type of each element
                        ArgABI elem_abi = classify_type(t.elements[i]);
                        llvm_str += elem_abi.llvm_type;
                    }
                    llvm_str += " }";
                    return ArgABI{PassMode::Direct, llvm_str, false};
                }
                // Large tuple -> Indirect
                return ArgABI{PassMode::Indirect, "ptr", false};

            } else if constexpr (std::is_same_v<T, mir::MirStructType>) {
                // Structs are opaque at MIR level (no field info available).
                // We cannot compute their size, so we conservatively classify as Indirect.
                // This matches the current behavior where %struct.* types are passed by pointer.
                //
                // Future optimization: if struct definitions are available (from mir::Module),
                // compute actual field layout and promote small structs to Direct.
                return ArgABI{PassMode::Indirect, "ptr", false};

            } else if constexpr (std::is_same_v<T, mir::MirEnumType>) {
                // Enums are opaque at MIR level (no variant info available).
                // Conservatively classify as Indirect.
                return ArgABI{PassMode::Indirect, "ptr", false};

            } else if constexpr (std::is_same_v<T, mir::MirFunctionType>) {
                // Function types are fat pointers: { func_ptr, env_ptr } -> Pair
                // This supports both plain function pointers and capturing closures.
                return ArgABI{PassMode::Pair, "{ ptr, ptr }", false};

            } else if constexpr (std::is_same_v<T, mir::MirDynType>) {
                // Dyn trait objects are fat pointers: { data_ptr, vtable_ptr } -> Pair
                return ArgABI{PassMode::Pair, "{ ptr, ptr }", false};

            } else if constexpr (std::is_same_v<T, mir::MirVectorType>) {
                // SIMD vectors are passed directly in vector registers.
                // Build the LLVM vector type string: <N x T>
                ArgABI elem_abi = classify_type(t.element);
                std::string llvm_str =
                    "<" + std::to_string(t.width) + " x " + elem_abi.llvm_type + ">";
                return ArgABI{PassMode::Direct, llvm_str, false};

            } else {
                // Unknown variant — should not happen if all MirType variants are covered.
                return ArgABI{PassMode::Ignore, "void", false};
            }
        },
        type->kind);
}

// ============================================================================
// Return Type Classification
// ============================================================================

ArgABI classify_return(const mir::MirTypePtr& type) {
    ArgABI result = classify_type(type);

    // For Indirect returns, the caller allocates space and passes a hidden
    // pointer as the first argument (sret convention).
    if (result.mode == PassMode::Indirect) {
        result.sret = true;
    }

    return result;
}

// ============================================================================
// Full Function ABI Computation
// ============================================================================

FnABI compute_fn_abi(const mir::Function& func) {
    FnABI abi;

    // Classify return type.
    // If the function already has sret annotated (from MIR building), respect that.
    if (func.uses_sret && func.original_return_type) {
        // The MIR builder already determined this needs sret.
        // Use the original return type for classification, but force Indirect+sret.
        ArgABI ret_abi = classify_type(func.original_return_type);
        ret_abi.mode = PassMode::Indirect;
        ret_abi.sret = true;
        abi.ret = ret_abi;
    } else {
        abi.ret = classify_return(func.return_type);
    }

    // Classify each parameter.
    for (const auto& param : func.params) {
        // Skip the sret parameter — it's part of the return convention, not a real arg.
        if (func.uses_sret && param.value_id == func.sret_param_id) {
            continue;
        }

        ArgABI arg_abi = classify_type(param.type);
        abi.args.push_back(arg_abi);
    }

    return abi;
}

} // namespace tml::codegen
