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

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_assert(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // assert_eq(left, right) - Assert two values are equal
    if (fn_name == "assert_eq") {
        if (call.args.size() >= 2) {
            std::string left = gen_expr(*call.args[0]);
            std::string left_type = last_expr_type_;
            bool left_unsigned = last_expr_is_unsigned_;
            std::string right = gen_expr(*call.args[1]);
            std::string right_type = last_expr_type_;
            bool right_unsigned = last_expr_is_unsigned_;

            // Determine the comparison type
            std::string cmp_type = left_type;
            if (cmp_type.empty())
                cmp_type = "i32";

            // Get source location and optional message for better error messages
            std::string file_literal = add_string_literal(options_.source_file);
            int line = static_cast<int>(call.span.start.line);
            std::string msg_literal;
            if (call.args.size() >= 3) {
                // User provided a message
                msg_literal = gen_expr(*call.args[2]);
            } else {
                msg_literal = add_string_literal("values not equal");
            }

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
                emit_line("  call void @assert_tml_loc(i32 0, ptr " + msg_literal + ", ptr " +
                          file_literal + ", i32 " + std::to_string(line) + ")");
                emit_line("  unreachable");

                emit_line(ok_label + ":");
                last_expr_type_ = "void";
                return "0";
            }

            // Handle type mismatches for all integer widths (i8, i16, i32, i64)
            if (left_type != right_type) {
                auto get_int_bits = [](const std::string& ty) -> int {
                    if (ty == "i1")
                        return 1;
                    if (ty == "i8")
                        return 8;
                    if (ty == "i16")
                        return 16;
                    if (ty == "i32")
                        return 32;
                    if (ty == "i64")
                        return 64;
                    if (ty == "i128")
                        return 128;
                    return 0;
                };
                int left_bits = get_int_bits(left_type);
                int right_bits = get_int_bits(right_type);

                if (left_bits > 0 && right_bits > 0 && left_bits != right_bits) {
                    // Extend the smaller operand to match the larger one
                    if (left_bits < right_bits) {
                        std::string ext_reg = fresh_reg();
                        std::string ext_op = left_unsigned ? "zext" : "sext";
                        emit_line("  " + ext_reg + " = " + ext_op + " " + left_type + " " + left +
                                  " to " + right_type);
                        left = ext_reg;
                        cmp_type = right_type;
                    } else {
                        std::string ext_reg = fresh_reg();
                        std::string ext_op = right_unsigned ? "zext" : "sext";
                        emit_line("  " + ext_reg + " = " + ext_op + " " + right_type + " " + right +
                                  " to " + left_type);
                        right = ext_reg;
                        cmp_type = left_type;
                    }
                }
            }

            // For numeric/bool types, use icmp (or fcmp for floats)
            std::string cmp_result = fresh_reg();
            bool is_float_cmp = (cmp_type == "float" || cmp_type == "double");
            if (is_float_cmp) {
                emit_line("  " + cmp_result + " = fcmp oeq " + cmp_type + " " + left + ", " +
                          right);
            } else {
                emit_line("  " + cmp_result + " = icmp eq " + cmp_type + " " + left + ", " + right);
            }

            std::string ok_label = fresh_label("assert_ok");
            std::string fail_label = fresh_label("assert_fail");
            emit_line("  br i1 " + cmp_result + ", label %" + ok_label + ", label %" + fail_label);

            emit_line(fail_label + ":");
            emit_line("  call void @assert_tml_loc(i32 0, ptr " + msg_literal + ", ptr " +
                      file_literal + ", i32 " + std::to_string(line) + ")");
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
            bool left_unsigned = last_expr_is_unsigned_;
            std::string right = gen_expr(*call.args[1]);
            std::string right_type = last_expr_type_;
            bool right_unsigned = last_expr_is_unsigned_;

            // Get source location and optional message
            std::string file_literal = add_string_literal(options_.source_file);
            int line = static_cast<int>(call.span.start.line);
            std::string msg_literal;
            if (call.args.size() >= 3) {
                msg_literal = gen_expr(*call.args[2]);
            } else {
                msg_literal = add_string_literal("values are equal");
            }

            std::string cmp_type = left_type;
            if (cmp_type.empty())
                cmp_type = "i32";

            // Handle type mismatches for all integer widths (i8, i16, i32, i64)
            if (left_type != right_type) {
                auto get_int_bits = [](const std::string& ty) -> int {
                    if (ty == "i1")
                        return 1;
                    if (ty == "i8")
                        return 8;
                    if (ty == "i16")
                        return 16;
                    if (ty == "i32")
                        return 32;
                    if (ty == "i64")
                        return 64;
                    if (ty == "i128")
                        return 128;
                    return 0;
                };
                int left_bits = get_int_bits(left_type);
                int right_bits = get_int_bits(right_type);

                if (left_bits > 0 && right_bits > 0 && left_bits != right_bits) {
                    if (left_bits < right_bits) {
                        std::string ext_reg = fresh_reg();
                        std::string ext_op = left_unsigned ? "zext" : "sext";
                        emit_line("  " + ext_reg + " = " + ext_op + " " + left_type + " " + left +
                                  " to " + right_type);
                        left = ext_reg;
                        cmp_type = right_type;
                    } else {
                        std::string ext_reg = fresh_reg();
                        std::string ext_op = right_unsigned ? "zext" : "sext";
                        emit_line("  " + ext_reg + " = " + ext_op + " " + right_type + " " + right +
                                  " to " + left_type);
                        right = ext_reg;
                        cmp_type = left_type;
                    }
                }
            }

            std::string cmp_result = fresh_reg();
            bool is_float_cmp = (cmp_type == "float" || cmp_type == "double");
            if (is_float_cmp) {
                emit_line("  " + cmp_result + " = fcmp one " + cmp_type + " " + left + ", " +
                          right);
            } else {
                emit_line("  " + cmp_result + " = icmp ne " + cmp_type + " " + left + ", " + right);
            }

            std::string ok_label = fresh_label("assert_ok");
            std::string fail_label = fresh_label("assert_fail");
            emit_line("  br i1 " + cmp_result + ", label %" + ok_label + ", label %" + fail_label);

            emit_line(fail_label + ":");
            emit_line("  call void @assert_tml_loc(i32 0, ptr " + msg_literal + ", ptr " +
                      file_literal + ", i32 " + std::to_string(line) + ")");
            emit_line("  unreachable");

            emit_line(ok_label + ":");
            last_expr_type_ = "void";
            return "0";
        }
        last_expr_type_ = "void";
        return "0";
    }

    // assert(condition) or assert(condition, message) - Assert condition is true
    if (fn_name == "assert") {
        if (!call.args.empty()) {
            std::string cond = gen_expr(*call.args[0]);
            std::string cond_type = last_expr_type_;

            // Convert non-i1 integer conditions to i1 (e.g., C runtime functions returning i32)
            if (cond_type != "i1" &&
                (cond_type == "i8" || cond_type == "i16" || cond_type == "i32" ||
                 cond_type == "i64" || cond_type == "i128")) {
                std::string bool_cond = fresh_reg();
                emit_line("  " + bool_cond + " = icmp ne " + cond_type + " " + cond + ", 0");
                cond = bool_cond;
            }

            // Get optional message argument
            std::string msg_literal;
            if (call.args.size() >= 2) {
                // Message provided as second argument
                std::string user_msg = gen_expr(*call.args[1]);
                msg_literal = user_msg; // Already a ptr to string
            } else {
                msg_literal = add_string_literal("condition was false");
            }

            // Get source location for better error messages
            std::string file_literal = add_string_literal(options_.source_file);
            int line = static_cast<int>(call.span.start.line);

            std::string ok_label = fresh_label("assert_ok");
            std::string fail_label = fresh_label("assert_fail");
            emit_line("  br i1 " + cond + ", label %" + ok_label + ", label %" + fail_label);

            emit_line(fail_label + ":");
            // Use assert_tml_loc with file and line info
            emit_line("  call void @assert_tml_loc(i32 0, ptr " + msg_literal + ", ptr " +
                      file_literal + ", i32 " + std::to_string(line) + ")");
            emit_line("  unreachable");

            emit_line(ok_label + ":");
            last_expr_type_ = "void";
            return "0";
        }
        last_expr_type_ = "void";
        return "0";
    }

    // assert_true(value, message) - Assert value is true (alias for assert)
    if (fn_name == "assert_true") {
        if (!call.args.empty()) {
            std::string cond = gen_expr(*call.args[0]);
            std::string cond_type = last_expr_type_;

            // Convert non-i1 integer conditions to i1
            if (cond_type != "i1" &&
                (cond_type == "i8" || cond_type == "i16" || cond_type == "i32" ||
                 cond_type == "i64" || cond_type == "i128")) {
                std::string bool_cond = fresh_reg();
                emit_line("  " + bool_cond + " = icmp ne " + cond_type + " " + cond + ", 0");
                cond = bool_cond;
            }

            std::string msg_literal;
            if (call.args.size() >= 2) {
                msg_literal = gen_expr(*call.args[1]);
            } else {
                msg_literal = add_string_literal("expected true");
            }

            std::string file_literal = add_string_literal(options_.source_file);
            int line = static_cast<int>(call.span.start.line);

            std::string ok_label = fresh_label("assert_ok");
            std::string fail_label = fresh_label("assert_fail");
            emit_line("  br i1 " + cond + ", label %" + ok_label + ", label %" + fail_label);

            emit_line(fail_label + ":");
            emit_line("  call void @assert_tml_loc(i32 0, ptr " + msg_literal + ", ptr " +
                      file_literal + ", i32 " + std::to_string(line) + ")");
            emit_line("  unreachable");

            emit_line(ok_label + ":");
            last_expr_type_ = "void";
            return "0";
        }
        last_expr_type_ = "void";
        return "0";
    }

    // assert_false(condition, message) - Assert condition is false
    if (fn_name == "assert_false") {
        if (!call.args.empty()) {
            std::string cond = gen_expr(*call.args[0]);
            std::string cond_type = last_expr_type_;

            // Convert non-i1 integer conditions to i1
            if (cond_type != "i1" &&
                (cond_type == "i8" || cond_type == "i16" || cond_type == "i32" ||
                 cond_type == "i64" || cond_type == "i128")) {
                std::string bool_cond = fresh_reg();
                emit_line("  " + bool_cond + " = icmp ne " + cond_type + " " + cond + ", 0");
                cond = bool_cond;
            }

            std::string msg_literal;
            if (call.args.size() >= 2) {
                msg_literal = gen_expr(*call.args[1]);
            } else {
                msg_literal = add_string_literal("expected false");
            }

            std::string file_literal = add_string_literal(options_.source_file);
            int line = static_cast<int>(call.span.start.line);

            // Inverted: branch to fail if condition is TRUE
            std::string ok_label = fresh_label("assert_ok");
            std::string fail_label = fresh_label("assert_fail");
            emit_line("  br i1 " + cond + ", label %" + fail_label + ", label %" + ok_label);

            emit_line(fail_label + ":");
            emit_line("  call void @assert_tml_loc(i32 0, ptr " + msg_literal + ", ptr " +
                      file_literal + ", i32 " + std::to_string(line) + ")");
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
