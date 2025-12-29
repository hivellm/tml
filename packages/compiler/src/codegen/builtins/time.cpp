// LLVM IR generator - Time builtin functions
// Handles: time_ms, time_us, time_ns, elapsed_*, sleep_*, Instant::*, Duration::*

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_time(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

    // time_ms() -> I32 - Current time in milliseconds
    if (fn_name == "time_ms") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @time_ms()");
        last_expr_type_ = "i32";
        return result;
    }

    // time_us() -> I64 - Current time in microseconds
    if (fn_name == "time_us") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i64 @time_us()");
        last_expr_type_ = "i64";
        return result;
    }

    // time_ns() -> I64 - Current time in nanoseconds
    if (fn_name == "time_ns") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i64 @time_ns()");
        last_expr_type_ = "i64";
        return result;
    }

    // elapsed_secs(start_ms: I32) -> Str - Elapsed time as "X.XXX" string
    if (fn_name == "elapsed_secs") {
        if (!call.args.empty()) {
            std::string start = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @elapsed_secs(i32 " + start + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        return "0";
    }

    // elapsed_ms(start_ms: I32) -> I32 - Elapsed milliseconds
    if (fn_name == "elapsed_ms") {
        if (!call.args.empty()) {
            std::string start = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @elapsed_ms(i32 " + start + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // elapsed_us(start_us: I64) -> I64 - Elapsed microseconds
    if (fn_name == "elapsed_us") {
        if (!call.args.empty()) {
            std::string start = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @elapsed_us(i64 " + start + ")");
            last_expr_type_ = "i64";
            return result;
        }
        return "0";
    }

    // elapsed_ns(start_ns: I64) -> I64 - Elapsed nanoseconds
    if (fn_name == "elapsed_ns") {
        if (!call.args.empty()) {
            std::string start = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @elapsed_ns(i64 " + start + ")");
            last_expr_type_ = "i64";
            return result;
        }
        return "0";
    }

    // sleep_ms(ms: I32) -> Unit - Sleep for milliseconds
    if (fn_name == "sleep_ms") {
        if (!call.args.empty()) {
            std::string ms = gen_expr(*call.args[0]);
            emit_line("  call void @sleep_ms(i32 " + ms + ")");
        }
        return "";
    }

    // sleep_us(us: I64) -> Unit - Sleep for microseconds
    if (fn_name == "sleep_us") {
        if (!call.args.empty()) {
            std::string us = gen_expr(*call.args[0]);
            emit_line("  call void @sleep_us(i64 " + us + ")");
        }
        return "";
    }

    // ============ INSTANT API (like Rust's std::time::Instant) ============

    // Instant::now() -> I64 - Get current instant
    if (fn_name == "Instant::now") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i64 @instant_now()");
        last_expr_type_ = "i64";
        return result;
    }

    // Instant::elapsed(start: I64) -> I64 - Get elapsed duration
    if (fn_name == "Instant::elapsed") {
        if (!call.args.empty()) {
            std::string start = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @instant_elapsed(i64 " + start + ")");
            last_expr_type_ = "i64";
            return result;
        }
        return "0";
    }

    // Duration::as_millis_f64(duration: I64) -> F64 - Get milliseconds as double
    if (fn_name == "Duration::as_millis_f64") {
        if (!call.args.empty()) {
            std::string duration = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @duration_as_millis_f64(i64 " + duration +
                      ")");
            last_expr_type_ = "double";
            return result;
        }
        return "0";
    }

    // Duration::as_secs_f64(duration: I64) -> Str - Format as "X.XXXXXX" seconds
    if (fn_name == "Duration::as_secs_f64") {
        if (!call.args.empty()) {
            std::string duration = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @duration_format_secs(i64 " + duration + ")");
            last_expr_type_ = "ptr";
            return result;
        }
        return "0";
    }

    return std::nullopt;
}

} // namespace tml::codegen
