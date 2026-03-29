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

    // ============================================================================
    // SSE2 Comparison Intrinsics (2.1)
    // ============================================================================

    // sse2_cmpgt_epi8(a, b) -> <16 x i8>  — PCMPGTB (signed byte greater-than)
    if (intrinsic_name == "sse2_cmpgt_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <16 x i1> " + cmp + " to <16 x i8>");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse2_cmplt_epi8(a, b) -> <16 x i8>  — via PCMPGTB with swapped args
    if (intrinsic_name == "sse2_cmplt_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp slt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <16 x i1> " + cmp + " to <16 x i8>");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse2_cmpeq_epi16(a, b) -> <8 x i16>  — PCMPEQW (word equal)
    if (intrinsic_name == "sse2_cmpeq_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <8 x i1> " + cmp + " to <8 x i16>");
            last_expr_type_ = "<8 x i16>";
            return result;
        }
        return "0";
    }

    // sse2_cmpeq_epi32(a, b) -> <4 x i32>  — PCMPEQD (dword equal)
    if (intrinsic_name == "sse2_cmpeq_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <4 x i1> " + cmp + " to <4 x i32>");
            last_expr_type_ = "<4 x i32>";
            return result;
        }
        return "0";
    }

    // sse2_cmpgt_epi16(a, b) -> <8 x i16>  — PCMPGTW (signed word greater-than)
    if (intrinsic_name == "sse2_cmpgt_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <8 x i1> " + cmp + " to <8 x i16>");
            last_expr_type_ = "<8 x i16>";
            return result;
        }
        return "0";
    }

    // sse2_cmpgt_epi32(a, b) -> <4 x i32>  — PCMPGTD (signed dword greater-than)
    if (intrinsic_name == "sse2_cmpgt_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <4 x i1> " + cmp + " to <4 x i32>");
            last_expr_type_ = "<4 x i32>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Bitwise Intrinsics (2.2)
    // ============================================================================

    // sse2_and_si128(a, b) -> <2 x i64>  — PAND (128-bit AND)
    if (intrinsic_name == "sse2_and_si128") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = and " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // sse2_or_si128(a, b) -> <2 x i64>  — POR (128-bit OR)
    if (intrinsic_name == "sse2_or_si128") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = or " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // sse2_xor_si128(a, b) -> <2 x i64>  — PXOR (128-bit XOR)
    if (intrinsic_name == "sse2_xor_si128") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = xor " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // sse2_andnot_si128(a, b) -> <2 x i64>  — PANDN (~a & b)
    if (intrinsic_name == "sse2_andnot_si128") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            // PANDN = ~a & b  (NOT a, then AND with b)
            std::string not_a = fresh_reg();
            emit_line("  " + not_a + " = xor " + a_type + " " + a + ", <i64 -1, i64 -1>");
            std::string result = fresh_reg();
            emit_line("  " + result + " = and " + a_type + " " + not_a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Min/Max Intrinsics (2.3)
    // ============================================================================

    // sse2_min_epu8(a, b) -> <16 x i8>  — PMINUB (unsigned byte min)
    if (intrinsic_name == "sse2_min_epu8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp ult " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = select <16 x i1> " + cmp + ", " + a_type + " " + a +
                      ", " + a_type + " " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // sse2_max_epu8(a, b) -> <16 x i8>  — PMAXUB (unsigned byte max)
    if (intrinsic_name == "sse2_max_epu8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp ugt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = select <16 x i1> " + cmp + ", " + a_type + " " + a +
                      ", " + a_type + " " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // sse2_min_epi16(a, b) -> <8 x i16>  — PMINSW (signed word min)
    if (intrinsic_name == "sse2_min_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp slt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = select <8 x i1> " + cmp + ", " + a_type + " " + a + ", " +
                      a_type + " " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // sse2_max_epi16(a, b) -> <8 x i16>  — PMAXSW (signed word max)
    if (intrinsic_name == "sse2_max_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = select <8 x i1> " + cmp + ", " + a_type + " " + a + ", " +
                      a_type + " " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Movemask Intrinsics (2.4)
    // ============================================================================

    // sse2_movemask_ps(v: <4 x float>) -> I32  — MOVMSKPS
    if (intrinsic_name == "sse2_movemask_ps") {
        if (!call.args.empty()) {
            std::string vec = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse.movmsk.ps(<4 x float> " + vec +
                      ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // sse2_movemask_pd(v: <2 x double>) -> I32  — MOVMSKPD
    if (intrinsic_name == "sse2_movemask_pd") {
        if (!call.args.empty()) {
            std::string vec = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse2.movmsk.pd(<2 x double> " + vec +
                      ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Pack/Unpack Intrinsics (2.5)
    // ============================================================================

    // sse2_packs_epi16(a, b) -> <16 x i8>  — PACKSSWB (i16 -> i8 signed saturation)
    if (intrinsic_name == "sse2_packs_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i8> @llvm.x86.sse2.packsswb.128(<8 x i16> " +
                      a + ", <8 x i16> " + b + ")");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse2_packus_epi16(a, b) -> <16 x i8>  — PACKUSWB (i16 -> u8 unsigned saturation)
    if (intrinsic_name == "sse2_packus_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i8> @llvm.x86.sse2.packuswb.128(<8 x i16> " +
                      a + ", <8 x i16> " + b + ")");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse2_packs_epi32(a, b) -> <8 x i16>  — PACKSSDW (i32 -> i16 signed saturation)
    if (intrinsic_name == "sse2_packs_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <8 x i16> @llvm.x86.sse2.packssdw.128(<4 x i32> " +
                      a + ", <4 x i32> " + b + ")");
            last_expr_type_ = "<8 x i16>";
            return result;
        }
        return "0";
    }

    // sse2_unpacklo_epi8(a, b) -> <16 x i8>  — PUNPCKLBW (interleave low bytes)
    if (intrinsic_name == "sse2_unpacklo_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            // shufflevector with mask [0,16,1,17,2,18,3,19,4,20,5,21,6,22,7,23]
            std::string result = fresh_reg();
            emit_line("  " + result + " = shufflevector " + a_type + " " + a + ", " + a_type + " " +
                      b +
                      ", <16 x i32> <i32 0, i32 16, i32 1, i32 17, i32 2, i32 18, i32 3, i32 19, "
                      "i32 4, i32 20, i32 5, i32 21, i32 6, i32 22, i32 7, i32 23>");
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // sse2_unpackhi_epi8(a, b) -> <16 x i8>  — PUNPCKHBW (interleave high bytes)
    if (intrinsic_name == "sse2_unpackhi_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            // shufflevector with mask [8,24,9,25,10,26,11,27,12,28,13,29,14,30,15,31]
            std::string result = fresh_reg();
            emit_line("  " + result + " = shufflevector " + a_type + " " + a + ", " + a_type + " " +
                      b +
                      ", <16 x i32> <i32 8, i32 24, i32 9, i32 25, i32 10, i32 26, i32 11, i32 27, "
                      "i32 12, i32 28, i32 13, i32 29, i32 14, i32 30, i32 15, i32 31>");
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Shift Intrinsics (2.6)
    // ============================================================================

    // sse2_slli_epi16(v, imm) -> <8 x i16>  — Shift left immediate (16-bit lanes)
    if (intrinsic_name == "sse2_slli_epi16") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string vec_type = last_expr_type_;
            std::string imm = gen_expr(*call.args[1]);
            // Splat the shift amount to all lanes
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <8 x i16> undef, i16 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <8 x i16> " + splat +
                      ", <8 x i16> undef, <8 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl <8 x i16> " + vec + ", " + splat2);
            last_expr_type_ = "<8 x i16>";
            return result;
        }
        return "0";
    }

    // sse2_slli_epi32(v, imm) -> <4 x i32>  — Shift left immediate (32-bit lanes)
    if (intrinsic_name == "sse2_slli_epi32") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <4 x i32> undef, i32 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <4 x i32> " + splat +
                      ", <4 x i32> undef, <4 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl <4 x i32> " + vec + ", " + splat2);
            last_expr_type_ = "<4 x i32>";
            return result;
        }
        return "0";
    }

    // sse2_slli_epi64(v, imm) -> <2 x i64>  — Shift left immediate (64-bit lanes)
    if (intrinsic_name == "sse2_slli_epi64") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <2 x i64> undef, i64 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <2 x i64> " + splat +
                      ", <2 x i64> undef, <2 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl <2 x i64> " + vec + ", " + splat2);
            last_expr_type_ = "<2 x i64>";
            return result;
        }
        return "0";
    }

    // sse2_srli_epi16(v, imm) -> <8 x i16>  — Shift right logical immediate (16-bit)
    if (intrinsic_name == "sse2_srli_epi16") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <8 x i16> undef, i16 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <8 x i16> " + splat +
                      ", <8 x i16> undef, <8 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = lshr <8 x i16> " + vec + ", " + splat2);
            last_expr_type_ = "<8 x i16>";
            return result;
        }
        return "0";
    }

    // sse2_srli_epi32(v, imm) -> <4 x i32>  — Shift right logical immediate (32-bit)
    if (intrinsic_name == "sse2_srli_epi32") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <4 x i32> undef, i32 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <4 x i32> " + splat +
                      ", <4 x i32> undef, <4 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = lshr <4 x i32> " + vec + ", " + splat2);
            last_expr_type_ = "<4 x i32>";
            return result;
        }
        return "0";
    }

    // sse2_srli_epi64(v, imm) -> <2 x i64>  — Shift right logical immediate (64-bit)
    if (intrinsic_name == "sse2_srli_epi64") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <2 x i64> undef, i64 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <2 x i64> " + splat +
                      ", <2 x i64> undef, <2 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = lshr <2 x i64> " + vec + ", " + splat2);
            last_expr_type_ = "<2 x i64>";
            return result;
        }
        return "0";
    }

    // sse2_srai_epi16(v, imm) -> <8 x i16>  — Shift right arithmetic immediate (16-bit)
    if (intrinsic_name == "sse2_srai_epi16") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <8 x i16> undef, i16 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <8 x i16> " + splat +
                      ", <8 x i16> undef, <8 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = ashr <8 x i16> " + vec + ", " + splat2);
            last_expr_type_ = "<8 x i16>";
            return result;
        }
        return "0";
    }

    // sse2_srai_epi32(v, imm) -> <4 x i32>  — Shift right arithmetic immediate (32-bit)
    if (intrinsic_name == "sse2_srai_epi32") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string imm = gen_expr(*call.args[1]);
            std::string splat = fresh_reg();
            emit_line("  " + splat + " = insertelement <4 x i32> undef, i32 " + imm + ", i32 0");
            std::string splat2 = fresh_reg();
            emit_line("  " + splat2 + " = shufflevector <4 x i32> " + splat +
                      ", <4 x i32> undef, <4 x i32> zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result + " = ashr <4 x i32> " + vec + ", " + splat2);
            last_expr_type_ = "<4 x i32>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE2 Memory Intrinsics (2.7)
    // ============================================================================

    // sse2_storeu_si128(ptr, v)  — Unaligned 128-bit store
    if (intrinsic_name == "sse2_storeu_si128") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string vec = gen_expr(*call.args[1]);
            std::string vec_type = last_expr_type_;
            emit_line("  store " + vec_type + " " + vec + ", ptr " + ptr + ", align 1");
            last_expr_type_ = "void";
            return "";
        }
        return "0";
    }

    // sse2_store_si128(ptr, v)  — Aligned 128-bit store
    if (intrinsic_name == "sse2_store_si128") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string vec = gen_expr(*call.args[1]);
            std::string vec_type = last_expr_type_;
            emit_line("  store " + vec_type + " " + vec + ", ptr " + ptr + ", align 16");
            last_expr_type_ = "void";
            return "";
        }
        return "0";
    }

    // ============================================================================
    // SSE4.2 String Comparison Intrinsics (3.1)
    // ============================================================================

    // sse42_cmpistrm(a, b, imm) -> <16 x i8>  — PCMPISTRM
    // Implicit-length string compare, returns byte mask.
    // imm8 controls comparison mode (see Intel manual).
    if (intrinsic_name == "sse42_cmpistrm") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string imm = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i8> @llvm.x86.sse42.pcmpistrm128(<16 x i8> " +
                      a + ", <16 x i8> " + b + ", i8 " + imm + ")");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse42_cmpistri(a, b, imm) -> I32  — PCMPISTRI
    // Implicit-length string compare, returns index of first match/mismatch.
    if (intrinsic_name == "sse42_cmpistri") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string imm = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse42.pcmpistri128(<16 x i8> " + a +
                      ", <16 x i8> " + b + ", i8 " + imm + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // sse42_cmpestrm(a, la, b, lb, imm) -> <16 x i8>  — PCMPESTRM
    // Explicit-length string compare, returns byte mask.
    if (intrinsic_name == "sse42_cmpestrm") {
        if (call.args.size() >= 5) {
            std::string a = gen_expr(*call.args[0]);
            std::string la = gen_expr(*call.args[1]);
            std::string b = gen_expr(*call.args[2]);
            std::string lb = gen_expr(*call.args[3]);
            std::string imm = gen_expr(*call.args[4]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i8> @llvm.x86.sse42.pcmpestrm128(<16 x i8> " +
                      a + ", i32 " + la + ", <16 x i8> " + b + ", i32 " + lb + ", i8 " + imm + ")");
            last_expr_type_ = "<16 x i8>";
            return result;
        }
        return "0";
    }

    // sse42_cmpestri(a, la, b, lb, imm) -> I32  — PCMPESTRI
    // Explicit-length string compare, returns index.
    if (intrinsic_name == "sse42_cmpestri") {
        if (call.args.size() >= 5) {
            std::string a = gen_expr(*call.args[0]);
            std::string la = gen_expr(*call.args[1]);
            std::string b = gen_expr(*call.args[2]);
            std::string lb = gen_expr(*call.args[3]);
            std::string imm = gen_expr(*call.args[4]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse42.pcmpestri128(<16 x i8> " + a +
                      ", i32 " + la + ", <16 x i8> " + b + ", i32 " + lb + ", i8 " + imm + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // SSE4.2 CRC32 Intrinsics (3.2)
    // ============================================================================

    // sse42_crc32_u8(crc: U32, data: U8) -> U32
    if (intrinsic_name == "sse42_crc32_u8") {
        if (call.args.size() >= 2) {
            std::string crc = gen_expr(*call.args[0]);
            std::string data = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse42.crc32.32.8(i32 " + crc +
                      ", i8 " + data + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // sse42_crc32_u16(crc: U32, data: U16) -> U32
    if (intrinsic_name == "sse42_crc32_u16") {
        if (call.args.size() >= 2) {
            std::string crc = gen_expr(*call.args[0]);
            std::string data = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse42.crc32.32.16(i32 " + crc +
                      ", i16 " + data + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // sse42_crc32_u32(crc: U32, data: U32) -> U32
    if (intrinsic_name == "sse42_crc32_u32") {
        if (call.args.size() >= 2) {
            std::string crc = gen_expr(*call.args[0]);
            std::string data = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.sse42.crc32.32.32(i32 " + crc +
                      ", i32 " + data + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // sse42_crc32_u64(crc: U64, data: U64) -> U64
    if (intrinsic_name == "sse42_crc32_u64") {
        if (call.args.size() >= 2) {
            std::string crc = gen_expr(*call.args[0]);
            std::string data = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @llvm.x86.sse42.crc32.64.64(i64 " + crc +
                      ", i64 " + data + ")");
            last_expr_type_ = "i64";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // POPCNT Intrinsics (3.3)
    // ============================================================================

    // popcnt_u32(val: U32) -> U32  — count set bits
    if (intrinsic_name == "popcnt_u32") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.ctpop.i32(i32 " + val + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // popcnt_u64(val: U64) -> U64  — count set bits
    if (intrinsic_name == "popcnt_u64") {
        if (!call.args.empty()) {
            std::string val = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @llvm.ctpop.i64(i64 " + val + ")");
            last_expr_type_ = "i64";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // AVX2 Comparison Intrinsics (4.3)
    // ============================================================================

    // avx2_cmpeq_epi8(a, b) -> <32 x i8>  — VPCMPEQB
    if (intrinsic_name == "avx2_cmpeq_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <32 x i1> " + cmp + " to <32 x i8>");
            last_expr_type_ = "<32 x i8>";
            return result;
        }
        return "0";
    }

    // avx2_cmpeq_epi16(a, b) -> <16 x i16>  — VPCMPEQW
    if (intrinsic_name == "avx2_cmpeq_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <16 x i1> " + cmp + " to <16 x i16>");
            last_expr_type_ = "<16 x i16>";
            return result;
        }
        return "0";
    }

    // avx2_cmpeq_epi32(a, b) -> <8 x i32>  — VPCMPEQD
    if (intrinsic_name == "avx2_cmpeq_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp eq " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <8 x i1> " + cmp + " to <8 x i32>");
            last_expr_type_ = "<8 x i32>";
            return result;
        }
        return "0";
    }

    // avx2_cmpgt_epi8(a, b) -> <32 x i8>  — VPCMPGTB
    if (intrinsic_name == "avx2_cmpgt_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <32 x i1> " + cmp + " to <32 x i8>");
            last_expr_type_ = "<32 x i8>";
            return result;
        }
        return "0";
    }

    // avx2_cmpgt_epi16(a, b) -> <16 x i16>  — VPCMPGTW
    if (intrinsic_name == "avx2_cmpgt_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <16 x i1> " + cmp + " to <16 x i16>");
            last_expr_type_ = "<16 x i16>";
            return result;
        }
        return "0";
    }

    // avx2_cmpgt_epi32(a, b) -> <8 x i32>  — VPCMPGTD
    if (intrinsic_name == "avx2_cmpgt_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string cmp = fresh_reg();
            emit_line("  " + cmp + " = icmp sgt " + a_type + " " + a + ", " + b);
            std::string result = fresh_reg();
            emit_line("  " + result + " = sext <8 x i1> " + cmp + " to <8 x i32>");
            last_expr_type_ = "<8 x i32>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // AVX2 Bitwise & Movemask Intrinsics (4.4)
    // ============================================================================

    // avx2_and_si256(a, b)  — VPAND
    if (intrinsic_name == "avx2_and_si256") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = and " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // avx2_or_si256(a, b)  — VPOR
    if (intrinsic_name == "avx2_or_si256") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = or " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // avx2_xor_si256(a, b)  — VPXOR
    if (intrinsic_name == "avx2_xor_si256") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string a_type = last_expr_type_;
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = xor " + a_type + " " + a + ", " + b);
            last_expr_type_ = a_type;
            return result;
        }
        return "0";
    }

    // avx2_movemask_epi8(v: <32 x i8>) -> I32  — VPMOVMSKB
    if (intrinsic_name == "avx2_movemask_epi8") {
        if (!call.args.empty()) {
            std::string vec = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @llvm.x86.avx2.pmovmskb(<32 x i8> " + vec + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // AVX2 Shuffle & Permute Intrinsics (4.5)
    // ============================================================================

    // avx2_shuffle_epi8(a, b) -> <32 x i8>  — VPSHUFB (in-lane byte shuffle)
    if (intrinsic_name == "avx2_shuffle_epi8") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <32 x i8> @llvm.x86.avx2.pshuf.b(<32 x i8> " + a +
                      ", <32 x i8> " + b + ")");
            last_expr_type_ = "<32 x i8>";
            return result;
        }
        return "0";
    }

    // avx2_permute4x64_epi64(v, idx) -> <4 x i64>  — VPERMQ via VPERMD
    // idx is a <4 x i64> vector of lane indices (each 0-3)
    // This is the variable-index form; LLVM lowers to VPERMQ when possible.
    if (intrinsic_name == "avx2_permute4x64_epi64") {
        if (call.args.size() >= 2) {
            std::string vec = gen_expr(*call.args[0]);
            std::string idx = gen_expr(*call.args[1]);
            // Use VPERMD on the i32 view: bitcast <4 x i64> to <8 x i32>,
            // expand index vector to <8 x i32>, then bitcast back.
            // For simplicity, use @llvm.x86.avx2.permd which does VPERMD.
            std::string bc_vec = fresh_reg();
            emit_line("  " + bc_vec + " = bitcast <4 x i64> " + vec + " to <8 x i32>");
            std::string bc_idx = fresh_reg();
            emit_line("  " + bc_idx + " = bitcast <4 x i64> " + idx + " to <8 x i32>");
            std::string permd = fresh_reg();
            emit_line("  " + permd + " = call <8 x i32> @llvm.x86.avx2.permd(<8 x i32> " + bc_vec +
                      ", <8 x i32> " + bc_idx + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = bitcast <8 x i32> " + permd + " to <4 x i64>");
            last_expr_type_ = "<4 x i64>";
            return result;
        }
        return "0";
    }

    // avx2_permute2x128_si256(a, b, imm) -> <4 x i64>  — VPERM2I128
    // imm8 selects which 128-bit halves to place in the result.
    // LLVM doesn't have a direct intrinsic; use shufflevector patterns.
    // imm8 = 0x20: [a_lo, b_lo], 0x31: [a_hi, b_hi], 0x03: [b_lo, a_hi], etc.
    if (intrinsic_name == "avx2_permute2x128_si256") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string imm = gen_expr(*call.args[2]);
            // For the common case imm=0x20 (interleave low halves):
            // shufflevector <4 x i64> a, <4 x i64> b, <i32 0, i32 1, i32 4, i32 5>
            // Since we need runtime imm support, emit a fallback using
            // extractelement/insertelement. For now, just pass through as identity (will be
            // extended later). Use inline asm for exact VPERM2I128 behavior:
            std::string result = fresh_reg();
            // Emit as a generic bitwise operation placeholder that LLVM can optimize
            // This is a simplified version — for now return the first operand
            emit_line("  " + result + " = shufflevector <4 x i64> " + a + ", <4 x i64> " + b +
                      ", <4 x i32> <i32 0, i32 1, i32 4, i32 5>");
            last_expr_type_ = "<4 x i64>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // AVX2 Horizontal & Pack Intrinsics (4.6)
    // ============================================================================

    // avx2_hadd_epi16(a, b) -> <16 x i16>  — VPHADDW (horizontal add pairs)
    if (intrinsic_name == "avx2_hadd_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i16> @llvm.x86.avx2.phadd.w(<16 x i16> " + a +
                      ", <16 x i16> " + b + ")");
            last_expr_type_ = "<16 x i16>";
            return result;
        }
        return "0";
    }

    // avx2_hadd_epi32(a, b) -> <8 x i32>  — VPHADDD (horizontal add pairs)
    if (intrinsic_name == "avx2_hadd_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <8 x i32> @llvm.x86.avx2.phadd.d(<8 x i32> " + a +
                      ", <8 x i32> " + b + ")");
            last_expr_type_ = "<8 x i32>";
            return result;
        }
        return "0";
    }

    // avx2_packs_epi16(a, b) -> <32 x i8>  — VPACKSSWB (i16->i8 signed saturation)
    if (intrinsic_name == "avx2_packs_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <32 x i8> @llvm.x86.avx2.packsswb(<16 x i16> " + a +
                      ", <16 x i16> " + b + ")");
            last_expr_type_ = "<32 x i8>";
            return result;
        }
        return "0";
    }

    // avx2_packs_epi32(a, b) -> <16 x i16>  — VPACKSSDW (i32->i16 signed saturation)
    if (intrinsic_name == "avx2_packs_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i16> @llvm.x86.avx2.packssdw(<8 x i32> " + a +
                      ", <8 x i32> " + b + ")");
            last_expr_type_ = "<16 x i16>";
            return result;
        }
        return "0";
    }

    // avx2_packus_epi16(a, b) -> <32 x i8>  — VPACKUSWB (i16->u8 unsigned saturation)
    if (intrinsic_name == "avx2_packus_epi16") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <32 x i8> @llvm.x86.avx2.packuswb(<16 x i16> " + a +
                      ", <16 x i16> " + b + ")");
            last_expr_type_ = "<32 x i8>";
            return result;
        }
        return "0";
    }

    // avx2_packus_epi32(a, b) -> <16 x i16>  — VPACKUSDW (i32->u16 unsigned saturation)
    if (intrinsic_name == "avx2_packus_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <16 x i16> @llvm.x86.avx2.packusdw(<8 x i32> " + a +
                      ", <8 x i32> " + b + ")");
            last_expr_type_ = "<16 x i16>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // AVX2 Gather Intrinsics (4.7)
    // ============================================================================

    // avx2_gather_epi32(base_ptr, indices, mask, scale) -> <8 x i32>  — VPGATHERDD
    // Uses llvm.masked.gather (target-independent; LLVM lowers to VPGATHERDD with +avx2)
    if (intrinsic_name == "avx2_gather_epi32") {
        if (call.args.size() >= 4) {
            std::string base = gen_expr(*call.args[0]);     // ptr
            std::string indices = gen_expr(*call.args[1]);  // <8 x i32>
            std::string mask_vec = gen_expr(*call.args[2]); // <8 x i32> (-1 = active)
            // scale arg consumed by GEP element size (i32 = 4 bytes)
            // Build vector of pointers: GEP base by each index
            std::string ptrs = fresh_reg();
            emit_line("  " + ptrs + " = getelementptr i32, ptr " + base + ", <8 x i32> " + indices);
            // Convert <8 x i32> mask to <8 x i1>
            std::string mask_cmp = fresh_reg();
            emit_line("  " + mask_cmp + " = icmp ne <8 x i32> " + mask_vec + ", zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result +
                      " = call <8 x i32> @llvm.masked.gather.v8i32.v8p0("
                      "<8 x ptr> " +
                      ptrs + ", i32 4, <8 x i1> " + mask_cmp + ", <8 x i32> zeroinitializer)");
            last_expr_type_ = "<8 x i32>";
            return result;
        }
        return "0";
    }

    // avx2_gather_epi64(base_ptr, indices, mask, scale) -> <4 x i64>  — VPGATHERDQ
    if (intrinsic_name == "avx2_gather_epi64") {
        if (call.args.size() >= 4) {
            std::string base = gen_expr(*call.args[0]);     // ptr
            std::string indices = gen_expr(*call.args[1]);  // <4 x i32>
            std::string mask_vec = gen_expr(*call.args[2]); // <4 x i64> (-1 = active)
            // sext <4 x i32> indices to <4 x i64> for GEP
            std::string idx64 = fresh_reg();
            emit_line("  " + idx64 + " = sext <4 x i32> " + indices + " to <4 x i64>");
            std::string ptrs = fresh_reg();
            emit_line("  " + ptrs + " = getelementptr i64, ptr " + base + ", <4 x i64> " + idx64);
            std::string mask_cmp = fresh_reg();
            emit_line("  " + mask_cmp + " = icmp ne <4 x i64> " + mask_vec + ", zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result +
                      " = call <4 x i64> @llvm.masked.gather.v4i64.v4p0("
                      "<4 x ptr> " +
                      ptrs + ", i32 8, <4 x i1> " + mask_cmp + ", <4 x i64> zeroinitializer)");
            last_expr_type_ = "<4 x i64>";
            return result;
        }
        return "0";
    }

    // avx2_gather_ps(base_ptr, indices, mask, scale) -> <8 x float>  — VGATHERDPS
    if (intrinsic_name == "avx2_gather_ps") {
        if (call.args.size() >= 4) {
            std::string base = gen_expr(*call.args[0]);     // ptr
            std::string indices = gen_expr(*call.args[1]);  // <8 x i32>
            std::string mask_vec = gen_expr(*call.args[2]); // <8 x i32> (as int mask)
            std::string ptrs = fresh_reg();
            emit_line("  " + ptrs + " = getelementptr float, ptr " + base + ", <8 x i32> " +
                      indices);
            std::string mask_cmp = fresh_reg();
            emit_line("  " + mask_cmp + " = icmp ne <8 x i32> " + mask_vec + ", zeroinitializer");
            std::string result = fresh_reg();
            emit_line("  " + result +
                      " = call <8 x float> @llvm.masked.gather.v8f32.v8p0("
                      "<8 x ptr> " +
                      ptrs + ", i32 4, <8 x i1> " + mask_cmp + ", <8 x float> zeroinitializer)");
            last_expr_type_ = "<8 x float>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // AVX2 Variable Shift Intrinsics (4.8)
    // ============================================================================

    // avx2_sllv_epi32(a, shift) -> <8 x i32>  — VPSLLVD (per-lane shift left)
    if (intrinsic_name == "avx2_sllv_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl <8 x i32> " + a + ", " + b);
            last_expr_type_ = "<8 x i32>";
            return result;
        }
        return "0";
    }

    // avx2_sllv_epi64(a, shift) -> <4 x i64>  — VPSLLVQ (per-lane shift left)
    if (intrinsic_name == "avx2_sllv_epi64") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = shl <4 x i64> " + a + ", " + b);
            last_expr_type_ = "<4 x i64>";
            return result;
        }
        return "0";
    }

    // avx2_srlv_epi32(a, shift) -> <8 x i32>  — VPSRLVD (per-lane shift right)
    if (intrinsic_name == "avx2_srlv_epi32") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = lshr <8 x i32> " + a + ", " + b);
            last_expr_type_ = "<8 x i32>";
            return result;
        }
        return "0";
    }

    // avx2_srlv_epi64(a, shift) -> <4 x i64>  — VPSRLVQ (per-lane shift right)
    if (intrinsic_name == "avx2_srlv_epi64") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = lshr <4 x i64> " + a + ", " + b);
            last_expr_type_ = "<4 x i64>";
            return result;
        }
        return "0";
    }

    // ============================================================================
    // FMA Intrinsics (4.9) — Fused Multiply-Add
    // ============================================================================

    // fma_fmadd_ps(a, b, c) -> <8 x float>  — VFMADD (a*b + c)
    if (intrinsic_name == "fma_fmadd_ps") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <8 x float> @llvm.fma.v8f32(<8 x float> " + a +
                      ", <8 x float> " + b + ", <8 x float> " + c + ")");
            last_expr_type_ = "<8 x float>";
            return result;
        }
        return "0";
    }

    // fma_fmadd_pd(a, b, c) -> <4 x double>  — VFMADD (a*b + c)
    if (intrinsic_name == "fma_fmadd_pd") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <4 x double> @llvm.fma.v4f64(<4 x double> " + a +
                      ", <4 x double> " + b + ", <4 x double> " + c + ")");
            last_expr_type_ = "<4 x double>";
            return result;
        }
        return "0";
    }

    // fma_fmsub_ps(a, b, c) -> <8 x float>  — VFMSUB (a*b - c = fma(a, b, -c))
    if (intrinsic_name == "fma_fmsub_ps") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string neg_c = fresh_reg();
            emit_line("  " + neg_c + " = fneg <8 x float> " + c);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <8 x float> @llvm.fma.v8f32(<8 x float> " + a +
                      ", <8 x float> " + b + ", <8 x float> " + neg_c + ")");
            last_expr_type_ = "<8 x float>";
            return result;
        }
        return "0";
    }

    // fma_fmsub_pd(a, b, c) -> <4 x double>  — VFMSUB (a*b - c)
    if (intrinsic_name == "fma_fmsub_pd") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string neg_c = fresh_reg();
            emit_line("  " + neg_c + " = fneg <4 x double> " + c);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <4 x double> @llvm.fma.v4f64(<4 x double> " + a +
                      ", <4 x double> " + b + ", <4 x double> " + neg_c + ")");
            last_expr_type_ = "<4 x double>";
            return result;
        }
        return "0";
    }

    // fma_fnmadd_ps(a, b, c) -> <8 x float>  — VFNMADD (-a*b + c = fma(-a, b, c))
    if (intrinsic_name == "fma_fnmadd_ps") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string neg_a = fresh_reg();
            emit_line("  " + neg_a + " = fneg <8 x float> " + a);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <8 x float> @llvm.fma.v8f32(<8 x float> " + neg_a +
                      ", <8 x float> " + b + ", <8 x float> " + c + ")");
            last_expr_type_ = "<8 x float>";
            return result;
        }
        return "0";
    }

    // fma_fnmadd_pd(a, b, c) -> <4 x double>  — VFNMADD (-a*b + c)
    if (intrinsic_name == "fma_fnmadd_pd") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string neg_a = fresh_reg();
            emit_line("  " + neg_a + " = fneg <4 x double> " + a);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call <4 x double> @llvm.fma.v4f64(<4 x double> " + neg_a +
                      ", <4 x double> " + b + ", <4 x double> " + c + ")");
            last_expr_type_ = "<4 x double>";
            return result;
        }
        return "0";
    }

    // fma_fmadd_ss(a, b, c) -> F32  — Scalar FMA (a*b + c)
    if (intrinsic_name == "fma_fmadd_ss") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call float @llvm.fma.f32(float " + a + ", float " + b +
                      ", float " + c + ")");
            last_expr_type_ = "float";
            return result;
        }
        return "0";
    }

    // fma_fmadd_sd(a, b, c) -> F64  — Scalar FMA (a*b + c)
    if (intrinsic_name == "fma_fmadd_sd") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string c = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @llvm.fma.f64(double " + a + ", double " + b +
                      ", double " + c + ")");
            last_expr_type_ = "double";
            return result;
        }
        return "0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
