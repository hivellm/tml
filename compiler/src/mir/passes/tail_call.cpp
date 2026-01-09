//! # Tail Call Optimization Pass
//!
//! Identifies and marks tail calls for optimization.

#include "mir/passes/tail_call.hpp"

namespace tml::mir {

auto TailCallPass::run_on_function(Function& func) -> bool {
    bool changed = false;
    tail_calls_.clear();

    for (auto& block : func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            if (is_tail_call_candidate(func, block, i)) {
                auto& inst = block.instructions[i];

                // Mark as tail call
                if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                    tail_calls_.insert(inst.result);
                    changed = true;
                    (void)call; // Suppress unused warning
                } else if (auto* method = std::get_if<MethodCallInst>(&inst.inst)) {
                    tail_calls_.insert(inst.result);
                    changed = true;
                    (void)method;
                }
            }
        }
    }

    return changed;
}

auto TailCallPass::is_tail_call_candidate(const Function& func, const BasicBlock& block,
                                           size_t inst_idx) -> bool {
    auto& inst = block.instructions[inst_idx];

    // Must be a call instruction
    bool is_call = std::holds_alternative<CallInst>(inst.inst) ||
                   std::holds_alternative<MethodCallInst>(inst.inst);
    if (!is_call) {
        return false;
    }

    // Check return type compatibility
    MirTypePtr call_return_type;
    if (auto* call = std::get_if<CallInst>(&inst.inst)) {
        call_return_type = call->return_type;
    } else if (auto* method = std::get_if<MethodCallInst>(&inst.inst)) {
        call_return_type = method->return_type;
    }

    // The call's return type should match the function's return type
    // (simplified check - would need more sophisticated type comparison)
    (void)func; // Function return type check would go here

    // Check if this is the last instruction before a return
    return is_followed_by_return(block, inst_idx, inst.result);
}

auto TailCallPass::is_followed_by_return(const BasicBlock& block, size_t inst_idx,
                                          ValueId call_result) -> bool {
    // Check if there are any instructions between the call and the terminator
    // (other than the return itself)

    // If there are instructions after the call, they must all be dead
    // or the call can't be a tail call
    if (inst_idx + 1 < block.instructions.size()) {
        // Any instruction after the call means it's not a tail call
        // (unless the instruction is a store that doesn't affect the return)
        return false;
    }

    // Check if the terminator is a return of the call result
    if (!block.terminator) {
        return false;
    }

    if (auto* ret = std::get_if<ReturnTerm>(&*block.terminator)) {
        // If returning void, the call must also return void
        if (!ret->value) {
            // Check if call returns unit/void
            return call_result == INVALID_VALUE;
        }

        // The return value must be exactly the call result
        return ret->value->id == call_result;
    }

    return false;
}

} // namespace tml::mir
