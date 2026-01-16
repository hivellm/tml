//! # LLVM IR Generator - Drop/RAII Support
//!
//! This file implements automatic destructor calls at scope exit.
//!
//! ## RAII in TML
//!
//! Types implementing the `Drop` behavior have their `drop()` method
//! called automatically when they go out of scope.
//!
//! ## Drop Scope Stack
//!
//! `drop_scopes_` tracks variables needing drop per lexical scope:
//!
//! | Method            | Action                              |
//! |-------------------|-------------------------------------|
//! | `push_drop_scope` | Enter new scope (e.g., block)       |
//! | `pop_drop_scope`  | Exit scope                          |
//! | `register_for_drop`| Track variable for later drop      |
//! | `emit_scope_drops`| Emit drop calls at scope exit       |
//!
//! ## Drop Order
//!
//! Drops are emitted in LIFO order (last declared, first dropped),
//! matching Rust's drop semantics.
//!
//! ## Generated Code
//!
//! ```llvm
//! ; At scope exit:
//! call void @tml_Resource_drop(ptr %resource)
//! ```

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

void LLVMIRGen::push_drop_scope() {
    drop_scopes_.push_back({});
}

void LLVMIRGen::pop_drop_scope() {
    if (!drop_scopes_.empty()) {
        drop_scopes_.pop_back();
    }
}

void LLVMIRGen::register_for_drop(const std::string& var_name, const std::string& var_reg,
                                  const std::string& type_name, const std::string& llvm_type) {
    // Only register if the type needs drop (implements Drop behavior)
    if (type_name.empty() || !env_.type_needs_drop(type_name)) {
        return;
    }

    if (!drop_scopes_.empty()) {
        drop_scopes_.back().push_back(DropInfo{var_name, var_reg, type_name, llvm_type});
    }
}

void LLVMIRGen::emit_drop_call(const DropInfo& info) {
    // Load the value from the variable's alloca
    std::string value_reg = fresh_reg();
    emit_line("  " + value_reg + " = load " + info.llvm_type + ", ptr " + info.var_reg);

    // Create a pointer to pass to drop (drop takes mut this)
    // Actually, for drop we pass the pointer directly since it's `mut this`
    // The drop function signature is: void @tml_TypeName_drop(ptr %this)
    // Look up in functions_ to get the correct LLVM name
    std::string drop_lookup_key = info.type_name + "_drop";
    auto drop_it = functions_.find(drop_lookup_key);
    std::string drop_func;
    if (drop_it != functions_.end()) {
        drop_func = drop_it->second.llvm_name;
    } else {
        drop_func = "@tml_" + get_suite_prefix() + info.type_name + "_drop";
    }
    emit_line("  call void " + drop_func + "(ptr " + info.var_reg + ")");

    // For @pool(thread_local: true) classes, release to thread-local pool
    if (tls_pool_classes_.count(info.type_name) > 0) {
        // Get class type for size calculation
        std::string class_type = "%class." + info.type_name;
        emit_line("  call void @tls_pool_release(ptr @pool.name." + info.type_name + ", ptr " +
                  info.var_reg + ", i64 ptrtoint (" + class_type + "* getelementptr (" +
                  class_type + ", " + class_type + "* null, i32 1) to i64))");
    }
    // For @pool classes (non-thread-local), release to global pool
    else if (pool_classes_.count(info.type_name) > 0) {
        emit_line("  call void @pool_release(ptr @pool." + info.type_name + ", ptr " +
                  info.var_reg + ")");
    }
}

void LLVMIRGen::emit_scope_drops() {
    if (drop_scopes_.empty()) {
        return;
    }

    // Emit drops in reverse order (LIFO - last declared is dropped first)
    const auto& current_scope = drop_scopes_.back();
    for (auto it = current_scope.rbegin(); it != current_scope.rend(); ++it) {
        emit_drop_call(*it);
    }
}

void LLVMIRGen::emit_all_drops() {
    // Emit drops for all scopes in reverse order (innermost to outermost)
    for (auto scope_it = drop_scopes_.rbegin(); scope_it != drop_scopes_.rend(); ++scope_it) {
        // Within each scope, drop in reverse declaration order
        for (auto it = scope_it->rbegin(); it != scope_it->rend(); ++it) {
            emit_drop_call(*it);
        }
    }
}

} // namespace tml::codegen
