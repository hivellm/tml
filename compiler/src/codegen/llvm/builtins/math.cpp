//! # LLVM IR Generator - Math Builtins
//!
//! This file implements mathematical builtin functions.
//!
//! ## Functions
//!
//! | Function    | LLVM Intrinsic/Code         |
//! |-------------|----------------------------|
//! | `sqrt`      | `@llvm.sqrt.f64`           |
//! | `pow`       | `@llvm.pow.f64`            |
//! | `abs`       | Select or `@llvm.abs`      |
//! | `floor`     | `@llvm.floor.f64`          |
//! | `ceil`      | `@llvm.ceil.f64`           |
//! | `round`     | `@llvm.round.f64`          |
//! | `black_box` | Inline asm barrier         |
//!
//! ## Black Box
//!
//! `black_box()` prevents LLVM from optimizing away a value.
//! Used in benchmarks to ensure computations aren't eliminated.

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

    // ============ FLOAT NEGATION ============

    // fneg_f32(x: F32) -> F32
    if (fn_name == "fneg_f32") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = fneg float " + value);
            last_expr_type_ = "float";
            return result;
        }
        return "0.0";
    }

    // fneg_f64(x: F64) -> F64
    if (fn_name == "fneg_f64") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = fneg double " + value);
            last_expr_type_ = "double";
            return result;
        }
        return "0.0";
    }

    // ============ FLOAT FUNCTIONS ============

    // float_to_fixed(value: F64, decimals: I32) -> Str
    if (fn_name == "float_to_fixed" || fn_name == "toFixed") {
        if (call.args.size() >= 2) {
            std::string value = gen_expr(*call.args[0]);
            std::string decimals = gen_expr(*call.args[1]);
            // Convert to double if needed
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @float_to_fixed(double " + double_val +
                      ", i32 " + decimals + ")");
            return result;
        }
        return "0";
    }

    // float_to_precision(value: F64, precision: I32) -> Str
    if (fn_name == "float_to_precision" || fn_name == "toPrecision") {
        if (call.args.size() >= 2) {
            std::string value = gen_expr(*call.args[0]);
            std::string precision = gen_expr(*call.args[1]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @float_to_precision(double " + double_val +
                      ", i32 " + precision + ")");
            return result;
        }
        return "0";
    }

    // float_to_string(value: F64) -> Str
    if (fn_name == "float_to_string" || fn_name == "toString") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @float_to_string(double " + double_val + ")");
            return result;
        }
        return "0";
    }

    // int_to_float(value: I32/I64) -> F64 — inline LLVM sitofp
    if (fn_name == "int_to_float" || fn_name == "toFloat") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string value_type = last_expr_type_;
            std::string result = fresh_reg();
            emit_line("  " + result + " = sitofp " + value_type + " " + value + " to double");
            last_expr_type_ = "double";
            return result;
        }
        return "0.0";
    }

    // float_to_int(value: F64) -> I32 — inline LLVM fptosi
    if (fn_name == "float_to_int" || fn_name == "toInt") {
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
            emit_line("  " + result + " = fptosi double " + double_val + " to i32");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // float_round(value: F64) -> I32 — LLVM @llvm.round.f64 + fptosi
    if (fn_name == "float_round" || (fn_name == "round" && !is_module_func("round"))) {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string rounded = fresh_reg();
            emit_line("  " + rounded + " = call double @llvm.round.f64(double " + double_val + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = fptosi double " + rounded + " to i32");
            return result;
        }
        return "0";
    }

    // float_floor(value: F64) -> I32 — LLVM @llvm.floor.f64 + fptosi
    if (fn_name == "float_floor" || (fn_name == "floor" && !is_module_func("floor"))) {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string floored = fresh_reg();
            emit_line("  " + floored + " = call double @llvm.floor.f64(double " + double_val + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = fptosi double " + floored + " to i32");
            return result;
        }
        return "0";
    }

    // float_ceil(value: F64) -> I32 — LLVM @llvm.ceil.f64 + fptosi
    if (fn_name == "float_ceil" || (fn_name == "ceil" && !is_module_func("ceil"))) {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string ceiled = fresh_reg();
            emit_line("  " + ceiled + " = call double @llvm.ceil.f64(double " + double_val + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = fptosi double " + ceiled + " to i32");
            return result;
        }
        return "0";
    }

    // abs(value: I32) -> I32 — LLVM @llvm.fabs.f64 (convert to double, abs, convert back)
    if (fn_name == "float_abs" || (fn_name == "abs" && !is_module_func("abs"))) {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string double_result = fresh_reg();
            emit_line("  " + double_result + " = call double @llvm.fabs.f64(double " + double_val +
                      ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = fptosi double " + double_result + " to i32");
            return result;
        }
        return "0";
    }

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

    // ============ BIT MANIPULATION — inline LLVM bitcast ============

    // float32_bits(f: F32) -> U32 — LLVM bitcast
    if (fn_name == "float32_bits") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = bitcast float " + value + " to i32");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // float32_from_bits(b: U32) -> F32 — LLVM bitcast
    if (fn_name == "float32_from_bits") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = bitcast i32 " + value + " to float");
            last_expr_type_ = "float";
            return result;
        }
        return "0.0";
    }

    // float64_bits(f: F64) -> U64 — LLVM bitcast
    if (fn_name == "float64_bits") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = bitcast double " + value + " to i64");
            last_expr_type_ = "i64";
            return result;
        }
        return "0";
    }

    // float64_from_bits(b: U64) -> F64 — LLVM bitcast
    if (fn_name == "float64_from_bits") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = bitcast i64 " + value + " to double");
            last_expr_type_ = "double";
            return result;
        }
        return "0.0";
    }

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

    // ============ NEXTAFTER FUNCTIONS ============

    // nextafter(x: F64, y: F64) -> F64
    if (fn_name == "nextafter" && !is_module_func("nextafter")) {
        if (call.args.size() >= 2) {
            std::string x = gen_expr(*call.args[0]);
            std::string y = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @nextafter(double " + x + ", double " + y +
                      ")");
            last_expr_type_ = "double";
            return result;
        }
        return "0.0";
    }

    // nextafter32 — removed in Phase 34 (dead code, no TML callers)

    return std::nullopt;
}

} // namespace tml::codegen
