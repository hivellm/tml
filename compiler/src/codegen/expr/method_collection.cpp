//! # LLVM IR Generator - Collection Methods
//!
//! This file implements instance methods for collection types.
//!
//! ## List Methods
//!
//! `push`, `pop`, `get`, `set`, `len`, `capacity`, `clear`, `is_empty`
//!
//! ## HashMap Methods
//!
//! `insert`, `get`, `remove`, `contains`, `len`, `clear`
//!
//! ## Buffer Methods
//!
//! `get`, `set`, `len`, `fill`
//!
//! Methods delegate to runtime functions like `@list_push`, `@hashmap_get`.

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_collection_method(const parser::MethodCallExpr& call,
                                      const std::string& receiver,
                                      const std::string& receiver_type_name,
                                      types::TypePtr receiver_type) -> std::optional<std::string> {
    const std::string& method = call.method;

    // Only handle List, HashMap, Buffer
    if (receiver_type_name != "List" && receiver_type_name != "HashMap" &&
        receiver_type_name != "Buffer") {
        return std::nullopt;
    }

    // Extract handle from collection struct
    std::string struct_type = "%struct." + receiver_type_name;
    if (receiver_type && receiver_type->is<types::NamedType>()) {
        const auto& named = receiver_type->as<types::NamedType>();
        if (!named.type_args.empty()) {
            struct_type = "%struct." + named.name;
            for (const auto& arg : named.type_args) {
                struct_type += "__" + mangle_type(arg);
            }
        }
    }

    std::string tmp = fresh_reg();
    emit_line("  " + tmp + " = alloca " + struct_type);
    emit_line("  store " + struct_type + " " + receiver + ", ptr " + tmp);
    std::string handle_ptr = fresh_reg();
    emit_line("  " + handle_ptr + " = getelementptr " + struct_type + ", ptr " + tmp +
              ", i32 0, i32 0");
    std::string handle = fresh_reg();
    emit_line("  " + handle + " = load ptr, ptr " + handle_ptr);

    // List methods
    if (receiver_type_name == "List") {
        if (method == "push") {
            if (call.args.empty()) {
                report_error("push requires an argument", call.span);
                return "0";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @list_push(ptr " + handle + ", i32 " + val +
                      ")");
            return result;
        }
        if (method == "pop") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @list_pop(ptr " + handle + ")");
            return result;
        }
        if (method == "get") {
            if (call.args.empty()) {
                report_error("get requires an argument", call.span);
                return "0";
            }
            std::string arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            std::string arg_type = last_expr_type_;
            std::string arg_i64 = arg;
            if (arg_type == "i32") {
                arg_i64 = fresh_reg();
                emit_line("  " + arg_i64 + " = sext i32 " + arg + " to i64");
            }
            emit_line("  " + result + " = call i64 @list_get(ptr " + handle + ", i64 " + arg_i64 +
                      ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "set") {
            if (call.args.size() < 2) {
                report_error("set requires two arguments", call.span);
                return "void";
            }
            std::string idx = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            emit_line("  call void @list_set(ptr " + handle + ", i32 " + idx + ", i32 " + val +
                      ")");
            return "void";
        }
        if (method == "len" || method == "length") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @list_len(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "capacity") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @list_capacity(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "clear") {
            emit_line("  call void @list_clear(ptr " + handle + ")");
            return "void";
        }
        if (method == "is_empty" || method == "isEmpty") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @list_is_empty(ptr " + handle + ")");
            return result;
        }
        if (method == "destroy") {
            emit_line("  call void @list_destroy(ptr " + handle + ")");
            return "void";
        }
    }

    // HashMap methods
    if (receiver_type_name == "HashMap") {
        if (method == "get") {
            if (call.args.empty()) {
                report_error("get requires an argument", call.span);
                return "0";
            }
            std::string key = gen_expr(*call.args[0]);
            std::string key_i64 = fresh_reg();
            emit_line("  " + key_i64 + " = sext i32 " + key + " to i64");
            std::string result_i64 = fresh_reg();
            emit_line("  " + result_i64 + " = call i64 @hashmap_get(ptr " + handle + ", i64 " +
                      key_i64 + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + result_i64 + " to i32");
            return result;
        }
        if (method == "set") {
            if (call.args.size() < 2) {
                report_error("set requires two arguments", call.span);
                return "void";
            }
            std::string key = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            std::string key_i64 = fresh_reg();
            std::string val_i64 = fresh_reg();
            emit_line("  " + key_i64 + " = sext i32 " + key + " to i64");
            emit_line("  " + val_i64 + " = sext i32 " + val + " to i64");
            emit_line("  call void @hashmap_set(ptr " + handle + ", i64 " + key_i64 + ", i64 " +
                      val_i64 + ")");
            return "void";
        }
        if (method == "has" || method == "contains") {
            if (call.args.empty()) {
                report_error("has requires a key argument", call.span);
                return "0";
            }
            std::string key = gen_expr(*call.args[0]);
            std::string key_i64 = fresh_reg();
            emit_line("  " + key_i64 + " = sext i32 " + key + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @hashmap_has(ptr " + handle + ", i64 " + key_i64 +
                      ")");
            return result;
        }
        if (method == "remove") {
            if (call.args.empty()) {
                report_error("remove requires a key argument", call.span);
                return "0";
            }
            std::string key = gen_expr(*call.args[0]);
            std::string key_i64 = fresh_reg();
            emit_line("  " + key_i64 + " = sext i32 " + key + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @hashmap_remove(ptr " + handle + ", i64 " +
                      key_i64 + ")");
            return result;
        }
        if (method == "len" || method == "length") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @hashmap_len(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "clear") {
            emit_line("  call void @hashmap_clear(ptr " + handle + ")");
            return "void";
        }
        if (method == "destroy") {
            emit_line("  call void @hashmap_destroy(ptr " + handle + ")");
            return "void";
        }
    }

    // Buffer methods
    if (receiver_type_name == "Buffer") {
        if (method == "write_byte") {
            if (call.args.empty()) {
                report_error("write_byte requires a value argument", call.span);
                return "0";
            }
            std::string val = gen_expr(*call.args[0]);
            emit_line("  call void @buffer_write_byte(ptr " + handle + ", i32 " + val + ")");
            return "void";
        }
        if (method == "read_byte") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_byte(ptr " + handle + ")");
            return result;
        }
        if (method == "write_i32") {
            if (call.args.empty()) {
                report_error("write_i32 requires a value argument", call.span);
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            emit_line("  call void @buffer_write_i32(ptr " + handle + ", i32 " + val + ")");
            return "void";
        }
        if (method == "write_i64") {
            if (call.args.empty()) {
                report_error("write_i64 requires a value argument", call.span);
                return "void";
            }
            std::string val = gen_expr(*call.args[0]);
            std::string val_i64 = fresh_reg();
            emit_line("  " + val_i64 + " = sext i32 " + val + " to i64");
            emit_line("  call void @buffer_write_i64(ptr " + handle + ", i64 " + val_i64 + ")");
            return "void";
        }
        if (method == "read_i32") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_i32(ptr " + handle + ")");
            return result;
        }
        if (method == "read_i64") {
            std::string result_i64 = fresh_reg();
            emit_line("  " + result_i64 + " = call i64 @buffer_read_i64(ptr " + handle + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + result_i64 + " to i32");
            return result;
        }
        if (method == "len" || method == "length") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_len(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "capacity") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_capacity(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "remaining") {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @buffer_remaining(ptr " + handle + ")");
            last_expr_type_ = "i64";
            return result;
        }
        if (method == "clear") {
            emit_line("  call void @buffer_clear(ptr " + handle + ")");
            return "void";
        }
        if (method == "reset_read") {
            emit_line("  call void @buffer_reset_read(ptr " + handle + ")");
            return "void";
        }
        if (method == "destroy") {
            emit_line("  call void @buffer_destroy(ptr " + handle + ")");
            return "void";
        }
    }

    return std::nullopt;
}

} // namespace tml::codegen
