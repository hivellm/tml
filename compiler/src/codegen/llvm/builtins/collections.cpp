//! # LLVM IR Generator - Collection Builtins
//!
//! This file implements collection intrinsic function calls.
//!
//! ## Buffer Functions
//!
//! `buffer_create`, `buffer_destroy`, `buffer_get`,
//! `buffer_set`, `buffer_len`
//!
//! All functions delegate to runtime implementations.
//!
//! Note: List and HashMap functions have been removed — now pure TML
//! (see lib/std/src/collections/list.tml, hashmap.tml)

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_collections(const std::string& fn_name,
                                            const parser::CallExpr& call)
    -> std::optional<std::string> {

    // Note: List and HashMap functions removed — now pure TML

    // ============ BUFFER FUNCTIONS ============

    // buffer_create(capacity) -> buf_ptr
    if (fn_name == "buffer_create") {
        if (call.args.empty()) {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @buffer_create(i64 16)");
            last_expr_type_ = "ptr";
            return result;
        } else {
            std::string cap_expr = gen_expr(*call.args[0]);
            std::string cap_type = last_expr_type_;
            std::string cap;
            if (cap_type == "i64") {
                cap = cap_expr;
            } else {
                cap = fresh_reg();
                emit_line("  " + cap + " = sext " + cap_type + " " + cap_expr + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @buffer_create(i64 " + cap + ")");
            last_expr_type_ = "ptr";
            return result;
        }
    }

    // buffer_destroy(buf) -> Unit
    if (fn_name == "buffer_destroy") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            emit_line("  call void @buffer_destroy(ptr " + buf + ")");
        }
        return "0";
    }

    // buffer_write_byte(buf, byte) -> Unit
    if (fn_name == "buffer_write_byte") {
        if (call.args.size() >= 2) {
            std::string buf = gen_expr(*call.args[0]);
            std::string byte = gen_expr(*call.args[1]);
            emit_line("  call void @buffer_write_byte(ptr " + buf + ", i32 " + byte + ")");
        }
        return "0";
    }

    // buffer_write_i32(buf, value) -> Unit
    if (fn_name == "buffer_write_i32") {
        if (call.args.size() >= 2) {
            std::string buf = gen_expr(*call.args[0]);
            std::string value = gen_expr(*call.args[1]);
            emit_line("  call void @buffer_write_i32(ptr " + buf + ", i32 " + value + ")");
        }
        return "0";
    }

    // buffer_read_byte(buf) -> I32
    if (fn_name == "buffer_read_byte") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_byte(ptr " + buf + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // buffer_read_i32(buf) -> I32
    if (fn_name == "buffer_read_i32") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_i32(ptr " + buf + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // buffer_len(buf) -> I32
    if (fn_name == "buffer_len") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @buffer_len(ptr " + buf + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // buffer_capacity(buf) -> I32
    if (fn_name == "buffer_capacity") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @buffer_capacity(ptr " + buf + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // buffer_remaining(buf) -> I32
    if (fn_name == "buffer_remaining") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @buffer_remaining(ptr " + buf + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // buffer_clear(buf) -> Unit
    if (fn_name == "buffer_clear") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            emit_line("  call void @buffer_clear(ptr " + buf + ")");
        }
        return "0";
    }

    // buffer_reset_read(buf) -> Unit
    if (fn_name == "buffer_reset_read") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            emit_line("  call void @buffer_reset_read(ptr " + buf + ")");
        }
        return "0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
