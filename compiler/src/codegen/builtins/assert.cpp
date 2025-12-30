// LLVM IR generator - Assert builtin functions
// Handles: assert_eq, assert_ne, assert

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_assert(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // assert_eq(left, right) - Assert two values are equal
    if (fn_name == "assert_eq") {
        if (call.args.size() >= 2) {
            std::string left = gen_expr(*call.args[0]);
            std::string left_type = last_expr_type_;
            std::string right = gen_expr(*call.args[1]);
            std::string right_type = last_expr_type_;

            // Determine the comparison type
            std::string cmp_type = left_type;
            if (cmp_type.empty())
                cmp_type = "i32";

            // For strings, use str_eq
            if (cmp_type == "ptr" || left_type == "ptr" || right_type == "ptr") {
                std::string cmp_result = fresh_reg();
                emit_line("  " + cmp_result + " = call i32 @str_eq(ptr " + left + ", ptr " + right +
                          ")");
                std::string bool_result = fresh_reg();
                emit_line("  " + bool_result + " = icmp ne i32 " + cmp_result + ", 0");

                std::string ok_label = fresh_label("assert_ok");
                std::string fail_label = fresh_label("assert_fail");
                emit_line("  br i1 " + bool_result + ", label %" + ok_label + ", label %" +
                          fail_label);

                emit_line(fail_label + ":");
                std::string msg = add_string_literal("assertion failed: values not equal");
                emit_line("  call void @panic(ptr " + msg + ")");
                emit_line("  unreachable");

                emit_line(ok_label + ":");
                return "";
            }

            // For numeric/bool types, use icmp
            std::string cmp_result = fresh_reg();
            emit_line("  " + cmp_result + " = icmp eq " + cmp_type + " " + left + ", " + right);

            std::string ok_label = fresh_label("assert_ok");
            std::string fail_label = fresh_label("assert_fail");
            emit_line("  br i1 " + cmp_result + ", label %" + ok_label + ", label %" + fail_label);

            emit_line(fail_label + ":");
            std::string msg = add_string_literal("assertion failed: values not equal");
            emit_line("  call void @panic(ptr " + msg + ")");
            emit_line("  unreachable");

            emit_line(ok_label + ":");
            return "";
        }
        return "";
    }

    // assert_ne(left, right) - Assert two values are not equal
    if (fn_name == "assert_ne") {
        if (call.args.size() >= 2) {
            std::string left = gen_expr(*call.args[0]);
            std::string left_type = last_expr_type_;
            std::string right = gen_expr(*call.args[1]);

            std::string cmp_type = left_type;
            if (cmp_type.empty())
                cmp_type = "i32";

            std::string cmp_result = fresh_reg();
            emit_line("  " + cmp_result + " = icmp ne " + cmp_type + " " + left + ", " + right);

            std::string ok_label = fresh_label("assert_ok");
            std::string fail_label = fresh_label("assert_fail");
            emit_line("  br i1 " + cmp_result + ", label %" + ok_label + ", label %" + fail_label);

            emit_line(fail_label + ":");
            std::string msg = add_string_literal("assertion failed: values are equal");
            emit_line("  call void @panic(ptr " + msg + ")");
            emit_line("  unreachable");

            emit_line(ok_label + ":");
            return "";
        }
        return "";
    }

    // assert(condition) - Assert condition is true
    if (fn_name == "assert") {
        if (!call.args.empty()) {
            std::string cond = gen_expr(*call.args[0]);

            std::string ok_label = fresh_label("assert_ok");
            std::string fail_label = fresh_label("assert_fail");
            emit_line("  br i1 " + cond + ", label %" + ok_label + ", label %" + fail_label);

            emit_line(fail_label + ":");
            std::string msg = add_string_literal("assertion failed");
            emit_line("  call void @panic(ptr " + msg + ")");
            emit_line("  unreachable");

            emit_line(ok_label + ":");
            return "";
        }
        return "";
    }

    return std::nullopt;
}

} // namespace tml::codegen
