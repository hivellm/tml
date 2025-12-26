// LLVM IR generator - Collections builtin functions
// Handles: list_*, hashmap_*, buffer_*

#include "tml/codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_collections(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // ============ LIST FUNCTIONS ============

    // list_create(capacity) -> list_ptr
    if (fn_name == "list_create") {
        if (call.args.empty()) {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @list_create(i64 4)");
            return result;
        } else {
            std::string i32_cap = gen_expr(*call.args[0]);
            std::string cap = fresh_reg();
            emit_line("  " + cap + " = sext i32 " + i32_cap + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @list_create(i64 " + cap + ")");
            return result;
        }
    }

    // list_destroy(list) -> Unit
    if (fn_name == "list_destroy") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            emit_line("  call void @list_destroy(ptr " + list + ")");
        }
        return "0";
    }

    // list_push(list, value) -> Unit
    if (fn_name == "list_push") {
        if (call.args.size() >= 2) {
            std::string list = gen_expr(*call.args[0]);
            std::string i32_value = gen_expr(*call.args[1]);
            std::string value = fresh_reg();
            emit_line("  " + value + " = sext i32 " + i32_value + " to i64");
            emit_line("  call void @list_push(ptr " + list + ", i64 " + value + ")");
        }
        return "0";
    }

    // list_pop(list) -> I32
    if (fn_name == "list_pop") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @list_pop(ptr " + list + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // list_get(list, index) -> I32
    if (fn_name == "list_get") {
        if (call.args.size() >= 2) {
            std::string list = gen_expr(*call.args[0]);
            std::string i32_index = gen_expr(*call.args[1]);
            std::string index = fresh_reg();
            emit_line("  " + index + " = sext i32 " + i32_index + " to i64");
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @list_get(ptr " + list + ", i64 " + index + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // list_set(list, index, value) -> Unit
    if (fn_name == "list_set") {
        if (call.args.size() >= 3) {
            std::string list = gen_expr(*call.args[0]);
            std::string i32_index = gen_expr(*call.args[1]);
            std::string index = fresh_reg();
            emit_line("  " + index + " = sext i32 " + i32_index + " to i64");
            std::string i32_value = gen_expr(*call.args[2]);
            std::string value = fresh_reg();
            emit_line("  " + value + " = sext i32 " + i32_value + " to i64");
            emit_line("  call void @list_set(ptr " + list + ", i64 " + index + ", i64 " + value + ")");
        }
        return "0";
    }

    // list_len(list) -> I32
    if (fn_name == "list_len") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @list_len(ptr " + list + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // list_capacity(list) -> I32
    if (fn_name == "list_capacity") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @list_capacity(ptr " + list + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // list_clear(list) -> Unit
    if (fn_name == "list_clear") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            emit_line("  call void @list_clear(ptr " + list + ")");
        }
        return "0";
    }

    // list_is_empty(list) -> Bool
    if (fn_name == "list_is_empty") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @list_is_empty(ptr " + list + ")");
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            return bool_result;
        }
        return "0";
    }

    // ============ HASHMAP FUNCTIONS ============

    // hashmap_create(capacity) -> map_ptr
    if (fn_name == "hashmap_create") {
        if (call.args.empty()) {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @hashmap_create(i64 16)");
            return result;
        } else {
            std::string i32_cap = gen_expr(*call.args[0]);
            std::string cap = fresh_reg();
            emit_line("  " + cap + " = sext i32 " + i32_cap + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @hashmap_create(i64 " + cap + ")");
            return result;
        }
    }

    // hashmap_destroy(map) -> Unit
    if (fn_name == "hashmap_destroy") {
        if (!call.args.empty()) {
            std::string map = gen_expr(*call.args[0]);
            emit_line("  call void @hashmap_destroy(ptr " + map + ")");
        }
        return "0";
    }

    // hashmap_set(map, key, value) -> Unit
    if (fn_name == "hashmap_set") {
        if (call.args.size() >= 3) {
            std::string map = gen_expr(*call.args[0]);
            std::string i32_key = gen_expr(*call.args[1]);
            std::string key = fresh_reg();
            emit_line("  " + key + " = sext i32 " + i32_key + " to i64");
            std::string i32_value = gen_expr(*call.args[2]);
            std::string value = fresh_reg();
            emit_line("  " + value + " = sext i32 " + i32_value + " to i64");
            emit_line("  call void @hashmap_set(ptr " + map + ", i64 " + key + ", i64 " + value + ")");
        }
        return "0";
    }

    // hashmap_get(map, key) -> I32
    if (fn_name == "hashmap_get") {
        if (call.args.size() >= 2) {
            std::string map = gen_expr(*call.args[0]);
            std::string i32_key = gen_expr(*call.args[1]);
            std::string key = fresh_reg();
            emit_line("  " + key + " = sext i32 " + i32_key + " to i64");
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @hashmap_get(ptr " + map + ", i64 " + key + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // hashmap_has(map, key) -> Bool
    if (fn_name == "hashmap_has") {
        if (call.args.size() >= 2) {
            std::string map = gen_expr(*call.args[0]);
            std::string i32_key = gen_expr(*call.args[1]);
            std::string key = fresh_reg();
            emit_line("  " + key + " = sext i32 " + i32_key + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @hashmap_has(ptr " + map + ", i64 " + key + ")");
            return result;
        }
        return "0";
    }

    // hashmap_remove(map, key) -> Bool
    if (fn_name == "hashmap_remove") {
        if (call.args.size() >= 2) {
            std::string map = gen_expr(*call.args[0]);
            std::string i32_key = gen_expr(*call.args[1]);
            std::string key = fresh_reg();
            emit_line("  " + key + " = sext i32 " + i32_key + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @hashmap_remove(ptr " + map + ", i64 " + key + ")");
            return result;
        }
        return "0";
    }

    // hashmap_len(map) -> I32
    if (fn_name == "hashmap_len") {
        if (!call.args.empty()) {
            std::string map = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @hashmap_len(ptr " + map + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // hashmap_clear(map) -> Unit
    if (fn_name == "hashmap_clear") {
        if (!call.args.empty()) {
            std::string map = gen_expr(*call.args[0]);
            emit_line("  call void @hashmap_clear(ptr " + map + ")");
        }
        return "0";
    }

    // ============ BUFFER FUNCTIONS ============

    // buffer_create(capacity) -> buf_ptr
    if (fn_name == "buffer_create") {
        if (call.args.empty()) {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @buffer_create(i64 16)");
            return result;
        } else {
            std::string i32_cap = gen_expr(*call.args[0]);
            std::string cap = fresh_reg();
            emit_line("  " + cap + " = sext i32 " + i32_cap + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @buffer_create(i64 " + cap + ")");
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
