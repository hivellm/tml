TML_MODULE("compiler")

//! # Interprocedural Optimization (IPO) Pass - Implementation

#include "mir/passes/ipo.hpp"

namespace tml::mir {

// ============================================================================
// IpoPass - Main IPO Implementation
// ============================================================================

auto IpoPass::run(Module& module) -> bool {
    stats_.reset();
    function_attrs_.clear();
    constant_args_.clear();
    analyzed_functions_.clear();

    bool changed = false;

    // Phase 1: Analyze all functions for attributes
    for (const auto& func : module.functions) {
        auto attrs = analyze_function_attributes(func);
        function_attrs_[func.name] = attrs;

        if (attrs.is_pure)
            stats_.pure_functions_found++;
        if (attrs.is_nothrow)
            stats_.nothrow_functions_found++;
        if (attrs.is_readonly)
            stats_.readonly_functions_found++;
    }

    // Phase 2: Gather constant argument information
    analyze_calls(module);

    // Phase 3: Apply interprocedural constant propagation
    if (apply_ipcp(module)) {
        changed = true;
    }

    // Phase 4: Apply argument promotion
    if (apply_argument_promotion(module)) {
        changed = true;
    }

    return changed;
}

auto IpoPass::get_attributes(const std::string& func_name) const -> const FunctionAttributes* {
    auto it = function_attrs_.find(func_name);
    if (it != function_attrs_.end()) {
        return &it->second;
    }
    return nullptr;
}

void IpoPass::analyze_calls(const Module& module) {
    for (const auto& func : module.functions) {
        for (const auto& block : func.blocks) {
            for (const auto& inst_data : block.instructions) {
                if (auto* call = std::get_if<CallInst>(&inst_data.inst)) {
                    // Analyze each argument for constant values
                    for (size_t i = 0; i < call->args.size(); i++) {
                        // Check if argument is a constant
                        // (would need to trace back to ConstantInst)
                        // For now, skip complex analysis
                    }
                }
            }
        }
    }
}

auto IpoPass::analyze_function_attributes(const Function& func) -> FunctionAttributes {
    FunctionAttributes attrs;
    attrs.is_pure = is_function_pure(func);
    attrs.is_nothrow = is_function_nothrow(func);
    attrs.is_readonly = is_function_readonly(func);
    attrs.is_willreturn = is_function_willreturn(func);
    attrs.is_speculatable = attrs.is_pure && attrs.is_nothrow && attrs.is_willreturn;
    return attrs;
}

auto IpoPass::is_function_pure(const Function& func) const -> bool {
    for (const auto& block : func.blocks) {
        for (const auto& inst_data : block.instructions) {
            // Store instructions are side effects
            if (std::holds_alternative<StoreInst>(inst_data.inst)) {
                // Check if it's storing to a local alloca (that's OK for pure)
                // For simplicity, mark as impure if any store
                return false;
            }

            // Call instructions may have side effects
            if (auto* call = std::get_if<CallInst>(&inst_data.inst)) {
                // Check if callee is known pure
                auto it = function_attrs_.find(call->func_name);
                if (it == function_attrs_.end() || !it->second.is_pure) {
                    // Unknown function or not pure - assume impure
                    // Exception: known pure builtins
                    if (call->func_name.find("@llvm.") == 0) {
                        // LLVM intrinsics are usually pure
                        continue;
                    }
                    return false;
                }
            }
        }
    }
    return true;
}

auto IpoPass::is_function_nothrow(const Function& func) const -> bool {
    for (const auto& block : func.blocks) {
        for (const auto& inst_data : block.instructions) {
            // Call instructions may throw
            if (auto* call = std::get_if<CallInst>(&inst_data.inst)) {
                // Check if callee is known nothrow
                auto it = function_attrs_.find(call->func_name);
                if (it == function_attrs_.end() || !it->second.is_nothrow) {
                    // Check for panic/throw calls
                    if (call->func_name.find("panic") != std::string::npos ||
                        call->func_name.find("throw") != std::string::npos ||
                        call->func_name.find("abort") != std::string::npos) {
                        return false;
                    }
                    // Unknown function - assume may throw for safety
                    return false;
                }
            }
        }

        // Check terminator for unreachable (indicates panic/throw)
        if (block.terminator) {
            if (std::holds_alternative<UnreachableTerm>(*block.terminator)) {
                // This path panics/throws
                return false;
            }
        }
    }
    return true;
}

auto IpoPass::is_function_readonly(const Function& func) const -> bool {
    for (const auto& block : func.blocks) {
        for (const auto& inst_data : block.instructions) {
            // Store instructions write memory
            if (std::holds_alternative<StoreInst>(inst_data.inst)) {
                return false;
            }

            // Some calls may write memory
            if (auto* call = std::get_if<CallInst>(&inst_data.inst)) {
                auto it = function_attrs_.find(call->func_name);
                if (it == function_attrs_.end() || !it->second.is_readonly) {
                    // Check for known readonly builtins
                    if (call->func_name.find("@llvm.") == 0 &&
                        call->func_name.find("read") != std::string::npos) {
                        continue;
                    }
                    return false;
                }
            }
        }
    }
    return true;
}

auto IpoPass::is_function_willreturn(const Function& func) const -> bool {
    // Simple analysis: assume will return unless infinite loop detected
    // A more sophisticated analysis would trace all paths

    // Check for obvious infinite loops
    for (const auto& block : func.blocks) {
        // If a block branches back to itself unconditionally, it's an infinite loop
        if (block.terminator) {
            if (auto* branch = std::get_if<BranchTerm>(&*block.terminator)) {
                if (branch->target == block.id) {
                    return false; // Unconditional self-loop
                }
            }
        }
    }

    return true;
}

auto IpoPass::apply_ipcp(Module& module) -> bool {
    // For now, this is a placeholder
    // Full IPCP would:
    // 1. Find call sites with constant arguments
    // 2. Clone the function with constants substituted
    // 3. Replace call sites with calls to specialized version
    (void)module;
    return false;
}

auto IpoPass::apply_argument_promotion(Module& module) -> bool {
    bool changed = false;

    for (auto& func : module.functions) {
        for (size_t i = 0; i < func.params.size(); i++) {
            if (can_promote_argument(func, i)) {
                if (promote_argument(func, i)) {
                    stats_.args_promoted++;
                    changed = true;
                }
            }
        }
    }

    return changed;
}

auto IpoPass::can_promote_argument(const Function& func, size_t arg_index) const -> bool {
    if (arg_index >= func.params.size()) {
        return false;
    }

    const auto& param = func.params[arg_index];

    // Check if parameter is a reference type
    if (!param.type) {
        return false;
    }

    auto* ptr_type = std::get_if<MirPointerType>(&param.type->kind);
    if (!ptr_type) {
        return false; // Not a reference
    }

    // Check if pointee is a small type (primitives, small structs)
    if (!ptr_type->pointee) {
        return false;
    }

    // Only promote primitives for now
    if (!std::holds_alternative<MirPrimitiveType>(ptr_type->pointee->kind)) {
        return false;
    }

    // Check that the reference is only read, not written through
    // (would need escape analysis for full correctness)

    return true;
}

auto IpoPass::promote_argument(Function& func, size_t arg_index) -> bool {
    // This would require:
    // 1. Change parameter type from ref T to T
    // 2. Update all uses in the function body
    // 3. Update all call sites to pass value instead of ref
    // Complex transformation - placeholder for now
    (void)func;
    (void)arg_index;
    return false;
}

// ============================================================================
// IpcpPass - Interprocedural Constant Propagation
// ============================================================================

auto IpcpPass::run(Module& module) -> bool {
    constant_args_.clear();
    gather_constants(module);

    bool changed = false;

    // For each function with constant args, specialize
    for (auto& func : module.functions) {
        auto it = constant_args_.find(func.name);
        if (it != constant_args_.end()) {
            for (const auto& [arg_idx, value] : it->second) {
                if (specialize_function(func, arg_idx, value)) {
                    changed = true;
                }
            }
        }
    }

    return changed;
}

void IpcpPass::gather_constants(const Module& module) {
    // Build a map from ValueId to Constant for each function
    // Then scan calls and record constant arguments

    for (const auto& func : module.functions) {
        // Build value -> constant map for this function
        std::unordered_map<ValueId, Constant> value_constants;
        for (const auto& block : func.blocks) {
            for (const auto& inst_data : block.instructions) {
                if (auto* const_inst = std::get_if<ConstantInst>(&inst_data.inst)) {
                    if (inst_data.result != INVALID_VALUE) {
                        value_constants[inst_data.result] = const_inst->value;
                    }
                }
            }
        }

        // Scan calls and track constant arguments
        for (const auto& block : func.blocks) {
            for (const auto& inst_data : block.instructions) {
                if (auto* call = std::get_if<CallInst>(&inst_data.inst)) {
                    for (size_t i = 0; i < call->args.size(); ++i) {
                        auto it = value_constants.find(call->args[i].id);
                        if (it != value_constants.end()) {
                            // Found a constant argument
                            auto& func_constants = constant_args_[call->func_name];
                            // Check if we already have this arg_index -> update or add
                            auto existing = func_constants.find(i);
                            if (existing == func_constants.end()) {
                                func_constants[i] = it->second;
                            }
                            // If different constant, we can't specialize (would need to track all)
                        }
                    }
                }
            }
        }
    }
}

auto IpcpPass::specialize_function(Function& func, size_t arg_index, const Constant& value)
    -> bool {
    // Placeholder - would replace parameter uses with constant
    (void)func;
    (void)arg_index;
    (void)value;
    return false;
}

// ============================================================================
// ArgPromotionPass
// ============================================================================

auto ArgPromotionPass::run(Module& module) -> bool {
    bool changed = false;

    for (auto& func : module.functions) {
        for (size_t i = 0; i < func.params.size(); i++) {
            if (should_promote(func.params[i])) {
                if (promote_param(func, i)) {
                    update_call_sites(module, func.name, i);
                    changed = true;
                }
            }
        }
    }

    return changed;
}

auto ArgPromotionPass::should_promote(const FunctionParam& param) const -> bool {
    if (!param.type) {
        return false;
    }

    // Check if it's a reference type
    auto* ptr_type = std::get_if<MirPointerType>(&param.type->kind);
    if (!ptr_type || !ptr_type->pointee) {
        return false;
    }

    // Only promote small types (primitives)
    return std::holds_alternative<MirPrimitiveType>(ptr_type->pointee->kind);
}

auto ArgPromotionPass::promote_param(Function& func, size_t param_index) -> bool {
    // Placeholder for actual transformation
    (void)func;
    (void)param_index;
    return false;
}

void ArgPromotionPass::update_call_sites(Module& module, const std::string& func_name,
                                         size_t param_index) {
    // Would need to update all call sites
    (void)module;
    (void)func_name;
    (void)param_index;
}

// ============================================================================
// AttrInferencePass
// ============================================================================

auto AttrInferencePass::run(Module& module) -> bool {
    attrs_.clear();

    for (const auto& func : module.functions) {
        auto attrs = analyze_function(func);
        attrs_[func.name] = attrs;

        // Add inferred attributes to function
        // This would require modifying Function to store attributes
    }

    return false; // Analysis pass, doesn't modify code
}

auto AttrInferencePass::get_attributes(const std::string& func_name) const
    -> const FunctionAttributes* {
    auto it = attrs_.find(func_name);
    if (it != attrs_.end()) {
        return &it->second;
    }
    return nullptr;
}

auto AttrInferencePass::analyze_function(const Function& func) -> FunctionAttributes {
    FunctionAttributes attrs;

    bool has_stores = false;
    bool has_throws = false;
    bool has_loads = false;

    for (const auto& block : func.blocks) {
        for (const auto& inst_data : block.instructions) {
            if (has_side_effects(inst_data.inst)) {
                attrs.is_pure = false;
            }
            if (can_throw(inst_data.inst)) {
                has_throws = true;
            }
            if (reads_memory(inst_data.inst)) {
                has_loads = true;
            }
            if (writes_memory(inst_data.inst)) {
                has_stores = true;
            }
        }
    }

    attrs.is_nothrow = !has_throws;
    attrs.is_readonly = !has_stores;
    attrs.is_pure = !has_stores && !has_throws;
    attrs.is_willreturn = true; // Simplified assumption
    attrs.is_speculatable = attrs.is_pure && attrs.is_nothrow;

    (void)has_loads;
    return attrs;
}

auto AttrInferencePass::has_side_effects(const Instruction& inst) const -> bool {
    return std::holds_alternative<StoreInst>(inst) || std::holds_alternative<CallInst>(inst);
}

auto AttrInferencePass::can_throw(const Instruction& inst) const -> bool {
    if (auto* call = std::get_if<CallInst>(&inst)) {
        return call->func_name.find("panic") != std::string::npos ||
               call->func_name.find("throw") != std::string::npos;
    }
    return false;
}

auto AttrInferencePass::reads_memory(const Instruction& inst) const -> bool {
    return std::holds_alternative<LoadInst>(inst);
}

auto AttrInferencePass::writes_memory(const Instruction& inst) const -> bool {
    return std::holds_alternative<StoreInst>(inst);
}

} // namespace tml::mir
