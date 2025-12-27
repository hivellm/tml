// LLVM IR generator - String and Char builtin functions
// Handles: str_len, str_hash, str_eq, str_concat, str_substring,
//          str_contains, str_starts_with, str_ends_with, str_to_upper,
//          str_to_lower, str_trim, str_char_at
// Char:    char_is_alphabetic, char_is_numeric, char_is_alphanumeric,
//          char_is_whitespace, char_is_uppercase, char_is_lowercase,
//          char_is_ascii, char_is_control, char_to_uppercase, char_to_lowercase,
//          char_to_digit, char_from_digit, char_code, char_from_code

#include "tml/codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_string(const std::string& fn_name,
                                       const parser::CallExpr& call) -> std::optional<std::string> {

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
            emit_line("  " + result + " = call ptr @str_substring(ptr " + s + ", i32 " + start +
                      ", i32 " + len + ")");
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
            emit_line("  " + i32_result + " = call i32 @str_contains(ptr " + h + ", ptr " + n +
                      ")");
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
            emit_line("  " + i32_result + " = call i32 @str_starts_with(ptr " + s + ", ptr " + p +
                      ")");
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
            emit_line("  " + i32_result + " = call i32 @str_ends_with(ptr " + s + ", ptr " +
                      suffix + ")");
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

    // ========================================================================
    // Char Operations
    // ========================================================================

    // char_is_alphabetic(c) -> Bool
    if (fn_name == "char_is_alphabetic") {
        if (!call.args.empty()) {
            std::string c = gen_expr(*call.args[0]);
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @char_is_alphabetic(i32 " + c + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // char_is_numeric(c) -> Bool
    if (fn_name == "char_is_numeric") {
        if (!call.args.empty()) {
            std::string c = gen_expr(*call.args[0]);
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @char_is_numeric(i32 " + c + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // char_is_alphanumeric(c) -> Bool
    if (fn_name == "char_is_alphanumeric") {
        if (!call.args.empty()) {
            std::string c = gen_expr(*call.args[0]);
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @char_is_alphanumeric(i32 " + c + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // char_is_whitespace(c) -> Bool
    if (fn_name == "char_is_whitespace") {
        if (!call.args.empty()) {
            std::string c = gen_expr(*call.args[0]);
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @char_is_whitespace(i32 " + c + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // char_is_uppercase(c) -> Bool
    if (fn_name == "char_is_uppercase") {
        if (!call.args.empty()) {
            std::string c = gen_expr(*call.args[0]);
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @char_is_uppercase(i32 " + c + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // char_is_lowercase(c) -> Bool
    if (fn_name == "char_is_lowercase") {
        if (!call.args.empty()) {
            std::string c = gen_expr(*call.args[0]);
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @char_is_lowercase(i32 " + c + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // char_is_ascii(c) -> Bool
    if (fn_name == "char_is_ascii") {
        if (!call.args.empty()) {
            std::string c = gen_expr(*call.args[0]);
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @char_is_ascii(i32 " + c + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // char_is_control(c) -> Bool
    if (fn_name == "char_is_control") {
        if (!call.args.empty()) {
            std::string c = gen_expr(*call.args[0]);
            std::string i32_result = fresh_reg();
            emit_line("  " + i32_result + " = call i32 @char_is_control(i32 " + c + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + i32_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // char_to_uppercase(c) -> Char
    if (fn_name == "char_to_uppercase") {
        if (!call.args.empty()) {
            std::string c = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @char_to_uppercase(i32 " + c + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // char_to_lowercase(c) -> Char
    if (fn_name == "char_to_lowercase") {
        if (!call.args.empty()) {
            std::string c = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @char_to_lowercase(i32 " + c + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // char_to_digit(c, radix) -> I32
    if (fn_name == "char_to_digit") {
        if (call.args.size() >= 2) {
            std::string c = gen_expr(*call.args[0]);
            std::string radix = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @char_to_digit(i32 " + c + ", i32 " + radix +
                      ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "-1";
    }

    // char_from_digit(digit, radix) -> Char
    if (fn_name == "char_from_digit") {
        if (call.args.size() >= 2) {
            std::string digit = gen_expr(*call.args[0]);
            std::string radix = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @char_from_digit(i32 " + digit + ", i32 " +
                      radix + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // char_code(c) -> I32
    if (fn_name == "char_code") {
        if (!call.args.empty()) {
            std::string c = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @char_code(i32 " + c + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // char_from_code(code) -> Char
    if (fn_name == "char_from_code") {
        if (!call.args.empty()) {
            std::string code = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @char_from_code(i32 " + code + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // ========================================================================
    // StringBuilder Operations (Mutable String)
    // ========================================================================

    // strbuilder_create(capacity) -> *Unit
    if (fn_name == "strbuilder_create") {
        std::string cap = "16"; // default capacity
        if (!call.args.empty()) {
            cap = gen_expr(*call.args[0]);
        }
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @strbuilder_create(i64 " + cap + ")");
        last_expr_type_ = "ptr";
        return result;
    }

    // strbuilder_destroy(sb) -> Unit
    if (fn_name == "strbuilder_destroy") {
        if (!call.args.empty()) {
            std::string sb = gen_expr(*call.args[0]);
            emit_line("  call void @strbuilder_destroy(ptr " + sb + ")");
            return "";
        }
        return "";
    }

    // strbuilder_push(sb, c) -> Unit
    if (fn_name == "strbuilder_push") {
        if (call.args.size() >= 2) {
            std::string sb = gen_expr(*call.args[0]);
            std::string c = gen_expr(*call.args[1]);
            emit_line("  call void @strbuilder_push(ptr " + sb + ", i32 " + c + ")");
            return "";
        }
        return "";
    }

    // strbuilder_push_str(sb, s) -> Unit
    if (fn_name == "strbuilder_push_str") {
        if (call.args.size() >= 2) {
            std::string sb = gen_expr(*call.args[0]);
            std::string s = gen_expr(*call.args[1]);
            emit_line("  call void @strbuilder_push_str(ptr " + sb + ", ptr " + s + ")");
            return "";
        }
        return "";
    }

    // strbuilder_len(sb) -> I64
    if (fn_name == "strbuilder_len") {
        if (!call.args.empty()) {
            std::string sb = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @strbuilder_len(ptr " + sb + ")");
            last_expr_type_ = "i64";
            return result;
        }
        return "0";
    }

    // strbuilder_capacity(sb) -> I64
    if (fn_name == "strbuilder_capacity") {
        if (!call.args.empty()) {
            std::string sb = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @strbuilder_capacity(ptr " + sb + ")");
            last_expr_type_ = "i64";
            return result;
        }
        return "0";
    }

    // strbuilder_clear(sb) -> Unit
    if (fn_name == "strbuilder_clear") {
        if (!call.args.empty()) {
            std::string sb = gen_expr(*call.args[0]);
            emit_line("  call void @strbuilder_clear(ptr " + sb + ")");
            return "";
        }
        return "";
    }

    // strbuilder_to_str(sb) -> Str
    if (fn_name == "strbuilder_to_str") {
        if (!call.args.empty()) {
            std::string sb = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @strbuilder_to_str(ptr " + sb + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    // strbuilder_as_str(sb) -> Str
    if (fn_name == "strbuilder_as_str") {
        if (!call.args.empty()) {
            std::string sb = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @strbuilder_as_str(ptr " + sb + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        return "null";
    }

    return std::nullopt;
}

} // namespace tml::codegen
