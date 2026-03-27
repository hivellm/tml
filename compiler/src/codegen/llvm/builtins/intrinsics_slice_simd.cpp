TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Slice, Array, Type Info, SIMD, and SSE2 Intrinsics
//!
//! This file implements the second half of the compiler intrinsic dispatch
//! table. Split from intrinsics.cpp for maintainability.
//!
//! ## Sections in this file
//!
//! - Slice Intrinsics (slice_get, slice_get_mut, slice_set, slice_offset, slice_swap)
//! - Array Intrinsics (array_as_ptr, array_as_mut_ptr, array_offset_ptr, array_take, etc.)
//! - Type Information Intrinsics (size_of, align_of, sizeof_type, type_id)
//! - Unsafe Conversions (transmute, cast)
//! - SIMD Vector Intrinsics (simd_load, simd_store, simd_extract, simd_insert, simd_splat, etc.)
//! - Native SSE2 Intrinsics (sse2_cmpeq_epi8, sse2_movemask_epi8)

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

/// Handles Slice, Array, Type Information, Unsafe Conversion, SIMD, and SSE2
/// intrinsics. Called from try_gen_intrinsic() after the Arithmetic, Comparison,
/// Bitwise, and Memory sections have been checked.
///
/// Parameters follow the same convention as try_gen_intrinsic:
///   intrinsic_name — base name with module prefix stripped (e.g. "slice_get")
///   fn_name        — the raw qualified function name passed by the caller
///   call           — the parsed call expression including arguments and callee
auto LLVMIRGen::try_gen_intrinsic_slice_simd(const std::string& intrinsic_name,
                                             const std::string& fn_name,
                                             const parser::CallExpr& call)
    -> std::optional<std::string> {

    // ============================================================================
    // Slice Intrinsics
    // ============================================================================

    // slice_get[T](data: ref T, index: I64) -> ref T
    // Returns a reference to element at index
    if (intrinsic_name == "slice_get") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);
            std::string data_type = last_expr_type_; // ptr

            // Infer element type from the first argument's semantic type
            std::string elem_type = "i8"; // Default
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            }

            std::string index = gen_expr(*call.args[1]);

            // GEP to compute address: data + index * sizeof(T)
            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr inbounds " + elem_type + ", ptr " + data +
                      ", i64 " + index);
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // slice_get_mut[T](data: mut ref T, index: I64) -> mut ref T
    // Same as slice_get but for mutable references
    if (intrinsic_name == "slice_get_mut") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);
            std::string data_type = last_expr_type_;

            // Infer element type
            std::string elem_type = "i8";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            } else if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            }

            std::string index = gen_expr(*call.args[1]);

            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr inbounds " + elem_type + ", ptr " + data +
                      ", i64 " + index);
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // slice_set[T](data: mut ref T, index: I64, value: T)
    // Sets element at index to value
    if (intrinsic_name == "slice_set") {
        if (call.args.size() >= 3) {
            std::string data = gen_expr(*call.args[0]);

            // Infer element type
            std::string elem_type = "i8";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            } else if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            }

            std::string index = gen_expr(*call.args[1]);
            std::string value = gen_expr(*call.args[2]);
            std::string value_type = last_expr_type_;

            // Compute address and store
            std::string addr = fresh_reg();
            emit_line("  " + addr + " = getelementptr inbounds " + elem_type + ", ptr " + data +
                      ", i64 " + index);
            emit_line("  store " + value_type + " " + value + ", ptr " + addr);
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // slice_offset[T](data: ref T, count: I64) -> ref T
    // Returns pointer offset by count elements
    if (intrinsic_name == "slice_offset") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);

            // Infer element type
            std::string elem_type = "i8";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            } else if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            }

            std::string count = gen_expr(*call.args[1]);

            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr inbounds " + elem_type + ", ptr " + data +
                      ", i64 " + count);
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // slice_swap[T](data: mut ref T, a: I64, b: I64)
    // Swaps elements at indices a and b
    if (intrinsic_name == "slice_swap") {
        if (call.args.size() >= 3) {
            std::string data = gen_expr(*call.args[0]);

            // Infer element type
            std::string elem_type = "i8";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            } else if (arg_type->is<types::RefType>()) {
                elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
            }

            std::string idx_a = gen_expr(*call.args[1]);
            std::string idx_b = gen_expr(*call.args[2]);

            // Compute addresses
            std::string addr_a = fresh_reg();
            std::string addr_b = fresh_reg();
            emit_line("  " + addr_a + " = getelementptr inbounds " + elem_type + ", ptr " + data +
                      ", i64 " + idx_a);
            emit_line("  " + addr_b + " = getelementptr inbounds " + elem_type + ", ptr " + data +
                      ", i64 " + idx_b);

            // Load both values
            std::string val_a = fresh_reg();
            std::string val_b = fresh_reg();
            emit_line("  " + val_a + " = load " + elem_type + ", ptr " + addr_a);
            emit_line("  " + val_b + " = load " + elem_type + ", ptr " + addr_b);

            // Store swapped
            emit_line("  store " + elem_type + " " + val_b + ", ptr " + addr_a);
            emit_line("  store " + elem_type + " " + val_a + ", ptr " + addr_b);

            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // ============================================================================
    // Array Intrinsics
    // ============================================================================

    // array_as_ptr[T](data: T) -> ref T
    // Returns a pointer to the first element of an array (for creating slices)
    if (intrinsic_name == "array_as_ptr") {
        if (!call.args.empty()) {
            // The argument should be an array field (like this.data)
            // We just need to get its address
            std::string arr = gen_expr(*call.args[0]);
            // For arrays in locals, gen_expr returns the alloca pointer
            // which is already what we need
            last_expr_type_ = "ptr";
            return arr;
        }
        return "null";
    }

    // array_as_mut_ptr[T](data: T) -> mut ref T
    // Same as array_as_ptr but for mutable references
    if (intrinsic_name == "array_as_mut_ptr") {
        if (!call.args.empty()) {
            std::string arr = gen_expr(*call.args[0]);
            last_expr_type_ = "ptr";
            return arr;
        }
        return "null";
    }

    // array_offset_ptr[T](data: ref T, count: I64) -> ref T
    // Computes an offset pointer within an array
    if (intrinsic_name == "array_offset_ptr") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);

            // Infer element type from the first argument
            std::string elem_type = "i8";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type) {
                if (arg_type->is<types::RefType>()) {
                    elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
                } else if (arg_type->is<types::RefType>()) {
                    elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
                }
            }

            std::string count = gen_expr(*call.args[1]);

            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr inbounds " + elem_type + ", ptr " + data +
                      ", i64 " + count);
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // array_offset_mut_ptr[T](data: mut ref T, count: I64) -> mut ref T
    // Same as array_offset_ptr but for mutable references
    if (intrinsic_name == "array_offset_mut_ptr") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);

            // Infer element type
            std::string elem_type = "i8";
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type) {
                if (arg_type->is<types::RefType>()) {
                    elem_type = llvm_type_from_semantic(arg_type->as<types::RefType>().inner);
                } else if (arg_type->is<types::PtrType>()) {
                    elem_type = llvm_type_from_semantic(arg_type->as<types::PtrType>().inner);
                }
            }

            std::string count = gen_expr(*call.args[1]);

            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr inbounds " + elem_type + ", ptr " + data +
                      ", i64 " + count);
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // array_take[T](data: [T; N], index: I64) -> T
    // Extracts an element from an array at a dynamic index (move semantics)
    if (intrinsic_name == "array_take") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);
            std::string data_type = last_expr_type_;
            std::string index = gen_expr(*call.args[1]);

            // Infer element type from the array type (e.g., "[3 x i32]" -> "i32")
            std::string elem_type = "i64"; // default
            if (data_type.size() > 3 && data_type[0] == '[') {
                auto x_pos = data_type.find(" x ");
                if (x_pos != std::string::npos) {
                    elem_type = data_type.substr(x_pos + 3);
                    if (!elem_type.empty() && elem_type.back() == ']') {
                        elem_type.pop_back();
                    }
                }
            }

            // Alloca the array, GEP to element, load
            std::string arr_ptr = fresh_reg();
            emit_line("  " + arr_ptr + " = alloca " + data_type);
            emit_line("  store " + data_type + " " + data + ", ptr " + arr_ptr);
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr inbounds " + data_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + index);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + elem_type + ", ptr " + elem_ptr);
            last_expr_type_ = elem_type;
            return result;
        }
        return "0";
    }

    // array_get[T](data: [T; N], index: I64) -> T
    // Same as array_take but with copy semantics
    if (intrinsic_name == "array_get") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);
            std::string data_type = last_expr_type_;
            std::string index = gen_expr(*call.args[1]);

            std::string elem_type = "i64";
            if (data_type.size() > 3 && data_type[0] == '[') {
                auto x_pos = data_type.find(" x ");
                if (x_pos != std::string::npos) {
                    elem_type = data_type.substr(x_pos + 3);
                    if (!elem_type.empty() && elem_type.back() == ']') {
                        elem_type.pop_back();
                    }
                }
            }

            std::string arr_ptr = fresh_reg();
            emit_line("  " + arr_ptr + " = alloca " + data_type);
            emit_line("  store " + data_type + " " + data + ", ptr " + arr_ptr);
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr inbounds " + data_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + index);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load " + elem_type + ", ptr " + elem_ptr);
            last_expr_type_ = elem_type;
            return result;
        }
        return "0";
    }

    // array_get_ref[T](data: [T; N], index: I64) -> ref T
    // Returns a pointer to an element in an array
    if (intrinsic_name == "array_get_ref") {
        if (call.args.size() >= 2) {
            std::string data = gen_expr(*call.args[0]);
            std::string data_type = last_expr_type_;
            std::string index = gen_expr(*call.args[1]);

            std::string arr_ptr = fresh_reg();
            emit_line("  " + arr_ptr + " = alloca " + data_type);
            emit_line("  store " + data_type + " " + data + ", ptr " + arr_ptr);
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr inbounds " + data_type + ", ptr " +
                      arr_ptr + ", i64 0, i64 " + index);
            last_expr_type_ = "ptr";
            return elem_ptr;
        }
        return "null";
    }

    // array_set[T](data: [T; N], index: I64, value: T) -> void
    // Sets an element in an array at a dynamic index
    if (intrinsic_name == "array_set") {
        if (call.args.size() >= 3) {
            std::string data = gen_expr(*call.args[0]);
            std::string data_type = last_expr_type_;
            std::string index = gen_expr(*call.args[1]);
            std::string value = gen_expr(*call.args[2]);
            std::string val_type = last_expr_type_;

            // For array_set, data should be a pointer to the array (from struct field)
            // We GEP into the array and store
            std::string elem_ptr = fresh_reg();
            emit_line("  " + elem_ptr + " = getelementptr inbounds " + data_type + ", ptr " + data +
                      ", i64 0, i64 " + index);
            emit_line("  store " + val_type + " " + value + ", ptr " + elem_ptr);
            last_expr_type_ = "void";
            return "";
        }
        return "";
    }

    // array_uninit[T; N]() -> [T; N]
    // Creates an uninitialized array (undef)
    if (intrinsic_name == "array_uninit") {
        // Try to get the type from the variable being assigned to
        // For now, return undef — the type will be resolved by the caller
        last_expr_type_ = "i8"; // placeholder, will be overridden by assignment context
        return "undef";
    }

    // ============================================================================
    // Type Information Intrinsics
    // ============================================================================

    // size_of[T]() -> I64
    if (intrinsic_name == "size_of") {
        std::string type_llvm = "i64"; // Default
        int size_bytes = 8;            // Default

        // Try to extract type argument from PathExpr generics (e.g., size_of[I32]())
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    // Resolve the type, using current type substitutions
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    type_llvm = llvm_type_from_semantic(resolved);

                    // Calculate size based on LLVM type
                    if (type_llvm == "i8")
                        size_bytes = 1;
                    else if (type_llvm == "i16")
                        size_bytes = 2;
                    else if (type_llvm == "i32" || type_llvm == "float")
                        size_bytes = 4;
                    else if (type_llvm == "i64" || type_llvm == "double" || type_llvm == "ptr")
                        size_bytes = 8;
                    else if (type_llvm == "i128")
                        size_bytes = 16;
                    else if (type_llvm == "i1")
                        size_bytes = 1;
                    else if (type_llvm.starts_with("%struct.") ||
                             type_llvm.starts_with("%class.")) {
                        // For structs, use GEP trick to get size
                        std::string size_ptr = fresh_reg();
                        std::string size_val = fresh_reg();
                        emit_line("  " + size_ptr + " = getelementptr inbounds " + type_llvm +
                                  ", ptr null, i32 1");
                        emit_line("  " + size_val + " = ptrtoint ptr " + size_ptr + " to i64");
                        last_expr_type_ = "i64";
                        return size_val;
                    }
                }
            }
        }

        last_expr_type_ = "i64";
        return std::to_string(size_bytes);
    }

    // align_of[T]() / alignof_type[T]() -> I64
    if (intrinsic_name == "align_of" || fn_name == "alignof_type") {
        int align_bytes = 8; // Default

        // Try to extract type argument from PathExpr generics
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    std::string type_llvm = llvm_type_from_semantic(resolved);

                    // Calculate alignment based on LLVM type
                    if (type_llvm == "i8" || type_llvm == "i1")
                        align_bytes = 1;
                    else if (type_llvm == "i16")
                        align_bytes = 2;
                    else if (type_llvm == "i32" || type_llvm == "float")
                        align_bytes = 4;
                    else if (type_llvm == "i64" || type_llvm == "double" || type_llvm == "ptr")
                        align_bytes = 8;
                    else if (type_llvm == "i128")
                        align_bytes = 16;
                    // For structs/classes, use 8 as default (pointer alignment)
                }
            }
        }

        last_expr_type_ = "i64";
        return std::to_string(align_bytes);
    }

    // sizeof_type[T]() -> I64 (alias for size_of)
    if (intrinsic_name == "sizeof_type") {
        // Reuse size_of logic - same implementation needed
        int size_bytes = 8;

        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    std::string type_llvm = llvm_type_from_semantic(resolved);

                    if (type_llvm == "i8")
                        size_bytes = 1;
                    else if (type_llvm == "i16")
                        size_bytes = 2;
                    else if (type_llvm == "i32" || type_llvm == "float")
                        size_bytes = 4;
                    else if (type_llvm == "i64" || type_llvm == "double" || type_llvm == "ptr")
                        size_bytes = 8;
                    else if (type_llvm == "i128")
                        size_bytes = 16;
                    else if (type_llvm == "i1")
                        size_bytes = 1;
                    else if (type_llvm.starts_with("%struct.") ||
                             type_llvm.starts_with("%class.")) {
                        std::string size_ptr = fresh_reg();
                        std::string size_val = fresh_reg();
                        emit_line("  " + size_ptr + " = getelementptr inbounds " + type_llvm +
                                  ", ptr null, i32 1");
                        emit_line("  " + size_val + " = ptrtoint ptr " + size_ptr + " to i64");
                        last_expr_type_ = "i64";
                        return size_val;
                    }
                }
            }
        }

        last_expr_type_ = "i64";
        return std::to_string(size_bytes);
    }

    // type_id[T]() -> U64
    // Returns a unique ID for each monomorphized type
    if (intrinsic_name == "type_id") {
        // Get type argument from call
        std::string type_name = "unknown";

        // Try to extract type argument from PathExpr generics (e.g., type_id[I32]())
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics && !path_expr.generics->args.empty()) {
                const auto& first_arg = path_expr.generics->args[0];
                if (first_arg.is_type()) {
                    // Use current_type_subs_ so that generic type params (e.g. T inside
                    // downcast[T]) are resolved to their concrete types (e.g. I32) before
                    // hashing. Without this, type_id[T]() always hashes "T" -> collision.
                    types::TypePtr type_arg =
                        resolve_parser_type_with_subs(*first_arg.as_type(), current_type_subs_);
                    type_name = mangle_type(type_arg);
                }
            }
        } else if (call.callee->is<parser::IdentExpr>()) {
            // Try to look up in current context if type_id called with turbofish later
            // For now, just handle the direct case
        }

        // Generate a stable hash from the type name
        // Use FNV-1a hash for stability across compilations
        uint64_t hash = 14695981039346656037ULL; // FNV-1a offset basis
        for (char c : type_name) {
            hash ^= static_cast<uint64_t>(c);
            hash *= 1099511628211ULL; // FNV-1a prime
        }

        last_expr_type_ = "i64";
        return std::to_string(hash);
    }

    // ============================================================================
    // Unsafe Conversions
    // ============================================================================

    // transmute[T, U](val: T) -> U
    if (intrinsic_name == "transmute") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;

            // For now, just bitcast - in practice would validate sizes match
            std::string result = fresh_reg();
            // Bitcast requires same size, so we just return the value
            // A full implementation would validate types
            last_expr_type_ = val_type;
            return val;
        }
        return "0";
    }

    // cast[T, U](val: T) -> U
    if (intrinsic_name == "cast") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            // Cast would need type argument resolution
            // For now, just return the value
            return val;
        }
        return "0";
    }

    // ============================================================================
    // SIMD Vector Intrinsics
    // ============================================================================

    // Helper lambda: resolve SIMD type name from the first generic type argument [V]
    auto resolve_simd_from_generics = [&](int arg_index =
                                              0) -> std::pair<std::string, const SimdTypeInfo*> {
        if (call.callee->is<parser::PathExpr>()) {
            const auto& path_expr = call.callee->as<parser::PathExpr>();
            if (path_expr.generics &&
                static_cast<int>(path_expr.generics->args.size()) > arg_index) {
                const auto& type_arg = path_expr.generics->args[arg_index];
                if (type_arg.is_type()) {
                    auto resolved =
                        resolve_parser_type_with_subs(*type_arg.as_type(), current_type_subs_);
                    if (resolved && resolved->is<types::NamedType>()) {
                        const auto& name = resolved->as<types::NamedType>().name;
                        auto it = simd_types_.find(name);
                        if (it != simd_types_.end()) {
                            return {name, &it->second};
                        }
                    }
                }
            }
        }
        return {"", nullptr};
    };

    // simd_load[V](ptr: ref V) -> V
    // Loads the entire @simd struct as a raw LLVM vector value.
    if (intrinsic_name == "simd_load") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);

            auto [name, info] = resolve_simd_from_generics();
            if (info) {
                std::string vec_type = simd_vec_type_str(*info);
                std::string result = fresh_reg();
                emit_line("  " + result + " = load " + vec_type + ", ptr " + ptr);
                last_expr_type_ = vec_type;
                return result;
            }

            // Fallback: try to infer from argument's semantic type
            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
            if (arg_type) {
                const types::Type* inner_type = nullptr;
                if (arg_type->is<types::RefType>()) {
                    inner_type = arg_type->as<types::RefType>().inner.get();
                }
                if (inner_type && inner_type->is<types::NamedType>()) {
                    const auto& sname = inner_type->as<types::NamedType>().name;
                    auto it = simd_types_.find(sname);
                    if (it != simd_types_.end()) {
                        std::string vec_type = simd_vec_type_str(it->second);
                        std::string result = fresh_reg();
                        emit_line("  " + result + " = load " + vec_type + ", ptr " + ptr);
                        last_expr_type_ = vec_type;
                        return result;
                    }
                }
            }
        }
        return "0";
    }

    // simd_store[V](ptr: mut ref V, val: V)
    // Stores a raw LLVM vector value back to a @simd struct.
    if (intrinsic_name == "simd_store") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            std::string val_type = last_expr_type_;

            // If val_type is already a vector type string, use it directly
            if (val_type.starts_with("<")) {
                emit_line("  store " + val_type + " " + val + ", ptr " + ptr);
            } else {
                // Resolve from generics
                auto [name, info] = resolve_simd_from_generics();
                if (info) {
                    std::string vec_type = simd_vec_type_str(*info);
                    emit_line("  store " + vec_type + " " + val + ", ptr " + ptr);
                }
            }
            last_expr_type_ = "void";
            return "0";
        }
        return "0";
    }

    // simd_extract[V, T](vec: V, idx: I32) -> T
    // Extracts a single element from a SIMD vector by lane index.
    if (intrinsic_name == "simd_extract") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string vec_type = last_expr_type_; // Should be "<N x T>" from simd_load
            std::string idx = gen_expr(*call.args[1]);

            // Parse element type from vector type string "<N x T>" -> "T"
            std::string elem_type = "i32"; // Default
            if (vec_type.starts_with("<")) {
                auto x_pos = vec_type.find(" x ");
                if (x_pos != std::string::npos) {
                    elem_type = vec_type.substr(x_pos + 3);
                    if (elem_type.back() == '>') {
                        elem_type.pop_back();
                    }
                }
            }

            std::string result = fresh_reg();
            emit_line("  " + result + " = extractelement " + vec_type + " " + vec + ", i32 " + idx);
            last_expr_type_ = elem_type;
            return result;
        }
        return "0";
    }

    // simd_insert[V, T](vec: V, elem: T, idx: I32) -> V
    // Inserts a single element into a SIMD vector at lane index.
    if (intrinsic_name == "simd_insert") {
        if (call.args.size() >= 3) {
            std::string vec = gen_expr(*call.args[0]);
            std::string vec_type = last_expr_type_; // "<N x T>"
            std::string elem = gen_expr(*call.args[1]);
            std::string elem_type = last_expr_type_;
            std::string idx = gen_expr(*call.args[2]);

            std::string result = fresh_reg();
            emit_line("  " + result + " = insertelement " + vec_type + " " + vec + ", " +
                      elem_type + " " + elem + ", i32 " + idx);
            last_expr_type_ = vec_type;
            return result;
        }
        return "0";
    }

    // simd_splat[V, T](val: T) -> V
    // Broadcasts a scalar value to all lanes of a SIMD vector.
    if (intrinsic_name == "simd_splat") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string val_type = last_expr_type_;

            auto [name, info] = resolve_simd_from_generics();
            if (info) {
                std::string vec_type = simd_vec_type_str(*info);
                int n = info->lane_count;
                // Efficient broadcast: insertelement at 0 + shufflevector zeroinitializer
                // Generates 2 instructions instead of N insertelements.
                // LLVM lowers this to a single broadcast instruction (e.g., VPBROADCASTB on x86).
                std::string insert_reg = fresh_reg();
                emit_line("  " + insert_reg + " = insertelement " + vec_type + " poison, " +
                          val_type + " " + val + ", i64 0");
                // Build zeroinitializer mask: <0, 0, 0, ...> (all lanes read from index 0)
                std::string mask = "<";
                for (int i = 0; i < n; ++i) {
                    if (i > 0)
                        mask += ", ";
                    mask += "i32 0";
                }
                mask += ">";
                std::string result = fresh_reg();
                emit_line("  " + result + " = shufflevector " + vec_type + " " + insert_reg + ", " +
                          vec_type + " poison, <" + std::to_string(n) + " x i32> " + mask);
                last_expr_type_ = vec_type;
                return result;
            }
        }
        return "0";
    }

    // simd_load_ptr[V](ptr: *Unit) -> V
    // Loads a SIMD vector from a raw pointer (unaligned, align 1).
    if (intrinsic_name == "simd_load_ptr") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);

            auto [name, info] = resolve_simd_from_generics();
            if (info) {
                std::string vec_type = simd_vec_type_str(*info);
                std::string result = fresh_reg();
                emit_line("  " + result + " = load " + vec_type + ", ptr " + ptr + ", align 1");
                last_expr_type_ = vec_type;
                return result;
            }
        }
        return "0";
    }

    // simd_bitmask(mask: <N x i1>) -> I32
    // Converts a vector boolean mask to an integer bitmask.
    // Bit i of the result is 1 if lane i of the mask is true.
    if (intrinsic_name == "simd_bitmask") {
        if (!call.args.empty()) {
            std::string mask = gen_expr(*call.args[0]);
            std::string mask_type = last_expr_type_; // e.g. "<16 x i1>"

            // Extract N from "<N x i1>"
            if (mask_type.starts_with("<")) {
                auto space_pos = mask_type.find(' ');
                std::string n_str = mask_type.substr(1, space_pos - 1);
                int n = std::stoi(n_str);

                std::string int_type = "i" + std::to_string(n);
                std::string cast_reg = fresh_reg();
                emit_line("  " + cast_reg + " = bitcast " + mask_type + " " + mask + " to " +
                          int_type);

                std::string result = fresh_reg();
                if (n < 32) {
                    emit_line("  " + result + " = zext " + int_type + " " + cast_reg + " to i32");
                } else if (n > 32) {
                    emit_line("  " + result + " = trunc " + int_type + " " + cast_reg + " to i32");
                } else {
                    result = cast_reg;
                }
                last_expr_type_ = "i32";
                return result;
            }
        }
        return "0";
    }

    // ============================================================================
    // Native SSE2 Intrinsics (x86-64)
    // ============================================================================

    // sse2_cmpeq_epi8(a: <16 x i8>, b: <16 x i8>) -> <16 x i8>
    // Byte-wise equality comparison. Returns 0xFF for match, 0x00 for no match.
    // Compiles to PCMPEQB on x86.
    if (intrinsic_name == "sse2_cmpeq_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            // icmp eq produces <16 x i1>, sext to <16 x i8> gives 0xFF/0x00 per lane
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <16 x i1> " + cmp + " to <16 x i8>");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse2_movemask_epi8(v: <16 x i8>) -> I32
    // Extracts the most significant bit of each byte into a 16-bit integer mask.
    // Compiles to PMOVMSKB on x86.
    if (intrinsic_name == "sse2_movemask_epi8") {
        if (!call.args.empty()) {
            std::string vec = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse2.pmovmskb.128(<16 x i8> " + vec +
                      ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
