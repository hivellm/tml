//! # Load-Store Optimization Pass
//!
//! Eliminates redundant loads and stores within basic blocks.

#include "mir/passes/load_store_opt.hpp"

#include "mir/passes/alias_analysis.hpp"

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
            // Skip volatile stores - they must not be optimized away
            if (store->is_volatile) {
                // Volatile store: invalidate any cached state for this pointer
                // since external code may observe the store, but don't eliminate it
                mem_state.erase(store->ptr.id);
                continue;
            }

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

            // Invalidate aliasing pointers before updating (a store to ptr may
            // invalidate other memory locations that alias with ptr)
            invalidate_aliasing(mem_state, ptr);

            // Update memory state for this pointer
            mem_state[ptr] = MemState{value, 0, true, false};
        } else if (auto* load = std::get_if<LoadInst>(&inst.inst)) {
            // Skip volatile loads - they must always read from memory
            if (load->is_volatile) {
                // Volatile load: invalidate cached state for this pointer
                // since we can't assume memory hasn't changed
                mem_state.erase(load->ptr.id);
                continue;
            }

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
                                    if (inner.left.id == result)
                                        inner.left.id = stored;
                                    if (inner.right.id == result)
                                        inner.right.id = stored;
                                } else if constexpr (std::is_same_v<T, UnaryInst>) {
                                    if (inner.operand.id == result)
                                        inner.operand.id = stored;
                                } else if constexpr (std::is_same_v<T, CastInst>) {
                                    if (inner.operand.id == result)
                                        inner.operand.id = stored;
                                } else if constexpr (std::is_same_v<T, StoreInst>) {
                                    if (inner.value.id == result)
                                        inner.value.id = stored;
                                } else if constexpr (std::is_same_v<T, CallInst>) {
                                    for (auto& arg : inner.args) {
                                        if (arg.id == result)
                                            arg.id = stored;
                                    }
                                } else if constexpr (std::is_same_v<T, SelectInst>) {
                                    if (inner.condition.id == result)
                                        inner.condition.id = stored;
                                    if (inner.true_val.id == result)
                                        inner.true_val.id = stored;
                                    if (inner.false_val.id == result)
                                        inner.false_val.id = stored;
                                } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                                    if (inner.aggregate.id == result)
                                        inner.aggregate.id = stored;
                                } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                                    if (inner.aggregate.id == result)
                                        inner.aggregate.id = stored;
                                    if (inner.value.id == result)
                                        inner.value.id = stored;
                                }
                            },
                            block.instructions[j].inst);
                    }

                    // Mark this load for removal
                    dead_stores.push_back(i);
                    changed = true;
                    // Don't update mem_state - the stored value is still valid
                    // for subsequent loads
                } else if (it->second.has_load) {
                    // Redundant load: reuse previous load result
                    ValueId prev_load = it->second.loaded_value;

                    // Replace all uses of result with previous load
                    for (size_t j = i + 1; j < block.instructions.size(); ++j) {
                        std::visit(
                            [result, prev_load](auto& inner) {
                                using T = std::decay_t<decltype(inner)>;

                                if constexpr (std::is_same_v<T, BinaryInst>) {
                                    if (inner.left.id == result)
                                        inner.left.id = prev_load;
                                    if (inner.right.id == result)
                                        inner.right.id = prev_load;
                                } else if constexpr (std::is_same_v<T, UnaryInst>) {
                                    if (inner.operand.id == result)
                                        inner.operand.id = prev_load;
                                } else if constexpr (std::is_same_v<T, CastInst>) {
                                    if (inner.operand.id == result)
                                        inner.operand.id = prev_load;
                                } else if constexpr (std::is_same_v<T, StoreInst>) {
                                    if (inner.value.id == result)
                                        inner.value.id = prev_load;
                                } else if constexpr (std::is_same_v<T, CallInst>) {
                                    for (auto& arg : inner.args) {
                                        if (arg.id == result)
                                            arg.id = prev_load;
                                    }
                                } else if constexpr (std::is_same_v<T, SelectInst>) {
                                    if (inner.condition.id == result)
                                        inner.condition.id = prev_load;
                                    if (inner.true_val.id == result)
                                        inner.true_val.id = prev_load;
                                    if (inner.false_val.id == result)
                                        inner.false_val.id = prev_load;
                                } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                                    if (inner.aggregate.id == result)
                                        inner.aggregate.id = prev_load;
                                } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                                    if (inner.aggregate.id == result)
                                        inner.aggregate.id = prev_load;
                                    if (inner.value.id == result)
                                        inner.value.id = prev_load;
                                }
                            },
                            block.instructions[j].inst);
                    }

                    dead_stores.push_back(i);
                    changed = true;
                    // Don't update mem_state - keep the previous load's value
                    // so subsequent redundant loads also get forwarded to original
                } else {
                    // No previous store or load for this pointer - record this load
                    mem_state[ptr] = MemState{0, result, false, true};
                }
            } else {
                // First load from this pointer - record it
                mem_state[ptr] = MemState{0, result, false, true};
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
    // Same pointer always aliases
    if (ptr1 == ptr2) {
        return true;
    }

    // Use alias analysis if available
    if (alias_analysis_) {
        auto result = alias_analysis_->alias(ptr1, ptr2);
        // NoAlias means definitely don't alias
        return result != AliasResult::NoAlias;
    }

    // Conservative: assume everything may alias
    return true;
}

auto LoadStoreOptPass::invalidate_aliasing(std::unordered_map<ValueId, MemState>& mem_state,
                                           ValueId store_ptr) -> void {
    if (!alias_analysis_) {
        // Without alias analysis, must invalidate everything
        mem_state.clear();
        return;
    }

    // With alias analysis, only invalidate entries that may alias with store_ptr
    std::vector<ValueId> to_invalidate;
    for (const auto& [ptr, _] : mem_state) {
        if (ptr != store_ptr && may_alias(ptr, store_ptr)) {
            to_invalidate.push_back(ptr);
        }
    }

    for (ValueId ptr : to_invalidate) {
        mem_state.erase(ptr);
    }
}

auto LoadStoreOptPass::invalidate_all(std::unordered_map<ValueId, MemState>& mem_state) -> void {
    mem_state.clear();
}

} // namespace tml::mir
