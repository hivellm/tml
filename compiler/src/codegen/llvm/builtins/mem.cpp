TML_MODULE("codegen_x86")

//! # LLVM IR Generator - Memory Builtins
//!
//! This file implements low-level memory intrinsics.
//!
//! ## Allocation
//!
//! | Function   | LLVM Call                  |
//! |------------|----------------------------|
//! | `alloc`    | `@malloc`                  |
//! | `dealloc`  | `@free`                    |
//! | `mem_alloc`| `@malloc`                  |
//! | `mem_free` | `@free`                    |
//!
//! ## Memory Operations
//!
//! | Function      | LLVM Intrinsic           |
//! |---------------|--------------------------|
//! | `mem_copy`    | `@llvm.memcpy`           |
//! | `mem_move`    | `@llvm.memmove`          |
//! | `mem_set`     | `@llvm.memset`           |
//! | `mem_zero`    | `@llvm.memset` with 0    |
//! | `mem_compare` | `@memcmp`                |
//!
//! ## Pointer Arithmetic
//!
//! `ptr_offset`, `read_i32`, `write_i32` for raw memory access.

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_mem(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // Memory allocation: alloc(size) -> ptr
    // Always inline as malloc call (registered as builtin for type checking)
    if (fn_name == "alloc") {
        if (!call.args.empty()) {
            std::string size = gen_expr(*call.args[0]);
            std::string size_type = last_expr_type_;
            std::string result = fresh_reg();

            // If size is i32, it's the count variant - multiply by 4 (sizeof I32)
            if (size_type == "i32") {
                std::string byte_size = fresh_reg();
                std::string size_ext = fresh_reg();
                emit_line("  " + size_ext + " = sext i32 " + size + " to i64");
                emit_line("  " + byte_size + " = mul i64 " + size_ext + ", 4");
                emit_line("  " + result + " = call ptr @malloc(i64 " + byte_size + ")");
            } else {
                // Size is already i64
                emit_line("  " + result + " = call ptr @malloc(i64 " + size + ")");
            }
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // Memory deallocation: dealloc(ptr)
    // Always inline as free call (registered as builtin for type checking)
    if (fn_name == "dealloc") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            emit_line("  call void @free(ptr " + ptr + ")");
        }
        return "0";
    }

    // mem_alloc(size: I64) -> *Unit
    if (fn_name == "mem_alloc") {
        if (!call.args.empty()) {
            std::string size = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @mem_alloc(i64 " + size + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // mem_alloc_zeroed(size: I64) -> *Unit
    if (fn_name == "mem_alloc_zeroed") {
        if (!call.args.empty()) {
            std::string size = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @mem_alloc_zeroed(i64 " + size + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // mem_realloc(ptr: *Unit, new_size: I64) -> *Unit
    if (fn_name == "mem_realloc") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string size = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @mem_realloc(ptr " + ptr + ", i64 " + size +
                      ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // mem_free(ptr: *Unit) -> Unit
    if (fn_name == "mem_free") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            emit_line("  call void @mem_free(ptr " + ptr + ")");
        }
        return "";
    }

    // mem_copy(dest: *Unit, src: *Unit, size: I64) -> Unit
    if (fn_name == "mem_copy") {
        if (call.args.size() >= 3) {
            std::string dest = gen_expr(*call.args[0]);
            std::string src = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            emit_line("  call void @mem_copy(ptr " + dest + ", ptr " + src + ", i64 " + size + ")");
        }
        return "";
    }

    // mem_move(dest: *Unit, src: *Unit, size: I64) -> Unit
    if (fn_name == "mem_move") {
        if (call.args.size() >= 3) {
            std::string dest = gen_expr(*call.args[0]);
            std::string src = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            emit_line("  call void @mem_move(ptr " + dest + ", ptr " + src + ", i64 " + size + ")");
        }
        return "";
    }

    // mem_set(ptr: *Unit, value: I32, size: I64) -> Unit
    if (fn_name == "mem_set") {
        if (call.args.size() >= 3) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            emit_line("  call void @mem_set(ptr " + ptr + ", i32 " + val + ", i64 " + size + ")");
        }
        return "";
    }

    // mem_zero(ptr: *Unit, size: I64) -> Unit
    if (fn_name == "mem_zero") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string size = gen_expr(*call.args[1]);
            emit_line("  call void @mem_zero(ptr " + ptr + ", i64 " + size + ")");
        }
        return "";
    }

    // mem_compare(a: *Unit, b: *Unit, size: I64) -> I32
    if (fn_name == "mem_compare") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @mem_compare(ptr " + a + ", ptr " + b +
                      ", i64 " + size + ")");
            last_expr_type_ = "i32";
            return result;
        }
        last_expr_type_ = "i32";
        return "0";
    }

    // mem_eq(a: *Unit, b: *Unit, size: I64) -> Bool
    if (fn_name == "mem_eq") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @mem_eq(ptr " + a + ", ptr " + b + ", i64 " +
                      size + ")");
            // Convert i32 result to i1 (Bool) - mem_eq returns 1 if equal, 0 if not equal
            // So we compare with 0 to check if non-zero (true if equal)
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            last_expr_type_ = "i1";
            return bool_result;
        }
        last_expr_type_ = "i1";
        return "0";
    }

    // Read from memory: read_i32(ptr) -> I32
    if (fn_name == "read_i32") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load i32, ptr " + ptr);
            last_expr_type_ = "i32";
            return result;
        }
        last_expr_type_ = "i32";
        return "0";
    }

    // Write to memory: write_i32(ptr, value)
    if (fn_name == "write_i32") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            emit_line("  store i32 " + val + ", ptr " + ptr);
        }
        return "0";
    }

    // Pointer offset: ptr_offset(ptr, offset) -> ptr
    if (fn_name == "ptr_offset") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string offset = gen_expr(*call.args[1]);
            std::string offset_type = last_expr_type_;
            std::string result = fresh_reg();
            // Convert offset to i64 if needed
            std::string offset64 = offset;
            if (offset_type == "i32") {
                offset64 = fresh_reg();
                emit_line("  " + offset64 + " = sext i32 " + offset + " to i64");
            }
            emit_line("  " + result + " = getelementptr i32, ptr " + ptr + ", i64 " + offset64);
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // mem::forget(value) / mem_forget(value) - consume value without dropping
    // This is a no-op: just evaluate the argument (for side effects) and discard
    if (fn_name == "mem_forget" || fn_name == "mem::forget") {
        if (!call.args.empty()) {
            gen_expr(*call.args[0]); // Evaluate but discard
        }
        last_expr_type_ = "void";
        return "0";
    }

    // mem::zeroed[T]() / mem_zeroed() - return a zero-initialized value of type T
    if (fn_name == "mem_zeroed" || fn_name == "mem::zeroed") {
        // Determine the target type from context
        std::string zero_type = "i32"; // default

        // Use expected_literal_type_ or current_ret_type_ as type hint
        if (!expected_literal_type_.empty()) {
            zero_type = expected_literal_type_;
        } else if (!current_ret_type_.empty() && current_ret_type_ != "void") {
            zero_type = current_ret_type_;
        }

        // Return the appropriate zero value
        if (zero_type == "float") {
            last_expr_type_ = "float";
            return "0.0";
        } else if (zero_type == "double") {
            last_expr_type_ = "double";
            return "0.0";
        } else if (zero_type == "i1") {
            last_expr_type_ = "i1";
            return "false";
        } else if (zero_type == "ptr") {
            last_expr_type_ = "ptr";
            return "null";
        } else {
            // Integer types (i8, i16, i32, i64, i128)
            last_expr_type_ = zero_type;
            return "0";
        }
    }

    return std::nullopt;
}

} // namespace tml::codegen
