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

    // panic(msg: Str) -> Never
    // Prints error message to stderr and exits
    if (fn_name == "panic") {
        if (!call.args.empty()) {
            std::string msg = gen_expr(*call.args[0]);
            emit_line("  call void @panic(ptr " + msg + ")");
            emit_line("  unreachable");
            block_terminated_ = true;
            return "0";
        }
        return "0";
    }

    // Memory allocation: alloc(size) -> ptr
    // Only use builtin if not imported from core::mem module
    if (fn_name == "alloc" && !env_.lookup_func("alloc").has_value()) {
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
    if (fn_name == "dealloc" && !env_.lookup_func("dealloc").has_value()) {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            emit_line("  call void @free(ptr " + ptr + ")");
        }
        return "0";
    }

    // ============ MEM_* FUNCTIONS (matches runtime/mem.c) ============

    // mem_alloc(size: I64) -> *Unit
    if (fn_name == "mem_alloc") {
        if (!call.args.empty()) {
            std::string size = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @mem_alloc(i64 " + size + ")");
            return result;
        }
        return "null";
    }

    // mem_alloc_zeroed(size: I64) -> *Unit
    if (fn_name == "mem_alloc_zeroed") {
        if (!call.args.empty()) {
            std::string size = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @mem_alloc_zeroed(i64 " + size + ")");
            return result;
        }
        return "null";
    }

    // mem_realloc(ptr: *Unit, new_size: I64) -> *Unit
    if (fn_name == "mem_realloc") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string size = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @mem_realloc(ptr " + ptr + ", i64 " + size + ")");
            return result;
        }
        return "null";
    }

    // mem_free(ptr: *Unit) -> Unit
    if (fn_name == "mem_free") {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            emit_line("  call void @mem_free(ptr " + ptr + ")");
        }
        return "";
    }

    // mem_copy(dest: *Unit, src: *Unit, size: I64) -> Unit
    if (fn_name == "mem_copy") {
        if (call.args.size() >= 3) {
            std::string dest = gen_expr(*call.args[0]);
            std::string src = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            emit_line("  call void @mem_copy(ptr " + dest + ", ptr " + src + ", i64 " + size + ")");
        }
        return "";
    }

    // mem_move(dest: *Unit, src: *Unit, size: I64) -> Unit
    if (fn_name == "mem_move") {
        if (call.args.size() >= 3) {
            std::string dest = gen_expr(*call.args[0]);
            std::string src = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            emit_line("  call void @mem_move(ptr " + dest + ", ptr " + src + ", i64 " + size + ")");
        }
        return "";
    }

    // mem_set(ptr: *Unit, value: I32, size: I64) -> Unit
    if (fn_name == "mem_set") {
        if (call.args.size() >= 3) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            emit_line("  call void @mem_set(ptr " + ptr + ", i32 " + val + ", i64 " + size + ")");
        }
        return "";
    }

    // mem_zero(ptr: *Unit, size: I64) -> Unit
    if (fn_name == "mem_zero") {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string size = gen_expr(*call.args[1]);
            emit_line("  call void @mem_zero(ptr " + ptr + ", i64 " + size + ")");
        }
        return "";
    }

    // mem_compare(a: *Unit, b: *Unit, size: I64) -> I32
    if (fn_name == "mem_compare") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @mem_compare(ptr " + a + ", ptr " + b + ", i64 " + size + ")");
            return result;
        }
        return "0";
    }

    // mem_eq(a: *Unit, b: *Unit, size: I64) -> Bool (I32)
    if (fn_name == "mem_eq") {
        if (call.args.size() >= 3) {
            std::string a = gen_expr(*call.args[0]);
            std::string b = gen_expr(*call.args[1]);
            std::string size = gen_expr(*call.args[2]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @mem_eq(ptr " + a + ", ptr " + b + ", i64 " + size + ")");
            return result;
        }
        return "0";
    }

    // Read from memory: read_i32(ptr) -> I32
    if (fn_name == "read_i32" && !env_.lookup_func("read_i32").has_value()) {
        if (!call.args.empty()) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = load i32, ptr " + ptr);
            return result;
        }
        return "0";
    }

    // Write to memory: write_i32(ptr, value)
    if (fn_name == "write_i32" && !env_.lookup_func("write_i32").has_value()) {
        if (call.args.size() >= 2) {
            std::string ptr = gen_expr(*call.args[0]);
            std::string val = gen_expr(*call.args[1]);
            emit_line("  store i32 " + val + ", ptr " + ptr);
        }
        return "0";
    }

    // Pointer offset: ptr_offset(ptr, offset) -> ptr
    if (fn_name == "ptr_offset" && !env_.lookup_func("ptr_offset").has_value()) {
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
            emit_line("  " + result + " = call ptr @thread_spawn(ptr " + func_ptr + ", ptr " + arg_ptr + ")");
            return result;
        }
        return "null";
    }

    // thread_join(handle) -> Unit
    if (fn_name == "thread_join") {
        if (!call.args.empty()) {
            std::string handle = gen_expr(*call.args[0]);
            emit_line("  call void @thread_join(ptr " + handle + ")");
        }
        return "0";
    }

    // thread_yield() -> Unit
    if (fn_name == "thread_yield") {
        emit_line("  call void @thread_yield()");
        return "0";
    }

    // thread_sleep(ms: I32) -> Unit
    if (fn_name == "thread_sleep") {
        if (!call.args.empty()) {
            std::string ms = gen_expr(*call.args[0]);
            emit_line("  call void @thread_sleep(i32 " + ms + ")");
        }
        return "0";
    }

    // thread_id() -> I32
    if (fn_name == "thread_id") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @thread_id()");
        return result;
    }

    // ============ TIME FUNCTIONS (Benchmarking) ============

    // time_ms() -> I32 - Current time in milliseconds
    if (fn_name == "time_ms") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i32 @time_ms()");
        return result;
    }

    // time_us() -> I64 - Current time in microseconds
    if (fn_name == "time_us") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i64 @time_us()");
        return result;
    }

    // time_ns() -> I64 - Current time in nanoseconds
    if (fn_name == "time_ns") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call i64 @time_ns()");
        return result;
    }

    // elapsed_secs(start_ms: I32) -> Str - Elapsed time as "X.XXX" string
    if (fn_name == "elapsed_secs") {
        if (!call.args.empty()) {
            std::string start = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @elapsed_secs(i32 " + start + ")");
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
            emit_line("  " + result + " = call double @duration_as_millis_f64(i64 " + duration + ")");
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
            emit_line("  " + result + " = call i64 @simd_sum_i32(ptr " + arr + ", i64 " + len + ")");
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
            emit_line("  " + result + " = call double @simd_sum_f64(ptr " + arr + ", i64 " + len + ")");
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
            emit_line("  " + result + " = call double @simd_dot_f64(ptr " + a + ", ptr " + b + ", i64 " + len + ")");
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
            emit_line("  " + result + " = call ptr @float_to_fixed(double " + double_val + ", i32 " + decimals + ")");
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
            emit_line("  " + result + " = call ptr @float_to_precision(double " + double_val + ", i32 " + precision + ")");
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
                emit_line("  " + double_val + " = sitofp " + last_expr_type_ + " " + value + " to double");
            } else {
                double_val = value;  // Already a double
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @float_to_int(double " + double_val + ")");
            last_expr_type_ = "i32";
            return result;
        }
        return "0";
    }

    // Helper: check if function is defined as a TML module function (not a builtin)
    // If so, skip the builtin handler and use the TML implementation
    auto is_module_func = [&](const std::string& name) -> bool {
        // Check if function is imported or defined in the type environment
        return env_.lookup_func(name).has_value();
    };

    // float_round(value: F64) -> I32
    // Only use builtin if not imported from a module
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
    // Only use float_abs builtin if not imported from a module
    if (fn_name == "float_abs" || (fn_name == "abs" && !is_module_func("abs"))) {
        if (!call.args.empty()) {
            std::string value = gen_expr(*call.args[0]);
            std::string double_val = fresh_reg();
            emit_line("  " + double_val + " = sitofp i32 " + value + " to double");
            std::string double_result = fresh_reg();
            emit_line("  " + double_result + " = call double @float_abs(double " + double_val + ")");
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
                emit_line("  " + double_val + " = sitofp " + last_expr_type_ + " " + value + " to double");
            } else {
                double_val = value;  // Already a double
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
                emit_line("  " + double_base + " = sitofp " + base_type + " " + base + " to double");
            } else {
                double_base = base;  // Already a double
            }
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @float_pow(double " + double_base + ", i32 " + exp + ")");
            last_expr_type_ = "double";
            return result;
        }
        return "1.0";
    }

    // ============ BIT MANIPULATION FUNCTIONS ============
    // These are always builtins (no TML module versions)

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
    // These are always builtins (no TML module versions)

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
            emit_line("  " + int_result + " = call i32 @is_inf(double " + f + ", i32 " + sign + ")");
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
    // Only use builtin if not imported from a module

    // nextafter(x: F64, y: F64) -> F64
    if (fn_name == "nextafter" && !is_module_func("nextafter")) {
        if (call.args.size() >= 2) {
            std::string x = gen_expr(*call.args[0]);
            std::string y = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call double @nextafter(double " + x + ", double " + y + ")");
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
            emit_line("  " + result + " = call float @nextafter32(float " + x + ", float " + y + ")");
            last_expr_type_ = "float";
            return result;
        }
        return "0.0";
    }

    // ============ CHANNEL PRIMITIVES (Go-style) ============

    // channel_create() -> channel_ptr
    if (fn_name == "channel_create") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @channel_create()");
        return result;
    }

    // channel_send(ch, value: I32) -> Bool
    if (fn_name == "channel_send") {
        if (call.args.size() >= 2) {
            std::string ch = gen_expr(*call.args[0]);
            std::string value = gen_expr(*call.args[1]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @channel_send(ptr " + ch + ", i32 " + value + ")");
            // Convert i32 to i1
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            last_expr_type_ = "i1";
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
            emit_line("  call i32 @channel_recv(ptr " + ch + ", ptr " + out_ptr + ")");
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
            emit_line("  " + result + " = call i32 @channel_try_send(ptr " + ch + ", i32 " + value + ")");
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
            emit_line("  " + result + " = call i32 @channel_try_recv(ptr " + ch + ", ptr " + out_ptr + ")");
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
            emit_line("  call void @channel_close(ptr " + ch + ")");
        }
        return "0";
    }

    // channel_destroy(ch) -> Unit
    if (fn_name == "channel_destroy") {
        if (!call.args.empty()) {
            std::string ch = gen_expr(*call.args[0]);
            emit_line("  call void @channel_destroy(ptr " + ch + ")");
        }
        return "0";
    }

    // channel_len(ch) -> I32
    if (fn_name == "channel_len") {
        if (!call.args.empty()) {
            std::string ch = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @channel_len(ptr " + ch + ")");
            return result;
        }
        return "0";
    }

    // ============ MUTEX PRIMITIVES ============

    // mutex_create() -> mutex_ptr
    if (fn_name == "mutex_create") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @mutex_create()");
        return result;
    }

    // mutex_lock(m) -> Unit
    if (fn_name == "mutex_lock") {
        if (!call.args.empty()) {
            std::string m = gen_expr(*call.args[0]);
            emit_line("  call void @mutex_lock(ptr " + m + ")");
        }
        return "0";
    }

    // mutex_unlock(m) -> Unit
    if (fn_name == "mutex_unlock") {
        if (!call.args.empty()) {
            std::string m = gen_expr(*call.args[0]);
            emit_line("  call void @mutex_unlock(ptr " + m + ")");
        }
        return "0";
    }

    // mutex_try_lock(m) -> Bool
    if (fn_name == "mutex_try_lock") {
        if (!call.args.empty()) {
            std::string m = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @mutex_try_lock(ptr " + m + ")");
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
            emit_line("  call void @mutex_destroy(ptr " + m + ")");
        }
        return "0";
    }

    // ============ WAITGROUP PRIMITIVES (Go-style) ============

    // waitgroup_create() -> wg_ptr
    if (fn_name == "waitgroup_create") {
        std::string result = fresh_reg();
        emit_line("  " + result + " = call ptr @waitgroup_create()");
        return result;
    }

    // waitgroup_add(wg, delta: I32) -> Unit
    if (fn_name == "waitgroup_add") {
        if (call.args.size() >= 2) {
            std::string wg = gen_expr(*call.args[0]);
            std::string delta = gen_expr(*call.args[1]);
            emit_line("  call void @waitgroup_add(ptr " + wg + ", i32 " + delta + ")");
        }
        return "0";
    }

    // waitgroup_done(wg) -> Unit
    if (fn_name == "waitgroup_done") {
        if (!call.args.empty()) {
            std::string wg = gen_expr(*call.args[0]);
            emit_line("  call void @waitgroup_done(ptr " + wg + ")");
        }
        return "0";
    }

    // waitgroup_wait(wg) -> Unit
    if (fn_name == "waitgroup_wait") {
        if (!call.args.empty()) {
            std::string wg = gen_expr(*call.args[0]);
            emit_line("  call void @waitgroup_wait(ptr " + wg + ")");
        }
        return "0";
    }

    // waitgroup_destroy(wg) -> Unit
    if (fn_name == "waitgroup_destroy") {
        if (!call.args.empty()) {
            std::string wg = gen_expr(*call.args[0]);
            emit_line("  call void @waitgroup_destroy(ptr " + wg + ")");
        }
        return "0";
    }

    // ============ LIST FUNCTIONS ============

    // list_create(capacity) -> list_ptr
    if (fn_name == "list_create") {
        if (call.args.empty()) {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @list_create(i64 4)");
            return result;
        } else {
            std::string i32_cap = gen_expr(*call.args[0]);
            std::string cap = fresh_reg();
            emit_line("  " + cap + " = sext i32 " + i32_cap + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @list_create(i64 " + cap + ")");
            return result;
        }
    }

    // list_destroy(list) -> Unit
    if (fn_name == "list_destroy") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            emit_line("  call void @list_destroy(ptr " + list + ")");
        }
        return "0";
    }

    // list_push(list, value) -> Unit
    if (fn_name == "list_push") {
        if (call.args.size() >= 2) {
            std::string list = gen_expr(*call.args[0]);
            std::string i32_value = gen_expr(*call.args[1]);
            std::string value = fresh_reg();
            emit_line("  " + value + " = sext i32 " + i32_value + " to i64");
            emit_line("  call void @list_push(ptr " + list + ", i64 " + value + ")");
        }
        return "0";
    }

    // list_pop(list) -> I32
    if (fn_name == "list_pop") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @list_pop(ptr " + list + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // list_get(list, index) -> I32
    if (fn_name == "list_get") {
        if (call.args.size() >= 2) {
            std::string list = gen_expr(*call.args[0]);
            std::string i32_index = gen_expr(*call.args[1]);
            std::string index = fresh_reg();
            emit_line("  " + index + " = sext i32 " + i32_index + " to i64");
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @list_get(ptr " + list + ", i64 " + index + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // list_set(list, index, value) -> Unit
    if (fn_name == "list_set") {
        if (call.args.size() >= 3) {
            std::string list = gen_expr(*call.args[0]);
            std::string i32_index = gen_expr(*call.args[1]);
            std::string index = fresh_reg();
            emit_line("  " + index + " = sext i32 " + i32_index + " to i64");
            std::string i32_value = gen_expr(*call.args[2]);
            std::string value = fresh_reg();
            emit_line("  " + value + " = sext i32 " + i32_value + " to i64");
            emit_line("  call void @list_set(ptr " + list + ", i64 " + index + ", i64 " + value + ")");
        }
        return "0";
    }

    // list_len(list) -> I32
    if (fn_name == "list_len") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @list_len(ptr " + list + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // list_capacity(list) -> I32
    if (fn_name == "list_capacity") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @list_capacity(ptr " + list + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // list_clear(list) -> Unit
    if (fn_name == "list_clear") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            emit_line("  call void @list_clear(ptr " + list + ")");
        }
        return "0";
    }

    // list_is_empty(list) -> Bool
    if (fn_name == "list_is_empty") {
        if (!call.args.empty()) {
            std::string list = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @list_is_empty(ptr " + list + ")");
            std::string bool_result = fresh_reg();
            emit_line("  " + bool_result + " = icmp ne i32 " + result + ", 0");
            return bool_result;
        }
        return "0";
    }

    // ============ HASHMAP FUNCTIONS ============

    // hashmap_create(capacity) -> map_ptr
    if (fn_name == "hashmap_create") {
        if (call.args.empty()) {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @hashmap_create(i64 16)");
            return result;
        } else {
            std::string i32_cap = gen_expr(*call.args[0]);
            std::string cap = fresh_reg();
            emit_line("  " + cap + " = sext i32 " + i32_cap + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @hashmap_create(i64 " + cap + ")");
            return result;
        }
    }

    // hashmap_destroy(map) -> Unit
    if (fn_name == "hashmap_destroy") {
        if (!call.args.empty()) {
            std::string map = gen_expr(*call.args[0]);
            emit_line("  call void @hashmap_destroy(ptr " + map + ")");
        }
        return "0";
    }

    // hashmap_set(map, key, value) -> Unit
    if (fn_name == "hashmap_set") {
        if (call.args.size() >= 3) {
            std::string map = gen_expr(*call.args[0]);
            std::string i32_key = gen_expr(*call.args[1]);
            std::string key = fresh_reg();
            emit_line("  " + key + " = sext i32 " + i32_key + " to i64");
            std::string i32_value = gen_expr(*call.args[2]);
            std::string value = fresh_reg();
            emit_line("  " + value + " = sext i32 " + i32_value + " to i64");
            emit_line("  call void @hashmap_set(ptr " + map + ", i64 " + key + ", i64 " + value + ")");
        }
        return "0";
    }

    // hashmap_get(map, key) -> I32
    if (fn_name == "hashmap_get") {
        if (call.args.size() >= 2) {
            std::string map = gen_expr(*call.args[0]);
            std::string i32_key = gen_expr(*call.args[1]);
            std::string key = fresh_reg();
            emit_line("  " + key + " = sext i32 " + i32_key + " to i64");
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @hashmap_get(ptr " + map + ", i64 " + key + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // hashmap_has(map, key) -> Bool
    if (fn_name == "hashmap_has") {
        if (call.args.size() >= 2) {
            std::string map = gen_expr(*call.args[0]);
            std::string i32_key = gen_expr(*call.args[1]);
            std::string key = fresh_reg();
            emit_line("  " + key + " = sext i32 " + i32_key + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @hashmap_has(ptr " + map + ", i64 " + key + ")");
            return result;
        }
        return "0";
    }

    // hashmap_remove(map, key) -> Bool
    if (fn_name == "hashmap_remove") {
        if (call.args.size() >= 2) {
            std::string map = gen_expr(*call.args[0]);
            std::string i32_key = gen_expr(*call.args[1]);
            std::string key = fresh_reg();
            emit_line("  " + key + " = sext i32 " + i32_key + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i1 @hashmap_remove(ptr " + map + ", i64 " + key + ")");
            return result;
        }
        return "0";
    }

    // hashmap_len(map) -> I32
    if (fn_name == "hashmap_len") {
        if (!call.args.empty()) {
            std::string map = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @hashmap_len(ptr " + map + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // hashmap_clear(map) -> Unit
    if (fn_name == "hashmap_clear") {
        if (!call.args.empty()) {
            std::string map = gen_expr(*call.args[0]);
            emit_line("  call void @hashmap_clear(ptr " + map + ")");
        }
        return "0";
    }

    // ============ BUFFER FUNCTIONS ============

    // buffer_create(capacity) -> buf_ptr
    if (fn_name == "buffer_create") {
        if (call.args.empty()) {
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @buffer_create(i64 16)");
            return result;
        } else {
            std::string i32_cap = gen_expr(*call.args[0]);
            std::string cap = fresh_reg();
            emit_line("  " + cap + " = sext i32 " + i32_cap + " to i64");
            std::string result = fresh_reg();
            emit_line("  " + result + " = call ptr @buffer_create(i64 " + cap + ")");
            return result;
        }
    }

    // buffer_destroy(buf) -> Unit
    if (fn_name == "buffer_destroy") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            emit_line("  call void @buffer_destroy(ptr " + buf + ")");
        }
        return "0";
    }

    // buffer_write_byte(buf, byte) -> Unit
    if (fn_name == "buffer_write_byte") {
        if (call.args.size() >= 2) {
            std::string buf = gen_expr(*call.args[0]);
            std::string byte = gen_expr(*call.args[1]);
            emit_line("  call void @buffer_write_byte(ptr " + buf + ", i32 " + byte + ")");
        }
        return "0";
    }

    // buffer_write_i32(buf, value) -> Unit
    if (fn_name == "buffer_write_i32") {
        if (call.args.size() >= 2) {
            std::string buf = gen_expr(*call.args[0]);
            std::string value = gen_expr(*call.args[1]);
            emit_line("  call void @buffer_write_i32(ptr " + buf + ", i32 " + value + ")");
        }
        return "0";
    }

    // buffer_read_byte(buf) -> I32
    if (fn_name == "buffer_read_byte") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_byte(ptr " + buf + ")");
            return result;
        }
        return "0";
    }

    // buffer_read_i32(buf) -> I32
    if (fn_name == "buffer_read_i32") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string result = fresh_reg();
            emit_line("  " + result + " = call i32 @buffer_read_i32(ptr " + buf + ")");
            return result;
        }
        return "0";
    }

    // buffer_len(buf) -> I32
    if (fn_name == "buffer_len") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @buffer_len(ptr " + buf + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // buffer_capacity(buf) -> I32
    if (fn_name == "buffer_capacity") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @buffer_capacity(ptr " + buf + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // buffer_remaining(buf) -> I32
    if (fn_name == "buffer_remaining") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            std::string i64_result = fresh_reg();
            emit_line("  " + i64_result + " = call i64 @buffer_remaining(ptr " + buf + ")");
            std::string result = fresh_reg();
            emit_line("  " + result + " = trunc i64 " + i64_result + " to i32");
            return result;
        }
        return "0";
    }

    // buffer_clear(buf) -> Unit
    if (fn_name == "buffer_clear") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            emit_line("  call void @buffer_clear(ptr " + buf + ")");
        }
        return "0";
    }

    // buffer_reset_read(buf) -> Unit
    if (fn_name == "buffer_reset_read") {
        if (!call.args.empty()) {
            std::string buf = gen_expr(*call.args[0]);
            emit_line("  call void @buffer_reset_read(ptr " + buf + ")");
        }
        return "0";
    }

    // ============ STRING UTILITIES ============

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
            emit_line("  " + result + " = call ptr @str_substring(ptr " + s + ", i32 " + start + ", i32 " + len + ")");
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
            emit_line("  " + i32_result + " = call i32 @str_contains(ptr " + h + ", ptr " + n + ")");
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
            emit_line("  " + i32_result + " = call i32 @str_starts_with(ptr " + s + ", ptr " + p + ")");
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
            emit_line("  " + i32_result + " = call i32 @str_ends_with(ptr " + s + ", ptr " + suffix + ")");
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

    // ============ ENUM CONSTRUCTORS ============

    // Check if this is an enum constructor
    if (call.callee->is<parser::IdentExpr>()) {
        const auto& ident = call.callee->as<parser::IdentExpr>();

        // First check pending generic enums
        for (const auto& [gen_enum_name, gen_enum_decl] : pending_generic_enums_) {
            for (size_t variant_idx = 0; variant_idx < gen_enum_decl->variants.size(); ++variant_idx) {
                const auto& variant = gen_enum_decl->variants[variant_idx];
                if (variant.name == ident.name) {
                    // Found generic enum constructor
                    std::string enum_type;

                    // Check if variant has payload (tuple_fields for tuple variants like Just(T))
                    bool has_payload = variant.tuple_fields.has_value() && !variant.tuple_fields->empty();

                    // If we have expected type from context, use it (for multi-param generics)
                    if (!expected_enum_type_.empty()) {
                        enum_type = expected_enum_type_;
                    } else if (!current_ret_type_.empty() &&
                               current_ret_type_.find("%struct." + gen_enum_name + "__") == 0) {
                        // Function returns this generic enum type - use the return type directly
                        // This handles multi-param generics like Outcome[T, E] where we can only
                        // infer T from Ok(value) but need E from context
                        enum_type = current_ret_type_;
                    } else {
                        // Infer type from arguments
                        std::vector<types::TypePtr> inferred_type_args;
                        if (has_payload && !call.args.empty()) {
                            types::TypePtr arg_type = infer_expr_type(*call.args[0]);
                            inferred_type_args.push_back(arg_type);
                        } else {
                            // No payload to infer from - default to I32
                            inferred_type_args.push_back(types::make_i32());
                        }
                        std::string mangled_name = require_enum_instantiation(gen_enum_name, inferred_type_args);
                        enum_type = "%struct." + mangled_name;
                    }

                    std::string result = fresh_reg();
                    std::string enum_val = fresh_reg();

                    // Create enum value on stack
                    emit_line("  " + enum_val + " = alloca " + enum_type + ", align 8");

                    // Set tag (field 0)
                    std::string tag_ptr = fresh_reg();
                    emit_line("  " + tag_ptr + " = getelementptr inbounds " + enum_type + ", ptr " + enum_val + ", i32 0, i32 0");
                    emit_line("  store i32 " + std::to_string(variant_idx) + ", ptr " + tag_ptr);

                    // Set payload if present (stored in field 1, the [N x i8] array)
                    if (has_payload && !call.args.empty()) {
                        std::string payload = gen_expr(*call.args[0]);

                        // Get pointer to payload field ([N x i8])
                        std::string payload_ptr = fresh_reg();
                        emit_line("  " + payload_ptr + " = getelementptr inbounds " + enum_type + ", ptr " + enum_val + ", i32 0, i32 1");

                        // Cast payload to bytes and store
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

        // Then check non-generic enums
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
        std::string fn_ptr;
        if (local_it->second.reg[0] == '@') {
            // Direct function reference (closure stored as @tml_closure_0)
            fn_ptr = local_it->second.reg;
        } else {
            // Load the function pointer from the alloca
            fn_ptr = fresh_reg();
            emit_line("  " + fn_ptr + " = load ptr, ptr " + local_it->second.reg);
        }

        // Generate arguments - first add captured variables if this is a closure with captures
        std::vector<std::pair<std::string, std::string>> arg_vals;

        // Prepend captured variables if present
        if (local_it->second.closure_captures.has_value()) {
            const auto& captures = local_it->second.closure_captures.value();
            for (size_t i = 0; i < captures.captured_names.size(); ++i) {
                const std::string& cap_name = captures.captured_names[i];
                const std::string& cap_type = captures.captured_types[i];

                // Look up the captured variable and load its value
                auto cap_it = locals_.find(cap_name);
                if (cap_it != locals_.end()) {
                    std::string cap_val = fresh_reg();
                    emit_line("  " + cap_val + " = load " + cap_type + ", ptr " + cap_it->second.reg);
                    arg_vals.push_back({cap_val, cap_type});
                } else {
                    // Captured variable not found - this shouldn't happen but handle gracefully
                    arg_vals.push_back({"0", cap_type});
                }
            }
        }

        // Add regular call arguments
        for (size_t i = 0; i < call.args.size(); ++i) {
            std::string val = gen_expr(*call.args[i]);
            arg_vals.push_back({val, last_expr_type_});
        }

        // Determine return type from semantic type if available
        std::string ret_type = "i32";  // Default fallback
        if (local_it->second.semantic_type && local_it->second.semantic_type->is<types::FuncType>()) {
            const auto& func_type = local_it->second.semantic_type->as<types::FuncType>();
            ret_type = llvm_type_from_semantic(func_type.return_type);
        }

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

    // Check if this is a generic function call
    auto pending_func_it = pending_generic_funcs_.find(fn_name);
    if (pending_func_it != pending_generic_funcs_.end()) {
        const auto& gen_func = *pending_func_it->second;

        // Build set of generic parameter names for unification
        std::unordered_set<std::string> generic_names;
        for (const auto& g : gen_func.generics) {
            generic_names.insert(g.name);
        }

        // Infer type arguments using unification
        // For each argument, unify the parameter type pattern with the argument type
        std::unordered_map<std::string, types::TypePtr> bindings;
        for (size_t i = 0; i < call.args.size() && i < gen_func.params.size(); ++i) {
            types::TypePtr arg_type = infer_expr_type(*call.args[i]);
            
            unify_types(*gen_func.params[i].type, arg_type, generic_names, bindings);
        }
        
        for (const auto& [k, v] : bindings) {
            
        }

        // Extract inferred type args in the order of generic parameters
        std::vector<types::TypePtr> inferred_type_args;
        for (const auto& g : gen_func.generics) {
            auto it = bindings.find(g.name);
            if (it != bindings.end()) {
                inferred_type_args.push_back(it->second);
            } else {
                // Generic not inferred - use Unit as fallback
                inferred_type_args.push_back(types::make_unit());
            }
        }

        // Register and get mangled name
        std::string mangled_name = require_func_instantiation(fn_name, inferred_type_args);

        // Use bindings as substitution map for return type
        std::unordered_map<std::string, types::TypePtr>& subs = bindings;

        // Get substituted return type
        std::string ret_type = "void";
        if (gen_func.return_type.has_value()) {
            types::TypePtr subbed_ret = resolve_parser_type_with_subs(**gen_func.return_type, subs);
            ret_type = llvm_type_from_semantic(subbed_ret);
        }

        // Generate arguments
        std::vector<std::pair<std::string, std::string>> arg_vals;
        for (size_t i = 0; i < call.args.size(); ++i) {
            std::string val = gen_expr(*call.args[i]);
            std::string arg_type = last_expr_type_;
            arg_vals.push_back({val, arg_type});
        }

        // Call the instantiated function
        std::string func_name = "@tml_" + mangled_name;
        if (ret_type == "void") {
            emit("  call void " + func_name + "(");
            for (size_t i = 0; i < arg_vals.size(); ++i) {
                if (i > 0) emit(", ");
                emit(arg_vals[i].second + " " + arg_vals[i].first);
            }
            emit_line(")");
            last_expr_type_ = "void";
            return "0";
        } else {
            std::string result = fresh_reg();
            emit("  " + result + " = call " + ret_type + " " + func_name + "(");
            for (size_t i = 0; i < arg_vals.size(); ++i) {
                if (i > 0) emit(", ");
                emit(arg_vals[i].second + " " + arg_vals[i].first);
            }
            emit_line(")");
            last_expr_type_ = ret_type;
            return result;
        }
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

    // Generate arguments with proper type conversion
    std::vector<std::pair<std::string, std::string>> arg_vals; // (value, type)
    for (size_t i = 0; i < call.args.size(); ++i) {
        std::string val = gen_expr(*call.args[i]);
        std::string actual_type = last_expr_type_;  // Type of the generated value
        std::string expected_type = "i32";  // Default

        // If we have function signature, use parameter type
        if (func_sig.has_value() && i < func_sig->params.size()) {
            expected_type = llvm_type_from_semantic(func_sig->params[i]);
        } else {
            // Fallback to inference
            // Check if it's a string constant
            if (val.starts_with("@.str.")) {
                expected_type = "ptr";
            } else if (call.args[i]->is<parser::LiteralExpr>()) {
                const auto& lit = call.args[i]->as<parser::LiteralExpr>();
                if (lit.token.kind == lexer::TokenKind::StringLiteral) {
                    expected_type = "ptr";
                } else if (lit.token.kind == lexer::TokenKind::BoolLiteral) {
                    expected_type = "i1";
                }
            }
        }

        // Insert type conversion if needed
        if (actual_type != expected_type) {
            // i32 -> i64 conversion
            if (actual_type == "i32" && expected_type == "i64") {
                std::string converted = fresh_reg();
                emit_line("  " + converted + " = sext i32 " + val + " to i64");
                val = converted;
            }
            // i64 -> i32 conversion (truncate)
            else if (actual_type == "i64" && expected_type == "i32") {
                std::string converted = fresh_reg();
                emit_line("  " + converted + " = trunc i64 " + val + " to i32");
                val = converted;
            }
            // i1 -> i32 conversion (zero extend)
            else if (actual_type == "i1" && expected_type == "i32") {
                std::string converted = fresh_reg();
                emit_line("  " + converted + " = zext i1 " + val + " to i32");
                val = converted;
            }
            // i32 -> i1 conversion (compare ne 0)
            else if (actual_type == "i32" && expected_type == "i1") {
                std::string converted = fresh_reg();
                emit_line("  " + converted + " = icmp ne i32 " + val + ", 0");
                val = converted;
            }
        }

        arg_vals.push_back({val, expected_type});
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
