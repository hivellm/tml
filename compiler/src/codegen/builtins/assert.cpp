//! # LLVM IR Generator - Assert Builtins
//!
//! This file implements assertion intrinsics for testing and debugging.
//!
//! ## Functions
//!
//! | Function    | Comparison | On Failure         |
//! |-------------|------------|---------------------|
//! | `assert`    | `cond`     | panic if false      |
//! | `assert_eq` | `icmp eq`  | panic if not equal  |
//! | `assert_ne` | `icmp ne`  | panic if equal      |
//!
//! ## Type Handling
//!
//! - **Strings**: Uses `str_eq` runtime function
//! - **Integers**: Automatic sign extension for mixed types (i32/i64)
//! - **Booleans**: Direct `icmp` comparison
//!
//! ## Generated Pattern
//!
//! ```llvm
//! %cmp = icmp eq i32 %left, %right
//! br i1 %cmp, label %assert_ok, label %assert_fail
//! assert_fail:
//!   call void @panic(ptr @msg)
//!   unreachable
//! assert_ok:
//!   ; continue
//! ```

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
                last_expr_type_ = "void";
                return "0";
            }

            // Handle type mismatches (e.g., i32 vs i64)
            if (left_type != right_type) {
                // If types differ, convert smaller to larger
                if (left_type == "i32" && right_type == "i64") {
                    // Sign-extend i32 left value to i64
                    std::string ext_reg = fresh_reg();
                    emit_line("  " + ext_reg + " = sext i32 " + left + " to i64");
                    left = ext_reg;
                    cmp_type = "i64";
                } else if (left_type == "i64" && right_type == "i32") {
                    // Sign-extend i32 right value to i64
                    std::string ext_reg = fresh_reg();
                    emit_line("  " + ext_reg + " = sext i32 " + right + " to i64");
                    right = ext_reg;
                    cmp_type = "i64";
                }
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
            last_expr_type_ = "void";
            return "0";
        }
        last_expr_type_ = "void";
        return "0";
    }

    // assert_ne(left, right) - Assert two values are not equal
    if (fn_name == "assert_ne") {
        if (call.args.size() >= 2) {
            std::string left = gen_expr(*call.args[0]);
            std::string left_type = last_expr_type_;
            std::string right = gen_expr(*call.args[1]);
            std::string right_type = last_expr_type_;

            std::string cmp_type = left_type;
            if (cmp_type.empty())
                cmp_type = "i32";

            // Handle type mismatches (e.g., i32 vs i64)
            if (left_type != right_type) {
                // If types differ, convert smaller to larger
                if (left_type == "i32" && right_type == "i64") {
                    std::string ext_reg = fresh_reg();
                    emit_line("  " + ext_reg + " = sext i32 " + left + " to i64");
                    left = ext_reg;
                    cmp_type = "i64";
                } else if (left_type == "i64" && right_type == "i32") {
                    std::string ext_reg = fresh_reg();
                    emit_line("  " + ext_reg + " = sext i32 " + right + " to i64");
                    right = ext_reg;
                    cmp_type = "i64";
                }
            }

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
            last_expr_type_ = "void";
            return "0";
        }
        last_expr_type_ = "void";
        return "0";
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
            last_expr_type_ = "void";
            return "0";
        }
        last_expr_type_ = "void";
        return "0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
