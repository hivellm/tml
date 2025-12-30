// LLVM IR generator - Synchronization builtin functions
// Handles: spinlock, threading, channels, mutex, waitgroup primitives

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_sync(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

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
            emit_line("  " + old_val + " = atomicrmw xchg ptr " + lock +
                      ", i32 1 acquire, align 4");
            std::string was_free = fresh_reg();
            emit_line("  " + was_free + " = icmp eq i32 " + old_val + ", 0");
            emit_line("  br i1 " + was_free + ", label %" + label_acquired + ", label %" +
                      label_loop);
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
            emit_line("  " + old_val + " = atomicrmw xchg ptr " + lock +
                      ", i32 1 acquire, align 4");
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
            emit_line("  " + result + " = call ptr @thread_spawn(ptr " + func_ptr + ", ptr " +
                      arg_ptr + ")");
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
            emit_line("  " + result + " = call i32 @channel_send(ptr " + ch + ", i32 " + value +
                      ")");
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
            emit_line("  " + result + " = call i32 @channel_try_send(ptr " + ch + ", i32 " + value +
                      ")");
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
            emit_line("  " + result + " = call i32 @channel_try_recv(ptr " + ch + ", ptr " +
                      out_ptr + ")");
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

    return std::nullopt;
}

} // namespace tml::codegen
