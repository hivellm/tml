//! # Return Value Optimization (RVO) Pass - Implementation
//!
//! This pass implements NRVO by identifying local variables returned from
//! all exit paths and eliminating unnecessary copies.

#include "mir/passes/rvo.hpp"

#include <algorithm>

namespace tml::mir {

// ============================================================================
// RvoPass - Function-level RVO
// ============================================================================

auto RvoPass::run_on_function(Function& func) -> bool {
    stats_.functions_analyzed++;

    // Clear state from previous function
    local_assignments_.clear();
    returned_locals_.clear();

    // Phase 1: Find all return statements
    auto returns = find_returns(func);

    if (returns.empty()) {
        return false;
    }

    bool changed = false;

    // Phase 2: Check if all returns return the same local variable
    auto common_local = all_returns_same_local(returns);

    if (common_local) {
        // All returns use the same local variable - apply NRVO
        if (apply_nrvo(func, *common_local)) {
            stats_.nrvo_applied++;
            stats_.multiple_returns_unified++;
            changed = true;
        }
    }

    // Phase 3: Check if function should use sret calling convention
    // DISABLED: sret conversion breaks inlining - functions become void returns
    // and inlining doesn't handle this case properly. Need to either:
    // 1. Run RVO after all inlining
    // 2. Make inlining aware of sret
    // For now, disable sret and focus on extractvalue/insertvalue optimizations
    // if (should_use_sret(func)) {
    //     if (convert_to_sret(func)) {
    //         stats_.sret_conversions++;
    //         changed = true;
    //     }
    // }

    return changed;
}

auto RvoPass::find_returns(const Function& func) -> std::vector<ReturnInfo> {
    std::vector<ReturnInfo> returns;

    for (size_t block_idx = 0; block_idx < func.blocks.size(); block_idx++) {
        const auto& block = func.blocks[block_idx];

        // Check the terminator for return statements
        if (block.terminator) {
            if (auto* ret = std::get_if<ReturnTerm>(&*block.terminator)) {
                ReturnInfo info;
                info.block_id = static_cast<uint32_t>(block_idx);
                info.inst_index = block.instructions.size(); // Terminator is after all instructions
                info.returned_value = ret->value.has_value() ? ret->value->id : INVALID_VALUE;
                info.is_local_var = is_local_variable(func, info.returned_value);
                info.is_struct_literal = is_struct_literal(func, info.returned_value);

                returns.push_back(info);
            }
        }
    }

    return returns;
}

auto RvoPass::all_returns_same_local(const std::vector<ReturnInfo>& returns)
    -> std::optional<ValueId> {
    if (returns.empty()) {
        return std::nullopt;
    }

    // Check if all returns are local variables
    for (const auto& ret : returns) {
        if (!ret.is_local_var) {
            return std::nullopt;
        }
    }

    // Check if all returns return the same variable
    ValueId first_var = returns[0].returned_value;
    for (size_t i = 1; i < returns.size(); i++) {
        if (returns[i].returned_value != first_var) {
            return std::nullopt;
        }
    }

    return first_var;
}

auto RvoPass::is_local_variable(const Function& func, ValueId value) const -> bool {
    if (value == INVALID_VALUE) {
        return false;
    }

    // Check if this value is defined by an AllocaInst in the entry block
    if (!func.blocks.empty()) {
        for (const auto& inst : func.blocks[0].instructions) {
            if (inst.result == value) {
                return std::holds_alternative<AllocaInst>(inst.inst);
            }
        }
    }

    // Also check other blocks for allocas (though they're usually in entry)
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == value) {
                return std::holds_alternative<AllocaInst>(inst.inst);
            }
        }
    }

    return false;
}

auto RvoPass::is_struct_literal(const Function& func, ValueId value) const -> bool {
    if (value == INVALID_VALUE) {
        return false;
    }

    // Search all blocks for the instruction that defines this value
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == value) {
                // Check if defined by StructInitInst, EnumInitInst, or TupleInitInst
                return std::holds_alternative<StructInitInst>(inst.inst) ||
                       std::holds_alternative<EnumInitInst>(inst.inst) ||
                       std::holds_alternative<TupleInitInst>(inst.inst);
            }
        }
    }

    return false;
}

auto RvoPass::should_use_sret(const Function& func) const -> bool {
    // Check if the function returns a struct type that's larger than threshold
    // For now, we check if the return type is a named struct type
    if (func.return_type) {
        if (auto* struct_type = std::get_if<MirStructType>(&func.return_type->kind)) {
            // Named struct types should use sret
            (void)struct_type;
            return true;
        }
        if (auto* tuple_type = std::get_if<MirTupleType>(&func.return_type->kind)) {
            // Tuple type - check element count as heuristic
            // More than 2 elements (> 16 bytes on 64-bit) -> use sret
            return tuple_type->elements.size() > 2;
        }
    }
    return false;
}

void RvoPass::mark_for_return_slot(Function& func, ValueId local_var) {
    // Mark this variable as being constructed directly in the return slot
    // This is done by adding metadata to the alloca instruction
    (void)func;
    (void)local_var;
    // In a full implementation, we would:
    // 1. Find the AllocaInst for local_var
    // 2. Add metadata indicating it should use the return slot
    // 3. Codegen would recognize this and avoid creating a separate alloca
}

