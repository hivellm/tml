//! # Load-Store Optimization Pass
//!
//! Eliminates redundant loads and stores within basic blocks.

#include "mir/passes/load_store_opt.hpp"

#include <algorithm>

namespace tml::mir {

auto LoadStoreOptPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    for (auto& block : func.blocks) {
        changed |= optimize_block(func, block);
    }

    return changed;
}

auto LoadStoreOptPass::optimize_block(Function& func, BasicBlock& block) -> bool {
    bool changed = false;
    std::unordered_map<ValueId, MemState> mem_state;

    std::vector<size_t> dead_stores;

    for (size_t i = 0; i < block.instructions.size(); ++i) {
        auto& inst = block.instructions[i];

        if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
            ValueId ptr = store->ptr.id;
            ValueId value = store->value.id;

            // Check for dead store: if we already stored to this ptr
            // and haven't loaded from it, the previous store is dead
            auto it = mem_state.find(ptr);
            if (it != mem_state.end() && it->second.has_store && !it->second.has_load) {
                // Find and mark the previous store as dead
                for (size_t j = 0; j < i; ++j) {
                    if (auto* prev_store = std::get_if<StoreInst>(&block.instructions[j].inst)) {
                        if (prev_store->ptr.id == ptr) {
                            dead_stores.push_back(j);
                        }
                    }
                }
            }

            // Update memory state
            mem_state[ptr] = MemState{value, 0, true, false};

            // Conservatively invalidate other pointers that may alias
            // (Simple approach: only track non-aliasing for now)
        } else if (auto* load = std::get_if<LoadInst>(&inst.inst)) {
            ValueId ptr = load->ptr.id;
            ValueId result = inst.result;

            auto it = mem_state.find(ptr);
            if (it != mem_state.end()) {
                if (it->second.has_store) {
                    // Store-to-load forwarding: replace load with stored value
                    ValueId stored = it->second.stored_value;

                    // Replace all uses of result with stored value
                    for (size_t j = i + 1; j < block.instructions.size(); ++j) {
                        std::visit(
                            [result, stored](auto& inner) {
                                using T = std::decay_t<decltype(inner)>;

                                if constexpr (std::is_same_v<T, BinaryInst>) {
                                    if (inner.left.id == result) inner.left.id = stored;
                                    if (inner.right.id == result) inner.right.id = stored;
                                } else if constexpr (std::is_same_v<T, UnaryInst>) {
                                    if (inner.operand.id == result) inner.operand.id = stored;
                                } else if constexpr (std::is_same_v<T, CastInst>) {
                                    if (inner.operand.id == result) inner.operand.id = stored;
                                } else if constexpr (std::is_same_v<T, StoreInst>) {
                                    if (inner.value.id == result) inner.value.id = stored;
                                } else if constexpr (std::is_same_v<T, CallInst>) {
                                    for (auto& arg : inner.args) {
                                        if (arg.id == result) arg.id = stored;
                                    }
                                } else if constexpr (std::is_same_v<T, SelectInst>) {
                                    if (inner.condition.id == result) inner.condition.id = stored;
                                    if (inner.true_val.id == result) inner.true_val.id = stored;
                                    if (inner.false_val.id == result) inner.false_val.id = stored;
                                }
                            },
                            block.instructions[j].inst);
                    }

                    // Mark this load for removal
                    dead_stores.push_back(i);
                    changed = true;
                } else if (it->second.has_load) {
                    // Redundant load: reuse previous load result
                    ValueId prev_load = it->second.loaded_value;

                    // Replace all uses of result with previous load
                    for (size_t j = i + 1; j < block.instructions.size(); ++j) {
                        std::visit(
                            [result, prev_load](auto& inner) {
                                using T = std::decay_t<decltype(inner)>;

                                if constexpr (std::is_same_v<T, BinaryInst>) {
                                    if (inner.left.id == result) inner.left.id = prev_load;
                                    if (inner.right.id == result) inner.right.id = prev_load;
                                } else if constexpr (std::is_same_v<T, UnaryInst>) {
                                    if (inner.operand.id == result) inner.operand.id = prev_load;
                                } else if constexpr (std::is_same_v<T, CastInst>) {
                                    if (inner.operand.id == result) inner.operand.id = prev_load;
                                } else if constexpr (std::is_same_v<T, StoreInst>) {
                                    if (inner.value.id == result) inner.value.id = prev_load;
                                } else if constexpr (std::is_same_v<T, CallInst>) {
                                    for (auto& arg : inner.args) {
                                        if (arg.id == result) arg.id = prev_load;
                                    }
                                } else if constexpr (std::is_same_v<T, SelectInst>) {
                                    if (inner.condition.id == result) inner.condition.id = prev_load;
                                    if (inner.true_val.id == result) inner.true_val.id = prev_load;
                                    if (inner.false_val.id == result) inner.false_val.id = prev_load;
                                }
                            },
                            block.instructions[j].inst);
                    }

                    dead_stores.push_back(i);
                    changed = true;
                }
            }

            // Update memory state with this load
            if (mem_state.find(ptr) == mem_state.end()) {
                mem_state[ptr] = MemState{0, result, false, true};
            } else {
                mem_state[ptr].loaded_value = result;
                mem_state[ptr].has_load = true;
            }
        } else if (std::holds_alternative<CallInst>(inst.inst) ||
                   std::holds_alternative<MethodCallInst>(inst.inst)) {
            // Calls may have side effects - invalidate all memory state
            invalidate_all(mem_state);
        }
    }

    // Remove dead instructions (in reverse order to maintain indices)
    std::sort(dead_stores.begin(), dead_stores.end(), std::greater<size_t>());
    dead_stores.erase(std::unique(dead_stores.begin(), dead_stores.end()), dead_stores.end());

    for (size_t idx : dead_stores) {
        block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(idx));
        changed = true;
    }

    (void)func; // May be used in future for cross-block analysis
    return changed;
}

auto LoadStoreOptPass::may_alias(ValueId ptr1, ValueId ptr2) -> bool {
    // Conservative: assume everything may alias unless it's the same pointer
    // A more sophisticated analysis would use points-to analysis
    return ptr1 != ptr2;
}

auto LoadStoreOptPass::invalidate_all(std::unordered_map<ValueId, MemState>& mem_state) -> void {
    mem_state.clear();
}

} // namespace tml::mir
