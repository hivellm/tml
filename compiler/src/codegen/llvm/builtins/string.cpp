//! # LLVM IR Generator - Numeric/Float Builtins
//!
//! This file implements numeric and float intrinsics via try_gen_builtin_string().
//!
//! ## History
//!
//! Originally contained 13 `str_*` builtins (str_len, str_eq, str_concat, etc.)
//! and char builtins. All string builtins were removed in Phase 31:
//! - String operations migrated to inline LLVM IR defines in runtime.cpp
//! - Bare `str_*()` calls in TML replaced with method calls (`.len()`, `==`, etc.)
//! - Char operations migrated to pure TML in Phase 18.2
//!
//! ## Remaining Functions
//!
//! | Function                | Runtime Call                     |
//! |-------------------------|----------------------------------|
//! | `i*_to_string`          | `@i64_to_string` (inline IR)     |
//! | `u*_to_string`          | `@i64_to_string` (inline IR)     |
//! | `f32/f64_to_string`     | `@f64_to_str` (inline IR)        |
//! | `f*_to_string_precision` | `@float_to_precision` (math.c)  |
//! | `f*_to_exp_string`      | `@float_to_exp` (math.c)         |
//! | `f*_is_nan/is_infinite` | Pure LLVM IR (`fcmp`)            |
//! | `f64_round`             | `@llvm.round.f64` intrinsic     |

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_string(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // ========================================================================
    // Integer to String Conversions
    // ========================================================================

    // i8_to_string(n) -> Str
    if (fn_name == "i8_to_string") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string n_type = last_expr_type_;
            std::string n_i64 = n;
            if (n_type != "i64") {
                n_i64 = fresh_reg();
                emit_line("  " + n_i64 + " = sext " + n_type + " " + n + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @i64_to_string(i64 " + n_i64 + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // i16_to_string(n) -> Str
    if (fn_name == "i16_to_string") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string n_type = last_expr_type_;
            std::string n_i64 = n;
            if (n_type != "i64") {
                n_i64 = fresh_reg();
                emit_line("  " + n_i64 + " = sext " + n_type + " " + n + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @i64_to_string(i64 " + n_i64 + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // i32_to_string(n) -> Str
    if (fn_name == "i32_to_string") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string n_type = last_expr_type_;
            std::string n_i64 = n;
            if (n_type != "i64") {
                n_i64 = fresh_reg();
                emit_line("  " + n_i64 + " = sext " + n_type + " " + n + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @i64_to_string(i64 " + n_i64 + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // i64_to_string(n) -> Str
    if (fn_name == "i64_to_string") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string n_type = last_expr_type_;
            // Extend smaller types to i64
            std::string n_i64 = n;
            if (n_type != "i64") {
                n_i64 = fresh_reg();
                emit_line("  " + n_i64 + " = sext " + n_type + " " + n + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @i64_to_string(i64 " + n_i64 + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // u8_to_string(n) -> Str
    if (fn_name == "u8_to_string") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string n_type = last_expr_type_;
            // Extend to i64 for the runtime
            std::string n_i64 = n;
            if (n_type != "i64") {
                n_i64 = fresh_reg();
                emit_line("  " + n_i64 + " = zext " + n_type + " " + n + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @i64_to_string(i64 " + n_i64 + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // u16_to_string(n) -> Str
    if (fn_name == "u16_to_string") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string n_type = last_expr_type_;
            std::string n_i64 = n;
            if (n_type != "i64") {
                n_i64 = fresh_reg();
                emit_line("  " + n_i64 + " = zext " + n_type + " " + n + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @i64_to_string(i64 " + n_i64 + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // u32_to_string(n) -> Str
    if (fn_name == "u32_to_string") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string n_type = last_expr_type_;
            std::string n_i64 = n;
            if (n_type != "i64") {
                n_i64 = fresh_reg();
                emit_line("  " + n_i64 + " = zext " + n_type + " " + n + " to i64");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @i64_to_string(i64 " + n_i64 + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // u64_to_string(n) -> Str
    if (fn_name == "u64_to_string") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @i64_to_string(i64 " + n + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // ========================================================================
    // Float to String Conversions
    // ========================================================================

    // f32_to_string(n) -> Str
    if (fn_name == "f32_to_string") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string n_type = last_expr_type_;
            // Extend float to double for the runtime
            std::string n_f64 = n;
            if (n_type == "float") {
                n_f64 = fresh_reg();
                emit_line("  " + n_f64 + " = fpext float " + n + " to double");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @f64_to_str(double " + n_f64 + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // f64_to_string(n) -> Str
    if (fn_name == "f64_to_string") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @f64_to_str(double " + n + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // f32_to_string_precision(n, precision) -> Str
    if (fn_name == "f32_to_string_precision") {
        if (call.args.size() >= 2) {
            std::string n = gen_expr(*call.args[0]);
            std::string n_type = last_expr_type_;
            // Extend float to double for the runtime
            std::string n_f64 = n;
            if (n_type == "float") {
                n_f64 = fresh_reg();
                emit_line("  " + n_f64 + " = fpext float " + n + " to double");
            }
            std::string prec = gen_expr(*call.args[1]);
            std::string prec_type = last_expr_type_;
            // Truncate precision to i32 if needed
            std::string prec_i32 = prec;
            if (prec_type == "i64") {
                prec_i32 = fresh_reg();
                emit_line("  " + prec_i32 + " = trunc i64 " + prec + " to i32");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @float_to_precision(double " + n_f64 + ", i32 " +
                      prec_i32 + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // f64_to_string_precision(n, precision) -> Str
    if (fn_name == "f64_to_string_precision") {
        if (call.args.size() >= 2) {
            std::string n = gen_expr(*call.args[0]);
            std::string prec = gen_expr(*call.args[1]);
            std::string prec_type = last_expr_type_;
            // Truncate precision to i32 if needed
            std::string prec_i32 = prec;
            if (prec_type == "i64") {
                prec_i32 = fresh_reg();
                emit_line("  " + prec_i32 + " = trunc i64 " + prec + " to i32");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @float_to_precision(double " + n + ", i32 " +
                      prec_i32 + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // f32_to_exp_string(n, uppercase) -> Str
    if (fn_name == "f32_to_exp_string") {
        if (call.args.size() >= 2) {
            std::string n = gen_expr(*call.args[0]);
            std::string n_type = last_expr_type_;
            // Extend float to double for the runtime
            std::string n_f64 = n;
            if (n_type == "float") {
                n_f64 = fresh_reg();
                emit_line("  " + n_f64 + " = fpext float " + n + " to double");
            }
            std::string upper = gen_expr(*call.args[1]);
            std::string upper_type = last_expr_type_;
            // Convert bool to i32 if needed
            std::string upper_i32 = upper;
            if (upper_type == "i1") {
                upper_i32 = fresh_reg();
                emit_line("  " + upper_i32 + " = zext i1 " + upper + " to i32");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @float_to_exp(double " + n_f64 + ", i32 " +
                      upper_i32 + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // f64_to_exp_string(n, uppercase) -> Str
    if (fn_name == "f64_to_exp_string") {
        if (call.args.size() >= 2) {
            std::string n = gen_expr(*call.args[0]);
            std::string upper = gen_expr(*call.args[1]);
            std::string upper_type = last_expr_type_;
            // Convert bool to i32 if needed
            std::string upper_i32 = upper;
            if (upper_type == "i1") {
                upper_i32 = fresh_reg();
                emit_line("  " + upper_i32 + " = zext i1 " + upper + " to i32");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @float_to_exp(double " + n + ", i32 " +
                      upper_i32 + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        last_expr_type_ = "ptr";
        return "null";
    }

    // ========================================================================
    // Float Intrinsics (pure LLVM IR, no C runtime)
    // ========================================================================

    // f32_is_nan(n) -> Bool — pure LLVM IR: fcmp uno (NaN != NaN)
    if (fn_name == "f32_is_nan") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = fcmp uno float " + n + ", 0.0");
            last_expr_type_ = "i1";
            return result;
        }
        last_expr_type_ = "i1";
        return "0";
    }

    // f64_is_nan(n) -> Bool — pure LLVM IR: fcmp uno (NaN != NaN)
    if (fn_name == "f64_is_nan") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = fcmp uno double " + n + ", 0.0");
            last_expr_type_ = "i1";
            return result;
        }
        last_expr_type_ = "i1";
        return "0";
    }

    // f32_is_infinite(n) -> Bool — pure LLVM IR: fabs + fcmp oeq +inf
    if (fn_name == "f32_is_infinite") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string abs_val = fresh_reg();
            emit_line("  " + abs_val + " = call float @llvm.fabs.f32(float " + n + ")");
            std::string result = fresh_reg();
            // Float +inf = 0x7F800000, LLVM float hex = upper 32 bits of 64-bit
            emit_line("  " + result + " = fcmp oeq float " + abs_val + ", 0x7F80000000000000");
            last_expr_type_ = "i1";
            return result;
        }
        last_expr_type_ = "i1";
        return "0";
    }

    // f64_is_infinite(n) -> Bool — pure LLVM IR: fabs + fcmp oeq +inf
    if (fn_name == "f64_is_infinite") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string abs_val = fresh_reg();
            emit_line("  " + abs_val + " = call double @llvm.fabs.f64(double " + n + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = fcmp oeq double " + abs_val + ", 0x7FF0000000000000");
            last_expr_type_ = "i1";
            return result;
        }
        last_expr_type_ = "i1";
        return "0";
    }

    // f64_round(n) -> F64
    if (fn_name == "f64_round") {
        if (!call.args.empty()) {
            std::string n = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @llvm.round.f64(double " + n + ")");
            last_expr_type_ = "double";
            return result;
        }
        last_expr_type_ = "double";
        return "0.0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
