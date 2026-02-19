//! # LLVM IR Generator - Synchronization Builtins
//!
//! This file implements thread synchronization intrinsics.
//!
//! ## Spinlock
//!
//! | Function       | Implementation              |
//! |----------------|-----------------------------|
//! | `spin_lock`    | CAS loop until acquired     |
//! | `spin_unlock`  | Atomic store 0              |
//! | `spin_trylock` | Single CAS attempt          |
//!
//! ## Threading
//!
//! | Function       | Runtime Call                |
//! |----------------|----------------------------|
//! | `thread_yield` | `@thread_yield`            |
//! | `thread_id`    | `@thread_id`               |
//! | `thread_sleep` | `@thread_sleep`            |
//!
//! ## Channels / Mutex / WaitGroup
//!
//! Higher-level primitives delegated to runtime functions.

#include "codegen/llvm/llvm_ir_gen.hpp"

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

    // Threading, Channel, Mutex, WaitGroup primitives — REMOVED (Phase 24)
    // thread_spawn/join/yield/sleep/id → @extern("tml_thread_*") in std::thread
    // channel_* → MPSC channels built from Mutex+Condvar in std::sync::mpsc
    // mutex_* → @extern("tml_mutex_*") in std::sync::mutex
    // waitgroup_* → not used in TML library

    return std::nullopt;
}

} // namespace tml::codegen
