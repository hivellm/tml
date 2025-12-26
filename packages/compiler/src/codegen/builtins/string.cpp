// LLVM IR generator - String builtin functions
// Handles: str_len, str_hash, str_eq, str_concat, str_substring,
//          str_contains, str_starts_with, str_ends_with, str_to_upper,
//          str_to_lower, str_trim, str_char_at

#include "tml/codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_string(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // str_len(s) -> I32
    if (fn_name == "str_len") {
        if (!call.args.empty()) {
            std::string s = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @str_len(ptr " + s + ")");
            return result;
        }
        return "0";
    }

    // str_hash(s) -> I32
    if (fn_name == "str_hash") {
        if (!call.args.empty()) {
            std::string s = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @str_hash(ptr " + s + ")");
            return result;
        }
        return "0";
    }

    // str_eq(a, b) -> Bool
    if (fn_name == "str_eq") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @str_eq(ptr " + a + ", ptr " + b + ")");
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            last_expr_type_ = "i1";
            return bool_result;
        }
        return "0";
    }

    // str_concat(a, b) -> Str
    if (fn_name == "str_concat") {
        if (call.args.size() >= 2) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_concat(ptr " + a + ", ptr " + b + ")");
            return result;
        }
        return "null";
    }

    // str_substring(s, start, len) -> Str
    if (fn_name == "str_substring") {
        if (call.args.size() >= 3) {
            std::string s = gen_expr(*call.args[0]);
            std::string start = gen_expr(*call.args[1]);
            std::string len = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_substring(ptr " + s + ", i32 " + start + ", i32 " + len + ")");
            return result;
        }
        return "null";
    }

    // str_contains(haystack, needle) -> Bool
    if (fn_name == "str_contains") {
        if (call.args.size() >= 2) {
            std::string h = gen_expr(*call.args[0]);
            std::string n = gen_expr(*call.args[1]);
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @str_contains(ptr " + h + ", ptr " + n + ")");
            // Convert i32 to i1 (Bool)
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // str_starts_with(s, prefix) -> Bool
    if (fn_name == "str_starts_with") {
        if (call.args.size() >= 2) {
            std::string s = gen_expr(*call.args[0]);
            std::string p = gen_expr(*call.args[1]);
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @str_starts_with(ptr " + s + ", ptr " + p + ")");
            // Convert i32 to i1 (Bool)
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // str_ends_with(s, suffix) -> Bool
    if (fn_name == "str_ends_with") {
        if (call.args.size() >= 2) {
            std::string s = gen_expr(*call.args[0]);
            std::string suffix = gen_expr(*call.args[1]);
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @str_ends_with(ptr " + s + ", ptr " + suffix + ")");
            // Convert i32 to i1 (Bool)
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // str_to_upper(s) -> Str
    if (fn_name == "str_to_upper") {
        if (!call.args.empty()) {
            std::string s = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_to_upper(ptr " + s + ")");
            return result;
        }
        return "null";
    }

    // str_to_lower(s) -> Str
    if (fn_name == "str_to_lower") {
        if (!call.args.empty()) {
            std::string s = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_to_lower(ptr " + s + ")");
            return result;
        }
        return "null";
    }

    // str_trim(s) -> Str
    if (fn_name == "str_trim") {
        if (!call.args.empty()) {
            std::string s = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @str_trim(ptr " + s + ")");
            return result;
        }
        return "null";
    }

    // str_char_at(s, index) -> Char (I32)
    if (fn_name == "str_char_at") {
        if (call.args.size() >= 2) {
            std::string s = gen_expr(*call.args[0]);
            std::string idx = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @str_char_at(ptr " + s + ", i32 " + idx + ")");
            return result;
        }
        return "0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
