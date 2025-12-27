// LLVM IR generator - Math builtin functions
// Handles: float conversions, rounding, sqrt, pow, bit manipulation,
//          special float values, SIMD operations, black_box

#include "codegen/llvm_ir_gen.hpp"

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

    // ============ SIMD OPERATIONS ============

    // simd_sum_i32(arr: ptr, len: I64) -> I64
    if (fn_name == "simd_sum_i32") {
        if (call.args.size() >= 2) {
            std::string arr = gen_expr(*call.args[0]);
            std::string len = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @simd_sum_i32(ptr " + arr + ", i64 " + len +
                      ")");
            return result;
        }
        return "0";
    }

    // simd_sum_f64(arr: ptr, len: I64) -> F64
    if (fn_name == "simd_sum_f64") {
        if (call.args.size() >= 2) {
            std::string arr = gen_expr(*call.args[0]);
            std::string len = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @simd_sum_f64(ptr " + arr + ", i64 " + len +
                      ")");
            return result;
        }
        return "0.0";
    }

    // simd_dot_f64(a: ptr, b: ptr, len: I64) -> F64
    if (fn_name == "simd_dot_f64") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string len = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @simd_dot_f64(ptr " + a + ", ptr " + b +
                      ", i64 " + len + ")");
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

    // int_to_float(value: I32) -> F64
    if (fn_name == "int_to_float" || fn_name == "toFloat") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @int_to_float(i32 " + value + ")");
            return result;
        }
        return "0.0";
    }

    // float_to_int(value: F64) -> I32
    if (fn_name == "float_to_int" || fn_name == "toInt") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val;
            // Convert to double if needed
            if (last_expr_type_ == "i32" || last_expr_type_ == "i64") {
                double_val = fresh_reg();
                emit_line("  " + double_val + " = sitofp " + last_expr_type_ + " " + value +
                          " to double");
            } else {
                double_val = value; // Already a double
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @float_to_int(double " + double_val + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // float_round(value: F64) -> I32
    if (fn_name == "float_round" || (fn_name == "round" && !is_module_func("round"))) {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @float_round(double " + double_val + ")");
            return result;
        }
        return "0";
    }

    // float_floor(value: F64) -> I32
    if (fn_name == "float_floor" || (fn_name == "floor" && !is_module_func("floor"))) {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @float_floor(double " + double_val + ")");
            return result;
        }
        return "0";
    }

    // float_ceil(value: F64) -> I32
    if (fn_name == "float_ceil" || (fn_name == "ceil" && !is_module_func("ceil"))) {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @float_ceil(double " + double_val + ")");
            return result;
        }
        return "0";
    }

    // abs(value: I32) -> I32 (returns absolute value as int)
    if (fn_name == "float_abs" || (fn_name == "abs" && !is_module_func("abs"))) {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string double_result = fresh_reg();
            emit_line("  " + double_result + " = call double @float_abs(double " + double_val +
                      ")");
            // Convert back to i32
            std::string result = fresh_reg();
            emit_line("  " + result + " = fptosi double " + double_result + " to i32");
            return result;
        }
        return "0";
    }

    // sqrt(value: F64) -> F64 (returns double)
    if (fn_name == "float_sqrt" || fn_name == "sqrt") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val;
            // Convert to double if needed
            if (last_expr_type_ == "i32" || last_expr_type_ == "i64") {
                double_val = fresh_reg();
                emit_line("  " + double_val + " = sitofp " + last_expr_type_ + " " + value +
                          " to double");
            } else {
                double_val = value; // Already a double
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @float_sqrt(double " + double_val + ")");
            last_expr_type_ = "double";
            return result;
        }
        return "0.0";
    }

    // pow(base: F64, exp: I32) -> F64 (returns double)
    if (fn_name == "float_pow" || fn_name == "pow") {
        if (call.args.size() >= 2) {
            std::string base = gen_expr(*call.args[0]);
            std::string base_type = last_expr_type_;
            std::string exp = gen_expr(*call.args[1]);
            std::string double_base;
            // Convert base to double if needed
            if (base_type == "i32" || base_type == "i64") {
                double_base = fresh_reg();
                emit_line("  " + double_base + " = sitofp " + base_type + " " + base +
                          " to double");
            } else {
                double_base = base; // Already a double
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @float_pow(double " + double_base + ", i32 " +
                      exp + ")");
            last_expr_type_ = "double";
            return result;
        }
        return "1.0";
    }

    // ============ BIT MANIPULATION FUNCTIONS ============

    // float32_bits(f: F32) -> U32
    if (fn_name == "float32_bits") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @float32_bits(float " + value + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // float32_from_bits(b: U32) -> F32
    if (fn_name == "float32_from_bits") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call float @float32_from_bits(i32 " + value + ")");
            last_expr_type_ = "float";
            return result;
        }
        return "0.0";
    }

    // float64_bits(f: F64) -> U64
    if (fn_name == "float64_bits") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @float64_bits(double " + value + ")");
            last_expr_type_ = "i64";
            return result;
        }
        return "0";
    }

    // float64_from_bits(b: U64) -> F64
    if (fn_name == "float64_from_bits") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @float64_from_bits(i64 " + value + ")");
            last_expr_type_ = "double";
            return result;
        }
        return "0.0";
    }

    // ============ SPECIAL FLOAT VALUES ============

    // infinity(sign: I32) -> F64
    if (fn_name == "infinity") {
        if (!call.args.empty()) {
            std::string sign = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @infinity(i32 " + sign + ")");
            last_expr_type_ = "double";
            return result;
        }
        // Default to positive infinity
        std::string result = fresh_reg();
        emit_line("  " + result + " = call double @infinity(i32 1)");
        last_expr_type_ = "double";
        return result;
    }

    // nan() -> F64
    if (fn_name == "nan") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call double @nan()");
        last_expr_type_ = "double";
        return result;
    }

    // is_inf(f: F64, sign: I32) -> Bool
    if (fn_name == "is_inf") {
        if (call.args.size() >= 2) {
            std::string f = gen_expr(*call.args[0]);
            std::string sign = gen_expr(*call.args[1]);
            std::string int_result = fresh_reg();
            emit_line("  " + int_result + " = call i32 @is_inf(double " + f + ", i32 " + sign +
                      ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + int_result + ", 0");
            last_expr_type_ = "i1";
            return result;
        }
        return "0";
    }

    // is_nan(f: F64) -> Bool
    if (fn_name == "is_nan") {
        if (!call.args.empty()) {
            std::string f = gen_expr(*call.args[0]);
            std::string int_result = fresh_reg();
            emit_line("  " + int_result + " = call i32 @is_nan(double " + f + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = icmp ne i32 " + int_result + ", 0");
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

    // nextafter32(x: F32, y: F32) -> F32
    if (fn_name == "nextafter32" && !is_module_func("nextafter32")) {
        if (call.args.size() >= 2) {
            std::string x = gen_expr(*call.args[0]);
            std::string y = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call float @nextafter32(float " + x + ", float " + y +
                      ")");
            last_expr_type_ = "float";
            return result;
        }
        return "0.0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
