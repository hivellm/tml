TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Slice, Array, Type Info, and Unsafe Conversion Intrinsics
//!
//! This file implements the non-SIMD half of the compiler intrinsic dispatch
//! table. Split from intrinsics.cpp for maintainability.
//!
//! ## Sections in this file
//!
//! - Slice Intrinsics (slice_get, slice_get_mut, slice_set, slice_offset, slice_swap)
//! - Array Intrinsics (array_as_ptr, array_as_mut_ptr, array_offset_ptr, array_take, etc.)
//! - Type Information Intrinsics (size_of, align_of, sizeof_type, type_id)
//! - Unsafe Conversions (transmute, cast)
//!
//! SIMD, SSE, and AVX intrinsics are dispatched to helper methods:
//! - try_gen_simd_vector_intrinsic()  — intrinsics_simd_vector.cpp
//! - try_gen_simd_sse_intrinsic()     — intrinsics_simd_sse.cpp
//! - try_gen_simd_avx_intrinsic()     — intrinsics_simd_avx.cpp

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

/// Handles Slice, Array, Type Information, Unsafe Conversion intrinsics, and
/// dispatches to SIMD helper methods. Called from try_gen_intrinsic() after the
/// Arithmetic, Comparison, Bitwise, and Memory sections have been checked.
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

    // list_get_mut(handle: *Unit, idx: I64) -> mut ref T
    //
    // Zero-copy mutable element access for `List[T]` (the IndexMut surface).
    // `handle` points at the 32-byte List header:
    //   {data ptr @0, len @8, cap @16, stride @24}
    // Element address = load(data) + idx * load(stride). Byte-addressed GEP on
    // i8 keeps this element-type-agnostic (mirrors List.get in list.tml). Result
    // is a `ptr` (mut ref). No bounds check here — List.index_mut is called from
    // TML which bounds-checks before the lowlevel block, exactly like slice_get_mut
    // stays raw and defers bounds handling to the caller.
    if (intrinsic_name == "list_get_mut") {
        if (call.args.size() >= 2) {
            std::string handle = gen_expr(*call.args[0]);
            std::string handle_type = last_expr_type_;
            // The header pointer may arrive as an i64 address — normalize to ptr.
            if (handle_type == "i64") {
                std::string conv = fresh_reg();
                emit_line("  " + conv + " = inttoptr i64 " + handle + " to ptr");
                handle = conv;
            }

            std::string index = gen_expr(*call.args[1]);
            std::string index_type = last_expr_type_;
            if (index_type == "i32") {
                std::string ext = fresh_reg();
                emit_line("  " + ext + " = sext i32 " + index + " to i64");
                index = ext;
            }

            // data = load ptr, ptr %handle   (field @0)
            std::string data = fresh_reg();
            emit_line("  " + data + " = load ptr, ptr " + handle);

            // stride = load i64, ptr (handle + 24)  (field @24)
            std::string stride_ptr = fresh_reg();
            emit_line("  " + stride_ptr + " = getelementptr inbounds i8, ptr " + handle +
                      ", i64 24");
            std::string stride = fresh_reg();
            emit_line("  " + stride + " = load i64, ptr " + stride_ptr);

            // byte_offset = idx * stride
            std::string byte_offset = fresh_reg();
            emit_line("  " + byte_offset + " = mul i64 " + index + ", " + stride);

            // elem_ptr = data + byte_offset   (byte-addressed)
            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr inbounds i8, ptr " + data + ", i64 " +
                      byte_offset);
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
    // SIMD, SSE, and AVX dispatch — delegated to helper methods
    // ============================================================================

    if (auto r = try_gen_simd_vector_intrinsic(intrinsic_name, fn_name, call))
        return r;
    if (auto r = try_gen_simd_sse_intrinsic(intrinsic_name, fn_name, call))
        return r;
    if (auto r = try_gen_simd_avx_intrinsic(intrinsic_name, fn_name, call))
        return r;

    return std::nullopt;
}

} // namespace tml::codegen
