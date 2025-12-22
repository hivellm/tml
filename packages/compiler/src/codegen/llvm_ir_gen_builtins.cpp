// LLVM IR generator - Builtin function calls
// Handles: print, memory ops, atomics, threading, channels, collections

#include "tml/codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::gen_call(const parser::CallExpr& call) -> std::string {
    // Get function name
    std::string fn_name;
    if (call.callee->is<parser::IdentExpr>()) {
        fn_name = call.callee->as<parser::IdentExpr>().name;
    } else if (call.callee->is<parser::PathExpr>()) {
        // Handle path expressions like Instant::now, Duration::as_millis_f64
        const auto& path = call.callee->as<parser::PathExpr>().path;
        // Join segments with ::
        for (size_t i = 0; i < path.segments.size(); ++i) {
            if (i > 0) fn_name += "::";
            fn_name += path.segments[i];
        }
    } else {
        report_error("Complex callee not supported", call.span);
        return "0";
    }

    // Handle builtin print/println - unified for all types
    if (fn_name == "print" || fn_name == "println") {
        bool with_newline = (fn_name == "println");

        if (call.args.empty()) {
            if (with_newline) {
                std::string result = fresh_reg();
                emit_line("  " + result + " = call i32 @putchar(i32 10)");
            }
            return "0";
        }

        // Check if first arg is a format string with {} placeholders
        bool is_format_string = false;
        std::string format_str;
        if (call.args[0]->is<parser::LiteralExpr>()) {
            const auto& lit = call.args[0]->as<parser::LiteralExpr>();
            if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                format_str = std::string(lit.token.string_value().value);
                if (format_str.find("{}") != std::string::npos && call.args.size() > 1) {
                    is_format_string = true;
                }
            }
        }

        if (is_format_string) {
            // Handle format string: "text {} more {}" with args
            return gen_format_print(format_str, call.args, 1, with_newline);
        }

        // Single value print - auto-detect type
        const auto& arg_expr = *call.args[0];
        std::string arg_val = gen_expr(arg_expr);

        // Try to infer type from expression
        auto arg_type = infer_print_type(arg_expr);

        // For identifiers, check if it's a known variable with type info
        if (arg_type == PrintArgType::Unknown && arg_expr.is<parser::IdentExpr>()) {
            const auto& ident = arg_expr.as<parser::IdentExpr>();
            auto it = locals_.find(ident.name);
            if (it != locals_.end()) {
                if (it->second.type == "i1") arg_type = PrintArgType::Bool;
                else if (it->second.type == "i32") arg_type = PrintArgType::Int;
                else if (it->second.type == "i64") arg_type = PrintArgType::I64;
                else if (it->second.type == "float" || it->second.type == "double") arg_type = PrintArgType::Float;
                else if (it->second.type == "ptr") arg_type = PrintArgType::Str;
            }
        }

        // For string constants (@.str.X), treat as string
        if (arg_val.starts_with("@.str.")) {
            arg_type = PrintArgType::Str;
        }

        std::string result = fresh_reg();

        switch (arg_type) {
            case PrintArgType::Str: {
                if (with_newline) {
                    emit_line("  " + result + " = call i32 @puts(ptr " + arg_val + ")");
                } else {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr " + arg_val + ")");
                }
                break;
            }
            case PrintArgType::Bool: {
                std::string label_true = fresh_label("print.true");
                std::string label_false = fresh_label("print.false");
                std::string label_end = fresh_label("print.end");

                emit_line("  br i1 " + arg_val + ", label %" + label_true + ", label %" + label_false);

                emit_line(label_true + ":");
                std::string r1 = fresh_reg();
                if (with_newline) {
                    emit_line("  " + r1 + " = call i32 @puts(ptr @.str.true)");
                } else {
                    emit_line("  " + r1 + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr @.str.true)");
                }
                emit_line("  br label %" + label_end);

                emit_line(label_false + ":");
                std::string r2 = fresh_reg();
                if (with_newline) {
                    emit_line("  " + r2 + " = call i32 @puts(ptr @.str.false)");
                } else {
                    emit_line("  " + r2 + " = call i32 (ptr, ...) @printf(ptr @.fmt.str.no_nl, ptr @.str.false)");
                }
                emit_line("  br label %" + label_end);

                emit_line(label_end + ":");
                block_terminated_ = false;
                return "0";
            }
            case PrintArgType::I64: {
                if (with_newline) {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.i64, i64 " + arg_val + ")");
                } else {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.i64.no_nl, i64 " + arg_val + ")");
                }
                break;
            }
            case PrintArgType::Float: {
                // For printf, floats are promoted to double - convert if needed
                std::string double_val = fresh_reg();
                emit_line("  " + double_val + " = fpext float " + arg_val + " to double");
                if (with_newline) {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.float, double " + double_val + ")");
                } else {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.float.no_nl, double " + double_val + ")");
                }
                break;
            }
            case PrintArgType::Int:
            case PrintArgType::Unknown:
            default: {
                if (with_newline) {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.int, i32 " + arg_val + ")");
                } else {
                    emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.int.no_nl, i32 " + arg_val + ")");
                }
                break;
            }
        }
        return result;
    }

    // Legacy support for print_i32, print_bool (deprecated but still works)
    if (fn_name == "print_i32") {
        if (!call.args.empty()) {
            std::string arg = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 (ptr, ...) @printf(ptr @.fmt.int, i32 " + arg + ")");
            return result;
        }
        return "0";
    }

    if (fn_name == "print_bool") {
        if (!call.args.empty()) {
            std::string arg = gen_expr(*call.args[0]);
            std::string label_true = fresh_label("bool.true");
            std::string label_false = fresh_label("bool.false");
            std::string label_end = fresh_label("bool.end");

            emit_line("  br i1 " + arg + ", label %" + label_true + ", label %" + label_false);

            emit_line(label_true + ":");
            std::string r1 = fresh_reg();
            emit_line("  " + r1 + " = call i32 @puts(ptr @.str.true)");
            emit_line("  br label %" + label_end);

            emit_line(label_false + ":");
            std::string r2 = fresh_reg();
            emit_line("  " + r2 + " = call i32 @puts(ptr @.str.false)");
            emit_line("  br label %" + label_end);

            emit_line(label_end + ":");
            block_terminated_ = false;
            return "0";
        }
        return "0";
    }

    // panic(msg: Str) -> Never
    // Prints error message to stderr and exits
    if (fn_name == "panic") {
        if (!call.args.empty()) {
            std::string msg = gen_expr(*call.args[0]);
            emit_line("  call void @tml_panic(ptr " + msg + ")");
            emit_line("  unreachable");
            block_terminated_ = true;
            return "0";
        }
        return "0";
    }

    // Memory allocation: alloc(size) -> ptr
    if (fn_name == "alloc") {
        if (!call.args.empty()) {
            std::string size = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            // Convert i32 size to i64 for malloc
            std::string size64 = fresh_reg();
            emit_line("  " + size64 + " = sext i32 " + size + " to i64");
            emit_line("  " + result + " = call ptr @malloc(i64 " + size64 + ")");
            return result;
        }
        return "null";
    }

    // Memory deallocation: dealloc(ptr)
    if (fn_name == "dealloc") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            emit_line("  call void @free(ptr " + ptr + ")");
        }
        return "0";
    }

    // Read from memory: read_i32(ptr) -> I32
    if (fn_name == "read_i32") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load i32, ptr " + ptr);
            return result;
        }
        return "0";
    }

    // Write to memory: write_i32(ptr, value)
    if (fn_name == "write_i32") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            emit_line("  store i32 " + val + ", ptr " + ptr);
        }
        return "0";
    }

    // Pointer offset: ptr_offset(ptr, offset) -> ptr
    if (fn_name == "ptr_offset") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string offset = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = getelementptr i32, ptr " + ptr + ", i32 " + offset);
            return result;
        }
        return "null";
    }

    // ============ ATOMIC OPERATIONS (Thread-Safe) ============

    // atomic_load(ptr) -> I32 - Thread-safe read
    if (fn_name == "atomic_load") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load atomic i32, ptr " + ptr + " seq_cst, align 4");
            return result;
        }
        return "0";
    }

    // atomic_store(ptr, value) - Thread-safe write
    if (fn_name == "atomic_store") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            emit_line("  store atomic i32 " + val + ", ptr " + ptr + " seq_cst, align 4");
        }
        return "0";
    }

    // atomic_add(ptr, value) -> I32 - Atomic fetch-and-add, returns old value
    if (fn_name == "atomic_add") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = atomicrmw add ptr " + ptr + ", i32 " + val + " seq_cst, align 4");
            return result;
        }
        return "0";
    }

    // atomic_sub(ptr, value) -> I32 - Atomic fetch-and-sub, returns old value
    if (fn_name == "atomic_sub") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = atomicrmw sub ptr " + ptr + ", i32 " + val + " seq_cst, align 4");
            return result;
        }
        return "0";
    }

    // atomic_exchange(ptr, value) -> I32 - Atomic exchange, returns old value
    if (fn_name == "atomic_exchange") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = atomicrmw xchg ptr " + ptr + ", i32 " + val + " seq_cst, align 4");
            return result;
        }
        return "0";
    }

    // atomic_cas(ptr, expected, desired) -> Bool - Compare-and-swap
    // Returns true if exchange happened (old value == expected)
    if (fn_name == "atomic_cas") {
        if (call.args.size() >= 3) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string expected = gen_expr(*call.args[1]);
            std::string desired = gen_expr(*call.args[2]);
            std::string cas_result = fresh_reg();
            std::string success = fresh_reg();
            emit_line("  " + cas_result + " = cmpxchg ptr " + ptr + ", i32 " + expected + ", i32 " + desired + " seq_cst seq_cst, align 4");
            emit_line("  " + success + " = extractvalue { i32, i1 } " + cas_result + ", 1");
            return success;
        }
        return "0";
    }

    // atomic_cas_val(ptr, expected, desired) -> I32 - CAS returning old value
    if (fn_name == "atomic_cas_val") {
        if (call.args.size() >= 3) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string expected = gen_expr(*call.args[1]);
            std::string desired = gen_expr(*call.args[2]);
            std::string cas_result = fresh_reg();
            std::string old_val = fresh_reg();
            emit_line("  " + cas_result + " = cmpxchg ptr " + ptr + ", i32 " + expected + ", i32 " + desired + " seq_cst seq_cst, align 4");
            emit_line("  " + old_val + " = extractvalue { i32, i1 } " + cas_result + ", 0");
            return old_val;
        }
        return "0";
    }

    // atomic_and(ptr, value) -> I32 - Atomic fetch-and-and
    if (fn_name == "atomic_and") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = atomicrmw and ptr " + ptr + ", i32 " + val + " seq_cst, align 4");
            return result;
        }
        return "0";
    }

    // atomic_or(ptr, value) -> I32 - Atomic fetch-and-or
    if (fn_name == "atomic_or") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = atomicrmw or ptr " + ptr + ", i32 " + val + " seq_cst, align 4");
            return result;
        }
        return "0";
    }

    // fence() - Memory barrier (full fence)
    if (fn_name == "fence") {
        emit_line("  fence seq_cst");
        return "0";
    }

    // fence_acquire() - Acquire fence
    if (fn_name == "fence_acquire") {
        emit_line("  fence acquire");
        return "0";
    }

    // fence_release() - Release fence
    if (fn_name == "fence_release") {
        emit_line("  fence release");
        return "0";
    }

    // ============ SPINLOCK PRIMITIVES ============

    // spin_lock(lock_ptr) - Acquire spinlock (spins until acquired)
    if (fn_name == "spin_lock") {
        if (!call.args.empty()) {
            std::string lock = gen_expr(*call.args[0]);
            std::string label_loop = fresh_label("spin.loop");
            std::string label_acquired = fresh_label("spin.acquired");

            emit_line("  br label %" + label_loop);
            emit_line(label_loop + ":");
            std::string old_val = fresh_reg();
            emit_line("  " + old_val + " = atomicrmw xchg ptr " + lock + ", i32 1 acquire, align 4");
            std::string was_free = fresh_reg();
            emit_line("  " + was_free + " = icmp eq i32 " + old_val + ", 0");
            emit_line("  br i1 " + was_free + ", label %" + label_acquired + ", label %" + label_loop);
            emit_line(label_acquired + ":");
            block_terminated_ = false;
        }
        return "0";
    }

    // spin_unlock(lock_ptr) - Release spinlock
    if (fn_name == "spin_unlock") {
        if (!call.args.empty()) {
            std::string lock = gen_expr(*call.args[0]);
            emit_line("  store atomic i32 0, ptr " + lock + " release, align 4");
        }
        return "0";
    }

    // spin_trylock(lock_ptr) -> Bool - Try to acquire, returns true if successful
    if (fn_name == "spin_trylock") {
        if (!call.args.empty()) {
            std::string lock = gen_expr(*call.args[0]);
            std::string old_val = fresh_reg();
            emit_line("  " + old_val + " = atomicrmw xchg ptr " + lock + ", i32 1 acquire, align 4");
            std::string success = fresh_reg();
            emit_line("  " + success + " = icmp eq i32 " + old_val + ", 0");
            return success;
        }
        return "0";
    }

    // ============ THREADING PRIMITIVES (via runtime) ============

    // thread_spawn(func_ptr, arg_ptr) -> thread_handle
    if (fn_name == "thread_spawn") {
        if (call.args.size() >= 2) {
            std::string func_ptr = gen_expr(*call.args[0]);
            std::string arg_ptr = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @tml_thread_spawn(ptr " + func_ptr + ", ptr " + arg_ptr + ")");
            return result;
        }
        return "null";
    }

    // thread_join(handle) -> Unit
    if (fn_name == "thread_join") {
        if (!call.args.empty()) {
            std::string handle = gen_expr(*call.args[0]);
            emit_line("  call void @tml_thread_join(ptr " + handle + ")");
        }
        return "0";
    }

    // thread_yield() -> Unit
    if (fn_name == "thread_yield") {
        emit_line("  call void @tml_thread_yield()");
        return "0";
    }

    // thread_sleep(ms: I32) -> Unit
    if (fn_name == "thread_sleep") {
        if (!call.args.empty()) {
            std::string ms = gen_expr(*call.args[0]);
            emit_line("  call void @tml_thread_sleep(i32 " + ms + ")");
        }
        return "0";
    }

    // thread_id() -> I32
    if (fn_name == "thread_id") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @tml_thread_id()");
        return result;
    }

    // ============ TIME FUNCTIONS (Benchmarking) ============

    // time_ms() -> I32 - Current time in milliseconds
    if (fn_name == "time_ms") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @tml_time_ms()");
        return result;
    }

    // time_us() -> I64 - Current time in microseconds
    if (fn_name == "time_us") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i64 @tml_time_us()");
        return result;
    }

    // time_ns() -> I64 - Current time in nanoseconds
    if (fn_name == "time_ns") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i64 @tml_time_ns()");
        return result;
    }

    // elapsed_secs(start_ms: I32) -> Str - Elapsed time as "X.XXX" string
    if (fn_name == "elapsed_secs") {
        if (!call.args.empty()) {
            std::string start = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @tml_elapsed_secs(i32 " + start + ")");
            return result;
        }
        return "0";
    }

    // elapsed_ms(start_ms: I32) -> I32 - Elapsed milliseconds
    if (fn_name == "elapsed_ms") {
        if (!call.args.empty()) {
            std::string start = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_elapsed_ms(i32 " + start + ")");
            return result;
        }
        return "0";
    }

    // ============ INSTANT API (like Rust's std::time::Instant) ============

    // Instant::now() -> I64 - Get current instant
    if (fn_name == "Instant::now") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i64 @tml_instant_now()");
        last_expr_type_ = "i64";
        return result;
    }

    // Instant::elapsed(start: I64) -> I64 - Get elapsed duration
    if (fn_name == "Instant::elapsed") {
        if (!call.args.empty()) {
            std::string start = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i64 @tml_instant_elapsed(i64 " + start + ")");
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
            emit_line("  " + result + " = call double @tml_duration_as_millis_f64(i64 " + duration + ")");
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
            emit_line("  " + result + " = call ptr @tml_duration_format_secs(i64 " + duration + ")");
            return result;
        }
        return "0";
    }

    // ============ BLACK BOX (prevent optimization) ============

    // black_box(value: I32) -> I32 - Prevent LLVM from optimizing away
    if (fn_name == "black_box") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_black_box_i32(i32 " + value + ")");
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
            emit_line("  " + result + " = call i64 @tml_black_box_i64(i64 " + value + ")");
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
            emit_line("  " + result + " = call i64 @tml_simd_sum_i32(ptr " + arr + ", i64 " + len + ")");
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
            emit_line("  " + result + " = call double @tml_simd_sum_f64(ptr " + arr + ", i64 " + len + ")");
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
            emit_line("  " + result + " = call double @tml_simd_dot_f64(ptr " + a + ", ptr " + b + ", i64 " + len + ")");
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
            emit_line("  " + result + " = call ptr @tml_float_to_fixed(double " + double_val + ", i32 " + decimals + ")");
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
            emit_line("  " + result + " = call ptr @tml_float_to_precision(double " + double_val + ", i32 " + precision + ")");
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
            emit_line("  " + result + " = call ptr @tml_float_to_string(double " + double_val + ")");
            return result;
        }
        return "0";
    }

    // int_to_float(value: I32) -> F64
    if (fn_name == "int_to_float" || fn_name == "toFloat") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @tml_int_to_float(i32 " + value + ")");
            return result;
        }
        return "0.0";
    }

    // float_to_int(value: F64) -> I32
    if (fn_name == "float_to_int" || fn_name == "toInt") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_float_to_int(double " + double_val + ")");
            return result;
        }
        return "0";
    }

    // float_round(value: F64) -> I32
    if (fn_name == "float_round" || fn_name == "round") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_float_round(double " + double_val + ")");
            return result;
        }
        return "0";
    }

    // float_floor(value: F64) -> I32
    if (fn_name == "float_floor" || fn_name == "floor") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_float_floor(double " + double_val + ")");
            return result;
        }
        return "0";
    }

    // float_ceil(value: F64) -> I32
    if (fn_name == "float_ceil" || fn_name == "ceil") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_float_ceil(double " + double_val + ")");
            return result;
        }
        return "0";
    }

    // abs(value: I32) -> I32 (returns absolute value as int)
    if (fn_name == "float_abs" || fn_name == "abs") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string double_result = fresh_reg();
            emit_line("  " + double_result + " = call double @tml_float_abs(double " + double_val + ")");
            // Convert back to i32
            std::string result = fresh_reg();
            emit_line("  " + result + " = fptosi double " + double_result + " to i32");
            return result;
        }
        return "0";
    }

    // sqrt(value: I32) -> F64 (returns double)
    if (fn_name == "float_sqrt" || fn_name == "sqrt") {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string double_result = fresh_reg();
            emit_line("  " + double_result + " = call double @tml_float_sqrt(double " + double_val + ")");
            // Convert to i32 since type checker says I32 for now
            // TODO: Add proper F64 type support
            std::string result = fresh_reg();
            emit_line("  " + result + " = fptosi double " + double_result + " to i32");
            return result;
        }
        return "0";
    }

    // pow(base: I32, exp: I32) -> F64 (returns double)
    if (fn_name == "float_pow" || fn_name == "pow") {
        if (call.args.size() >= 2) {
            std::string base = gen_expr(*call.args[0]);
            std::string exp = gen_expr(*call.args[1]);
            std::string double_base = fresh_reg();
            emit_line("  " + double_base + " = sitofp i32 " + base + " to double");
            std::string double_result = fresh_reg();
            emit_line("  " + double_result + " = call double @tml_float_pow(double " + double_base + ", i32 " + exp + ")");
            // Convert to i32 since type checker says I32 for now
            std::string result = fresh_reg();
            emit_line("  " + result + " = fptosi double " + double_result + " to i32");
            return result;
        }
        return "1";
    }

    // ============ CHANNEL PRIMITIVES (Go-style) ============

    // channel_create() -> channel_ptr
    if (fn_name == "channel_create") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @tml_channel_create()");
        return result;
    }

    // channel_send(ch, value: I32) -> Bool
    if (fn_name == "channel_send") {
        if (call.args.size() >= 2) {
            std::string ch = gen_expr(*call.args[0]);
            std::string value = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_channel_send(ptr " + ch + ", i32 " + value + ")");
            // Convert i32 to i1
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            return bool_result;
        }
        return "0";
    }

    // channel_recv(ch) -> I32
    if (fn_name == "channel_recv") {
        if (!call.args.empty()) {
            std::string ch = gen_expr(*call.args[0]);
            // Allocate temp for output value
            std::string out_ptr = fresh_reg();
            emit_line("  " + out_ptr + " = alloca i32, align 4");
            emit_line("  call i32 @tml_channel_recv(ptr " + ch + ", ptr " + out_ptr + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = load i32, ptr " + out_ptr);
            return result;
        }
        return "0";
    }

    // channel_try_send(ch, value: I32) -> Bool
    if (fn_name == "channel_try_send") {
        if (call.args.size() >= 2) {
            std::string ch = gen_expr(*call.args[0]);
            std::string value = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_channel_try_send(ptr " + ch + ", i32 " + value + ")");
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            return bool_result;
        }
        return "0";
    }

    // channel_try_recv(ch, out_ptr) -> Bool
    if (fn_name == "channel_try_recv") {
        if (call.args.size() >= 2) {
            std::string ch = gen_expr(*call.args[0]);
            std::string out_ptr = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_channel_try_recv(ptr " + ch + ", ptr " + out_ptr + ")");
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            return bool_result;
        }
        return "0";
    }

    // channel_close(ch) -> Unit
    if (fn_name == "channel_close") {
        if (!call.args.empty()) {
            std::string ch = gen_expr(*call.args[0]);
            emit_line("  call void @tml_channel_close(ptr " + ch + ")");
        }
        return "0";
    }

    // channel_destroy(ch) -> Unit
    if (fn_name == "channel_destroy") {
        if (!call.args.empty()) {
            std::string ch = gen_expr(*call.args[0]);
            emit_line("  call void @tml_channel_destroy(ptr " + ch + ")");
        }
        return "0";
    }

    // channel_len(ch) -> I32
    if (fn_name == "channel_len") {
        if (!call.args.empty()) {
            std::string ch = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_channel_len(ptr " + ch + ")");
            return result;
        }
        return "0";
    }

    // ============ MUTEX PRIMITIVES ============

    // mutex_create() -> mutex_ptr
    if (fn_name == "mutex_create") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @tml_mutex_create()");
        return result;
    }

    // mutex_lock(m) -> Unit
    if (fn_name == "mutex_lock") {
        if (!call.args.empty()) {
            std::string m = gen_expr(*call.args[0]);
            emit_line("  call void @tml_mutex_lock(ptr " + m + ")");
        }
        return "0";
    }

    // mutex_unlock(m) -> Unit
    if (fn_name == "mutex_unlock") {
        if (!call.args.empty()) {
            std::string m = gen_expr(*call.args[0]);
            emit_line("  call void @tml_mutex_unlock(ptr " + m + ")");
        }
        return "0";
    }

    // mutex_try_lock(m) -> Bool
    if (fn_name == "mutex_try_lock") {
        if (!call.args.empty()) {
            std::string m = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_mutex_try_lock(ptr " + m + ")");
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            return bool_result;
        }
        return "0";
    }

    // mutex_destroy(m) -> Unit
    if (fn_name == "mutex_destroy") {
        if (!call.args.empty()) {
            std::string m = gen_expr(*call.args[0]);
            emit_line("  call void @tml_mutex_destroy(ptr " + m + ")");
        }
        return "0";
    }

    // ============ WAITGROUP PRIMITIVES (Go-style) ============

    // waitgroup_create() -> wg_ptr
    if (fn_name == "waitgroup_create") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @tml_waitgroup_create()");
        return result;
    }

    // waitgroup_add(wg, delta: I32) -> Unit
    if (fn_name == "waitgroup_add") {
        if (call.args.size() >= 2) {
            std::string wg = gen_expr(*call.args[0]);
            std::string delta = gen_expr(*call.args[1]);
            emit_line("  call void @tml_waitgroup_add(ptr " + wg + ", i32 " + delta + ")");
        }
        return "0";
    }

    // waitgroup_done(wg) -> Unit
    if (fn_name == "waitgroup_done") {
        if (!call.args.empty()) {
            std::string wg = gen_expr(*call.args[0]);
            emit_line("  call void @tml_waitgroup_done(ptr " + wg + ")");
        }
        return "0";
    }

    // waitgroup_wait(wg) -> Unit
    if (fn_name == "waitgroup_wait") {
        if (!call.args.empty()) {
            std::string wg = gen_expr(*call.args[0]);
            emit_line("  call void @tml_waitgroup_wait(ptr " + wg + ")");
        }
        return "0";
    }

    // waitgroup_destroy(wg) -> Unit
    if (fn_name == "waitgroup_destroy") {
        if (!call.args.empty()) {
            std::string wg = gen_expr(*call.args[0]);
            emit_line("  call void @tml_waitgroup_destroy(ptr " + wg + ")");
        }
        return "0";
    }

    // ============ LIST FUNCTIONS ============

    // list_create(capacity) -> list_ptr
    if (fn_name == "list_create") {
        std::string cap = call.args.empty() ? "4" : gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @tml_list_create(i32 " + cap + ")");
        return result;
    }

    // list_destroy(list) -> Unit
    if (fn_name == "list_destroy") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            emit_line("  call void @tml_list_destroy(ptr " + list + ")");
        }
        return "0";
    }

    // list_push(list, value) -> Unit
    if (fn_name == "list_push") {
        if (call.args.size() >= 2) {
            std::string list = gen_expr(*call.args[0]);
            std::string value = gen_expr(*call.args[1]);
            emit_line("  call void @tml_list_push(ptr " + list + ", i32 " + value + ")");
        }
        return "0";
    }

    // list_pop(list) -> I32
    if (fn_name == "list_pop") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_list_pop(ptr " + list + ")");
            return result;
        }
        return "0";
    }

    // list_get(list, index) -> I32
    if (fn_name == "list_get") {
        if (call.args.size() >= 2) {
            std::string list = gen_expr(*call.args[0]);
            std::string index = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_list_get(ptr " + list + ", i32 " + index + ")");
            return result;
        }
        return "0";
    }

    // list_set(list, index, value) -> Unit
    if (fn_name == "list_set") {
        if (call.args.size() >= 3) {
            std::string list = gen_expr(*call.args[0]);
            std::string index = gen_expr(*call.args[1]);
            std::string value = gen_expr(*call.args[2]);
            emit_line("  call void @tml_list_set(ptr " + list + ", i32 " + index + ", i32 " + value + ")");
        }
        return "0";
    }

    // list_len(list) -> I32
    if (fn_name == "list_len") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_list_len(ptr " + list + ")");
            return result;
        }
        return "0";
    }

    // list_capacity(list) -> I32
    if (fn_name == "list_capacity") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_list_capacity(ptr " + list + ")");
            return result;
        }
        return "0";
    }

    // list_clear(list) -> Unit
    if (fn_name == "list_clear") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            emit_line("  call void @tml_list_clear(ptr " + list + ")");
        }
        return "0";
    }

    // list_is_empty(list) -> Bool
    if (fn_name == "list_is_empty") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_list_is_empty(ptr " + list + ")");
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            return bool_result;
        }
        return "0";
    }

    // ============ HASHMAP FUNCTIONS ============

    // hashmap_create() -> map_ptr
    if (fn_name == "hashmap_create") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @tml_hashmap_create()");
        return result;
    }

    // hashmap_destroy(map) -> Unit
    if (fn_name == "hashmap_destroy") {
        if (!call.args.empty()) {
            std::string map = gen_expr(*call.args[0]);
            emit_line("  call void @tml_hashmap_destroy(ptr " + map + ")");
        }
        return "0";
    }

    // hashmap_set(map, key, value) -> Unit
    if (fn_name == "hashmap_set") {
        if (call.args.size() >= 3) {
            std::string map = gen_expr(*call.args[0]);
            std::string key = gen_expr(*call.args[1]);
            std::string value = gen_expr(*call.args[2]);
            emit_line("  call void @tml_hashmap_set(ptr " + map + ", i32 " + key + ", i32 " + value + ")");
        }
        return "0";
    }

    // hashmap_get(map, key) -> I32
    if (fn_name == "hashmap_get") {
        if (call.args.size() >= 2) {
            std::string map = gen_expr(*call.args[0]);
            std::string key = gen_expr(*call.args[1]);
            std::string out_ptr = fresh_reg();
            emit_line("  " + out_ptr + " = alloca i32, align 4");
            emit_line("  call i32 @tml_hashmap_get(ptr " + map + ", i32 " + key + ", ptr " + out_ptr + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = load i32, ptr " + out_ptr);
            return result;
        }
        return "0";
    }

    // hashmap_has(map, key) -> Bool
    if (fn_name == "hashmap_has") {
        if (call.args.size() >= 2) {
            std::string map = gen_expr(*call.args[0]);
            std::string key = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_hashmap_has(ptr " + map + ", i32 " + key + ")");
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            return bool_result;
        }
        return "0";
    }

    // hashmap_remove(map, key) -> Bool
    if (fn_name == "hashmap_remove") {
        if (call.args.size() >= 2) {
            std::string map = gen_expr(*call.args[0]);
            std::string key = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_hashmap_remove(ptr " + map + ", i32 " + key + ")");
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            return bool_result;
        }
        return "0";
    }

    // hashmap_len(map) -> I32
    if (fn_name == "hashmap_len") {
        if (!call.args.empty()) {
            std::string map = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_hashmap_len(ptr " + map + ")");
            return result;
        }
        return "0";
    }

    // hashmap_clear(map) -> Unit
    if (fn_name == "hashmap_clear") {
        if (!call.args.empty()) {
            std::string map = gen_expr(*call.args[0]);
            emit_line("  call void @tml_hashmap_clear(ptr " + map + ")");
        }
        return "0";
    }

    // ============ BUFFER FUNCTIONS ============

    // buffer_create(capacity) -> buf_ptr
    if (fn_name == "buffer_create") {
        std::string cap = call.args.empty() ? "16" : gen_expr(*call.args[0]);
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @tml_buffer_create(i32 " + cap + ")");
        return result;
    }

    // buffer_destroy(buf) -> Unit
    if (fn_name == "buffer_destroy") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            emit_line("  call void @tml_buffer_destroy(ptr " + buf + ")");
        }
        return "0";
    }

    // buffer_write_byte(buf, byte) -> Unit
    if (fn_name == "buffer_write_byte") {
        if (call.args.size() >= 2) {
            std::string buf = gen_expr(*call.args[0]);
            std::string byte = gen_expr(*call.args[1]);
            emit_line("  call void @tml_buffer_write_byte(ptr " + buf + ", i32 " + byte + ")");
        }
        return "0";
    }

    // buffer_write_i32(buf, value) -> Unit
    if (fn_name == "buffer_write_i32") {
        if (call.args.size() >= 2) {
            std::string buf = gen_expr(*call.args[0]);
            std::string value = gen_expr(*call.args[1]);
            emit_line("  call void @tml_buffer_write_i32(ptr " + buf + ", i32 " + value + ")");
        }
        return "0";
    }

    // buffer_read_byte(buf) -> I32
    if (fn_name == "buffer_read_byte") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_buffer_read_byte(ptr " + buf + ")");
            return result;
        }
        return "0";
    }

    // buffer_read_i32(buf) -> I32
    if (fn_name == "buffer_read_i32") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_buffer_read_i32(ptr " + buf + ")");
            return result;
        }
        return "0";
    }

    // buffer_len(buf) -> I32
    if (fn_name == "buffer_len") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_buffer_len(ptr " + buf + ")");
            return result;
        }
        return "0";
    }

    // buffer_capacity(buf) -> I32
    if (fn_name == "buffer_capacity") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_buffer_capacity(ptr " + buf + ")");
            return result;
        }
        return "0";
    }

    // buffer_remaining(buf) -> I32
    if (fn_name == "buffer_remaining") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_buffer_remaining(ptr " + buf + ")");
            return result;
        }
        return "0";
    }

    // buffer_clear(buf) -> Unit
    if (fn_name == "buffer_clear") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            emit_line("  call void @tml_buffer_clear(ptr " + buf + ")");
        }
        return "0";
    }

    // buffer_reset_read(buf) -> Unit
    if (fn_name == "buffer_reset_read") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            emit_line("  call void @tml_buffer_reset_read(ptr " + buf + ")");
        }
        return "0";
    }

    // ============ STRING UTILITIES ============

    // str_len(s) -> I32
    if (fn_name == "str_len") {
        if (!call.args.empty()) {
            std::string s = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_str_len(ptr " + s + ")");
            return result;
        }
        return "0";
    }

    // str_hash(s) -> I32
    if (fn_name == "str_hash") {
        if (!call.args.empty()) {
            std::string s = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @tml_str_hash(ptr " + s + ")");
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
            emit_line("  " + result + " = call i32 @tml_str_eq(ptr " + a + ", ptr " + b + ")");
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            return bool_result;
        }
        return "0";
    }

    // ============ ENUM CONSTRUCTORS ============

    // Check if this is an enum constructor
    if (call.callee->is<parser::IdentExpr>()) {
        const auto& ident = call.callee->as<parser::IdentExpr>();

        for (const auto& [enum_name, enum_def] : env_.all_enums()) {
            for (size_t variant_idx = 0; variant_idx < enum_def.variants.size(); ++variant_idx) {
                const auto& [variant_name, payload_types] = enum_def.variants[variant_idx];

                if (variant_name == ident.name) {
                    // Found enum constructor
                    std::string enum_type = "%struct." + enum_name;
                    std::string result = fresh_reg();
                    std::string enum_val = fresh_reg();

                    // Create enum value on stack
                    emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                    // Set tag (field 0)
                    std::string tag_ptr = fresh_reg();
                    emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " + enum_val + ", i32 0, i32 0");
                    emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " + tag_ptr);

                    // Set payload if present (stored in field 1, the [N x i8] array)
                    if (!payload_types.empty() && !call.args.empty()) {
                        std::string payload = gen_expr(*call.args[0]);

                        // Get pointer to payload field ([N x i8])
                        std::string payload_ptr = fresh_reg();
                        emit_line("  " + payload_ptr + " = getelementptr inbounds " + enum_type + ", ptr " + enum_val + ", i32 0, i32 1");

                        // Cast payload to bytes and store
                        // For simplicity, bitcast the i8 array pointer to the payload type pointer
                        std::string payload_typed_ptr = fresh_reg();
                        emit_line("  " + payload_typed_ptr + " = bitcast ptr " + payload_ptr + " to ptr");
                        emit_line("  store " + last_expr_type_ + " " + payload + ", ptr " + payload_typed_ptr);
                    }

                    // Load the complete enum value
                    emit_line("  " + result + " = load " + enum_type + ", ptr " + enum_val);
                    last_expr_type_ = enum_type;
                    return result;
                }
            }
        }
    }

    // Check if this is an indirect call through a function pointer variable
    auto local_it = locals_.find(fn_name);
    if (local_it != locals_.end() && local_it->second.type == "ptr") {
        // This is a function pointer variable - generate indirect call
        // Load the function pointer from the alloca
        std::string fn_ptr = fresh_reg();
        emit_line("  " + fn_ptr + " = load ptr, ptr " + local_it->second.reg);

        // Generate arguments
        std::vector<std::pair<std::string, std::string>> arg_vals;
        for (size_t i = 0; i < call.args.size(); ++i) {
            std::string val = gen_expr(*call.args[i]);
            arg_vals.push_back({val, last_expr_type_});
        }

        // Generate indirect call - assume i32 return type for now
        std::string ret_type = "i32";
        std::string result = fresh_reg();
        emit("  " + result + " = call " + ret_type + " " + fn_ptr + "(");
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0) emit(", ");
            emit(arg_vals[i].second + " " + arg_vals[i].first);
        }
        emit_line(")");
        last_expr_type_ = ret_type;
        return result;
    }

    // User-defined function - look up signature from type environment
    std::string mangled = "@tml_" + fn_name;

    // Try to look up the function signature
    auto func_sig = env_.lookup_func(fn_name);

    // Determine return type
    std::string ret_type = "i32";  // Default
    if (func_sig.has_value()) {
        ret_type = llvm_type_from_semantic(func_sig->return_type);
    }

    // Generate arguments with type inference
    std::vector<std::pair<std::string, std::string>> arg_vals; // (value, type)
    for (size_t i = 0; i < call.args.size(); ++i) {
        std::string val = gen_expr(*call.args[i]);
        std::string type = "i32";  // Default

        // If we have function signature, use parameter type
        if (func_sig.has_value() && i < func_sig->params.size()) {
            type = llvm_type_from_semantic(func_sig->params[i]);
        } else {
            // Fallback to inference
            // Check if it's a string constant
            if (val.starts_with("@.str.")) {
                type = "ptr";
            } else if (call.args[i]->is<parser::LiteralExpr>()) {
                const auto& lit = call.args[i]->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                    type = "ptr";
                } else if (lit.token.kind == lexer::TokenKind::BoolLiteral) {
                    type = "i1";
                }
            }
        }
        arg_vals.push_back({val, type});
    }

    // Call - handle void vs non-void return types
    if (ret_type == "void") {
        emit("  call void " + mangled + "(");
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0) emit(", ");
            emit(arg_vals[i].second + " " + arg_vals[i].first);
        }
        emit_line(")");
        last_expr_type_ = "void";
        return "0";
    } else {
        std::string result = fresh_reg();
        emit("  " + result + " = call " + ret_type + " " + mangled + "(");
        for (size_t i = 0; i < arg_vals.size(); ++i) {
            if (i > 0) emit(", ");
            emit(arg_vals[i].second + " " + arg_vals[i].first);
        }
        emit_line(")");
        last_expr_type_ = ret_type;
        return result;
    }
}

} // namespace tml::codegen
