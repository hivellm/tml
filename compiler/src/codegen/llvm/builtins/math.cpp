//! # LLVM IR Generator - Math Builtins
//!
//! This file implements mathematical builtin functions via try_gen_builtin_math().
//!
//! ## History
//!
//! Originally contained handlers for 20+ math builtins. Most removed as dead code:
//! - Phase 34: nextafter32 removed
//! - Phase 36: float_to_fixed, float_to_precision, float_to_string removed
//! - Phase 38: fneg, int_to_float, float_to_int, float_bits, round/floor/ceil/abs
//!   (guarded, bypassed by TML impls), nextafter removed
//!
//! ## Remaining Functions
//!
//! | Function    | LLVM Intrinsic/Code         |
//! |-------------|----------------------------|
//! | `sqrt`      | `@llvm.sqrt.f64`           |
//! | `pow`       | `@llvm.pow.f64`            |
//! | `black_box` | Inline asm barrier (`@black_box_*`) |
//! | `infinity`  | LLVM float constant + select |
//! | `nan`       | LLVM float constant (0x7FF8...) |
//! | `is_inf`    | `fcmp oeq` with +/-inf     |
//! | `is_nan`    | `fcmp uno`                 |

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_math(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // Helper: check if function is defined as a TML module function (not a builtin)
    auto is_module_func = [&](const std::string& name) -> bool {
        return env_.lookup_func(name).has_value();
    };

    // ============ BLACK BOX (prevent optimization) ============

    // black_box(value: I32) -> I32 - Prevent LLVM from optimizing away
    if (fn_name == "black_box") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @black_box_i32(i32 " + value + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // black_box_i64(value: I64) -> I64 - Prevent LLVM from optimizing away
    if (fn_name == "black_box_i64") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @black_box_i64(i64 " + value + ")");
            last_expr_type_ = "i64";
            return result;
        }
        return "0";
    }

    // black_box_f64(value: F64) -> F64 - Prevent LLVM from optimizing away
    if (fn_name == "black_box_f64") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @black_box_f64(double " + value + ")");
            last_expr_type_ = "double";
            return result;
        }
        return "0.0";
    }

    // SIMD operations (simd_sum_i32, simd_sum_f64, simd_dot_f64) — dispatched
    // through TML implementations in std::math::simd (Phase 33: removed hardcoded codegen)

    // fneg_f32, fneg_f64 — removed in Phase 38 (dead code: 0 TML callers)
    // int_to_float/toFloat, float_to_int/toInt — removed in Phase 38 (dead code: 0 TML callers)
    // float_to_fixed, float_to_precision, float_to_string — removed in Phase 36 (dead code)
    // float_round/round, float_floor/floor, float_ceil/ceil, float_abs/abs — removed in Phase 38
    //   (dead code: TML implementations exist in std::math, is_module_func guard always skips
    //   these)

    // sqrt(value: F64) -> F64 — LLVM @llvm.sqrt.f64
    if (fn_name == "float_sqrt" || fn_name == "sqrt") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val;
            if (last_expr_type_ == "i32" || last_expr_type_ == "i64") {
                double_val = fresh_reg();
                emit_line("  " + double_val + " = sitofp " + last_expr_type_ + " " + value +
                          " to double");
            } else {
                double_val = value;
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @llvm.sqrt.f64(double " + double_val + ")");
            last_expr_type_ = "double";
            return result;
        }
        return "0.0";
    }

    // pow(base: F64, exp: I32/I64) -> F64 — LLVM @llvm.pow.f64 (exp converted to double)
    if (fn_name == "float_pow" || fn_name == "pow") {
        if (call.args.size() >= 2) {
            std::string base = gen_expr(*call.args[0]);
            std::string base_type = last_expr_type_;
            std::string exp = gen_expr(*call.args[1]);
            std::string exp_type = last_expr_type_;
            std::string double_base;
            if (base_type == "i32" || base_type == "i64") {
                double_base = fresh_reg();
                emit_line("  " + double_base + " = sitofp " + base_type + " " + base +
                          " to double");
            } else {
                double_base = base;
            }
            // Convert exponent to double for @llvm.pow.f64
            std::string double_exp;
            if (exp_type == "double" || exp_type == "float") {
                double_exp = exp;
            } else {
                double_exp = fresh_reg();
                emit_line("  " + double_exp + " = sitofp " + exp_type + " " + exp + " to double");
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @llvm.pow.f64(double " + double_base +
                      ", double " + double_exp + ")");
            last_expr_type_ = "double";
            return result;
        }
        return "1.0";
    }

    // float32_bits, float32_from_bits, float64_bits, float64_from_bits — removed in Phase 38
    // (dead code: 0 TML callers)

    // ============ SPECIAL FLOAT VALUES — inline LLVM constants + fcmp ============

    // infinity(sign: I32) -> F64 — LLVM constant
    if (fn_name == "infinity") {
        if (!call.args.empty()) {
            std::string sign = gen_expr(*call.args[0]);
            // Check if sign is negative (sign < 0)
            std::string is_neg = fresh_reg();
            emit_line("  " + is_neg + " = icmp slt i32 " + sign + ", 0");
            std::string result = fresh_reg();
            // 0x7FF0000000000000 = +inf, 0xFFF0000000000000 = -inf
            emit_line("  " + result + " = select i1 " + is_neg +
                      ", double 0xFFF0000000000000, double 0x7FF0000000000000");
            last_expr_type_ = "double";
            return result;
        }
        // Default to positive infinity
        last_expr_type_ = "double";
        return "0x7FF0000000000000";
    }

    // nan() -> F64 — LLVM constant (quiet NaN)
    if (fn_name == "nan") {
        last_expr_type_ = "double";
        return "0x7FF8000000000000";
    }

    // is_inf(f: F64, sign: I32) -> Bool — LLVM fcmp
    if (fn_name == "is_inf") {
        if (call.args.size() >= 2) {
            std::string f = gen_expr(*call.args[0]);
            std::string sign = gen_expr(*call.args[1]);
            // Check sign: positive (1), negative (-1), or either (0)
            std::string is_pos_inf = fresh_reg();
            emit_line("  " + is_pos_inf + " = fcmp oeq double " + f + ", 0x7FF0000000000000");
            std::string is_neg_inf = fresh_reg();
            emit_line("  " + is_neg_inf + " = fcmp oeq double " + f + ", 0xFFF0000000000000");
            std::string is_any_inf = fresh_reg();
            emit_line("  " + is_any_inf + " = or i1 " + is_pos_inf + ", " + is_neg_inf);
            // sign == 0 -> either, sign > 0 -> positive, sign < 0 -> negative
            std::string sign_zero = fresh_reg();
            emit_line("  " + sign_zero + " = icmp eq i32 " + sign + ", 0");
            std::string sign_pos = fresh_reg();
            emit_line("  " + sign_pos + " = icmp sgt i32 " + sign + ", 0");
            std::string pos_check = fresh_reg();
            emit_line("  " + pos_check + " = select i1 " + sign_pos + ", i1 " + is_pos_inf +
                      ", i1 " + is_neg_inf);
            std::string result = fresh_reg();
            emit_line("  " + result + " = select i1 " + sign_zero + ", i1 " + is_any_inf + ", i1 " +
                      pos_check);
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // is_nan(f: F64) -> Bool — LLVM fcmp uno
    if (fn_name == "is_nan") {
        if (!call.args.empty()) {
            std::string f = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = fcmp uno double " + f + ", 0.0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // nextafter — removed in Phase 38 (dead code: 0 TML callers)
    // nextafter32 — removed in Phase 34 (dead code, no TML callers)

    return std::nullopt;
}

} // namespace tml::codegen
