//! # LLVM IR Generator - Atomic Builtins
//!
//! This file implements atomic operation intrinsics.
//!
//! ## Atomic Operations
//!
//! | Function         | LLVM Instruction             |
//! |------------------|------------------------------|
//! | `atomic_load`    | `load atomic seq_cst`        |
//! | `atomic_store`   | `store atomic seq_cst`       |
//! | `atomic_add`     | `atomicrmw add seq_cst`      |
//! | `atomic_sub`     | `atomicrmw sub seq_cst`      |
//! | `atomic_exchange`| `atomicrmw xchg seq_cst`     |
//! | `atomic_cas`     | `cmpxchg seq_cst`            |
//! | `atomic_and`     | `atomicrmw and seq_cst`      |
//! | `atomic_or`      | `atomicrmw or seq_cst`       |
//!
//! ## Memory Fences
//!
//! | Function        | LLVM Instruction             |
//! |-----------------|------------------------------|
//! | `fence_acquire` | `fence acquire`              |
//! | `fence_release` | `fence release`              |
//! | `fence`         | `fence seq_cst`              |

#include "codegen/llvm/llvm_ir_gen.hpp"

namespace tml::codegen {

auto LLVMIRGen::try_gen_builtin_atomic(const std::string& fn_name, const parser::CallExpr& call)
    -> std::optional<std::string> {

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
            emit_line("  " + result + " = atomicrmw add ptr " + ptr + ", i32 " + val +
                      " seq_cst, align 4");
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
            emit_line("  " + result + " = atomicrmw sub ptr " + ptr + ", i32 " + val +
                      " seq_cst, align 4");
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
            emit_line("  " + result + " = atomicrmw xchg ptr " + ptr + ", i32 " + val +
                      " seq_cst, align 4");
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
            emit_line("  " + cas_result + " = cmpxchg ptr " + ptr + ", i32 " + expected + ", i32 " +
                      desired + " seq_cst seq_cst, align 4");
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
            emit_line("  " + cas_result + " = cmpxchg ptr " + ptr + ", i32 " + expected + ", i32 " +
                      desired + " seq_cst seq_cst, align 4");
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
            emit_line("  " + result + " = atomicrmw and ptr " + ptr + ", i32 " + val +
                      " seq_cst, align 4");
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
            emit_line("  " + result + " = atomicrmw or ptr " + ptr + ", i32 " + val +
                      " seq_cst, align 4");
            return result;
        }
        return "0";
    }

    // fence() - Memory barrier (full fence)
    if (fn_name == "fence") {
        emit_line("  fence seq_cst");
        return "0";
    }

    // compiler_fence() - Compiler-only barrier (no hardware fence)
    if (fn_name == "compiler_fence") {
        emit_line("  fence syncscope(\"singlethread\") seq_cst");
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

    // Typed atomic operations (atomic_fetch_add_i32, atomic_swap_i64, etc.) — REMOVED (Phase 24)
    // Now handled via @extern FFI declarations in core::alloc::sync.tml
    // atomic_fence, atomic_fence_acquire, atomic_fence_release — also removed (use @extern)
    // Generic atomics (atomic_load, atomic_store, atomic_add, etc.) still handled above as inline
    // LLVM

    return std::nullopt;
}

} // namespace tml::codegen
