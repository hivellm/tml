// LLVM IR generator - Drop/RAII support
// Implements automatic drop call generation at scope exit

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
    std::string drop_func = "@tml_" + info.type_name + "_drop";
    emit_line("  call void " + drop_func + "(ptr " + info.var_reg + ")");
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