auto RvoPass::convert_to_sret(Function& func) -> bool {
    // Skip if already converted or no return type
    if (func.uses_sret || !func.return_type || func.return_type->is_unit()) {
        return false;
    }

    // Skip main function - it must return I32 by convention
    if (func.name == "main" || func.name == "tml_main") {
        return false;
    }

    // Save original return type for codegen
    func.original_return_type = func.return_type;

    // Create sret parameter with a new value ID
    ValueId sret_id = func.fresh_value();
    func.sret_param_id = sret_id;

    FunctionParam sret_param;
    sret_param.name = "sret";
    sret_param.type = make_pointer_type(func.return_type, true); // mutable pointer
    sret_param.value_id = sret_id;

    // Insert sret as first parameter
    func.params.insert(func.params.begin(), sret_param);

    // Transform all return statements:
    // 1. Add store of return value to sret pointer before return
    // 2. Change return to return void
    for (auto& block : func.blocks) {
        if (!block.terminator)
            continue;

        if (auto* ret = std::get_if<ReturnTerm>(&*block.terminator)) {
            if (ret->value.has_value()) {
                // Create store instruction: store return_value to sret pointer
                StoreInst store;
                store.ptr = Value{sret_id, sret_param.type};
                store.value = *ret->value;
                store.value_type = func.original_return_type;

                InstructionData store_inst;
                store_inst.result = INVALID_VALUE; // Store has no result
                store_inst.type = nullptr;
                store_inst.inst = store;
                store_inst.span = {}; // No span for generated instructions

                block.instructions.push_back(store_inst);

                // Change return to void
                ret->value = std::nullopt;
            }
        }
    }

    // Change function return type to void
    func.return_type = make_unit_type();
    func.uses_sret = true;

    return true;
}

auto RvoPass::apply_nrvo(Function& func, ValueId local_var) -> bool {
    // Mark the local variable for return slot construction
    mark_for_return_slot(func, local_var);
    returned_locals_.insert(local_var);
    return true;
}

// ============================================================================
// ModuleRvoPass - Module-level RVO
// ============================================================================

auto ModuleRvoPass::run(Module& module) -> bool {
    RvoPass func_pass;

    // FunctionPass::run iterates over functions and calls run_on_function
    bool changed = func_pass.run(module);

    // Aggregate stats
    stats_ = func_pass.stats();

    return changed;
}

// ============================================================================
// SretConversionPass - Post-inlining sret conversion
// ============================================================================

auto SretConversionPass::run_on_function(Function& func) -> bool {
    // Only convert if beneficial and not already converted
    if (!should_use_sret(func)) {
        return false;
    }

    if (convert_to_sret(func)) {
        conversions_++;
        return true;
    }

    return false;
}

auto SretConversionPass::should_use_sret(const Function& func) const -> bool {
    // Skip if already converted
    if (func.uses_sret) {
        return false;
    }

    // Skip if no return type or void
    if (!func.return_type || func.return_type->is_unit()) {
        return false;
    }

    // Skip main function - it must return I32 by convention
    if (func.name == "main" || func.name == "tml_main") {
        return false;
    }

    // Skip extern functions
    if (func.blocks.empty()) {
        return false;
    }

    // Check if the return type is a struct/tuple that would benefit from sret
    if (auto* struct_type = std::get_if<MirStructType>(&func.return_type->kind)) {
        // Named struct types should use sret
        (void)struct_type;
        return true;
    }

    if (auto* tuple_type = std::get_if<MirTupleType>(&func.return_type->kind)) {
        // Tuple with more than 2 elements (> 16 bytes typically)
        return tuple_type->elements.size() > 2;
    }

    return false;
}

auto SretConversionPass::convert_to_sret(Function& func) -> bool {
    // Save original return type for codegen
    func.original_return_type = func.return_type;

    // Create sret parameter with a new value ID
    ValueId sret_id = func.fresh_value();
    func.sret_param_id = sret_id;

    FunctionParam sret_param;
    sret_param.name = "sret";
    sret_param.type = make_pointer_type(func.return_type, true); // mutable pointer
    sret_param.value_id = sret_id;

    // Insert sret as first parameter
    func.params.insert(func.params.begin(), sret_param);

    // Transform all return statements:
    // 1. Add store of return value to sret pointer before return
    // 2. Change return to return void
    for (auto& block : func.blocks) {
        if (!block.terminator)
            continue;

        if (auto* ret = std::get_if<ReturnTerm>(&*block.terminator)) {
            if (ret->value.has_value()) {
                // Create store instruction: store return_value to sret pointer
                StoreInst store;
                store.ptr = Value{sret_id, sret_param.type};
                store.value = *ret->value;
                store.value_type = func.original_return_type;

                InstructionData store_inst;
                store_inst.result = INVALID_VALUE; // Store has no result
                store_inst.type = nullptr;
                store_inst.inst = store;
                store_inst.span = {}; // No span for generated instructions

                block.instructions.push_back(store_inst);

                // Change return to void
                ret->value = std::nullopt;
            }
        }
    }

    // Change function return type to void
    func.return_type = make_unit_type();
    func.uses_sret = true;

    return true;
}

} // namespace tml::mir
