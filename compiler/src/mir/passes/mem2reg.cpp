//! # Mem2Reg Pass (Memory to Register Promotion)
//!
//! Promotes stack allocations to SSA registers.

#include "mir/passes/mem2reg.hpp"

#include <algorithm>

namespace tml::mir {

auto Mem2RegPass::run_on_function(Function& func) -> bool {
    bool changed = false;
    bool made_progress = true;

    // Iterate until no more progress (promotions may enable further promotions)
    while (made_progress) {
        made_progress = false;

        auto allocas = collect_allocas(func);

        for (auto& info : allocas) {
            // Try simple single-store promotion first
            if (info.stores.size() == 1) {
                if (promote_single_store(func, info)) {
                    made_progress = true;
                    changed = true;
                    break; // Re-collect after modification
                }
            }
            // For multiple stores, we'd need phi nodes (more complex)
            // For now, skip these cases
        }
    }

    return changed;
}

auto Mem2RegPass::collect_allocas(Function& func) -> std::vector<AllocaInfo> {
    std::vector<AllocaInfo> allocas;

    // Find all allocas in entry block
    if (func.blocks.empty()) {
        return allocas;
    }

    auto& entry = func.blocks[0];
    for (size_t i = 0; i < entry.instructions.size(); ++i) {
        auto& inst = entry.instructions[i];
        if (auto* alloca = std::get_if<AllocaInst>(&inst.inst)) {
            AllocaInfo info;
            info.alloca_id = inst.result;
            info.alloc_type = alloca->alloc_type;
            info.name = alloca->name;
            info.block_idx = 0;
            info.inst_idx = i;

            if (is_promotable(func, inst.result, info)) {
                allocas.push_back(std::move(info));
            }
        }
    }

    return allocas;
}

auto Mem2RegPass::is_promotable(const Function& func, ValueId alloca_id,
                                 AllocaInfo& info) -> bool {
    // Check all uses of the alloca
    for (size_t b = 0; b < func.blocks.size(); ++b) {
        const auto& block = func.blocks[b];
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            const auto& inst = block.instructions[i];

            bool used = std::visit(
                [&](const auto& inner) -> bool {
                    using T = std::decay_t<decltype(inner)>;

                    if constexpr (std::is_same_v<T, LoadInst>) {
                        if (inner.ptr.id == alloca_id) {
                            info.loads.emplace_back(b, i, inst.result);
                            info.use_blocks.insert(block.id);
                            return true;
                        }
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        if (inner.ptr.id == alloca_id) {
                            info.stores.emplace_back(b, i, inner.value.id);
                            info.def_blocks.insert(block.id);
                            return true;
                        }
                        // If alloca is used as a value (not ptr), can't promote
                        if (inner.value.id == alloca_id) {
                            return false; // Mark as not promotable
                        }
                    } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                        // Address taken via GEP - can't promote
                        if (inner.base.id == alloca_id) {
                            return false;
                        }
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        // Passed to function - can't promote
                        for (const auto& arg : inner.args) {
                            if (arg.id == alloca_id) {
                                return false;
                            }
                        }
                    } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                        if (inner.receiver.id == alloca_id) {
                            return false;
                        }
                        for (const auto& arg : inner.args) {
                            if (arg.id == alloca_id) {
                                return false;
                            }
                        }
                    }

                    return true; // Continue checking
                },
                inst.inst);

            if (!used) {
                return false; // Address was taken
            }
        }
    }

    // Must have at least one store to be useful
    return !info.stores.empty();
}

auto Mem2RegPass::promote_single_store(Function& func, AllocaInfo& info) -> bool {
    if (info.stores.size() != 1) {
        return false;
    }

    auto [store_block, store_idx, stored_value] = info.stores[0];

    // For simple promotion: the store must be in the entry block
    // and must come before all loads (or dominate them)
    if (store_block != 0) {
        return false; // Store not in entry block
    }

    // Check that the store comes before all loads in the same block
    // For cross-block loads, the entry block dominates all blocks
    for (const auto& [load_block, load_idx, load_result] : info.loads) {
        if (load_block == store_block && load_idx < store_idx) {
            // Load before store in same block - can't promote simply
            return false;
        }
    }

    // Replace all loads with the stored value
    for (const auto& [load_block, load_idx, load_result] : info.loads) {
        replace_value(func, load_result, stored_value);
    }

    // Collect indices to remove (loads, store, alloca) - sorted in reverse order
    std::vector<std::pair<size_t, size_t>> to_remove;

    // Add loads
    for (const auto& [load_block, load_idx, _] : info.loads) {
        to_remove.emplace_back(load_block, load_idx);
    }

    // Add store
    to_remove.emplace_back(store_block, store_idx);

    // Add alloca
    to_remove.emplace_back(info.block_idx, info.inst_idx);

    // Sort by block then by index (descending for safe removal)
    std::sort(to_remove.begin(), to_remove.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first)
            return a.first > b.first;
        return a.second > b.second;
    });

    // Remove instructions
    for (const auto& [block_idx, inst_idx] : to_remove) {
        auto& block = func.blocks[block_idx];
        if (inst_idx < block.instructions.size()) {
            block.instructions.erase(block.instructions.begin() +
                                     static_cast<std::ptrdiff_t>(inst_idx));
        }
    }

    return true;
}

auto Mem2RegPass::promote_with_phi(Function& /*func*/, AllocaInfo& /*info*/) -> bool {
    // Full phi-node insertion algorithm (Cytron et al.)
    // This is more complex and requires dominance frontier computation
    // For now, we only handle the simple single-store case
    return false;
}

auto Mem2RegPass::replace_value(Function& func, ValueId old_value, ValueId new_value) -> void {
    for (auto& block : func.blocks) {
        for (auto& inst : block.instructions) {
            std::visit(
                [old_value, new_value](auto& i) {
                    using T = std::decay_t<decltype(i)>;

                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        if (i.left.id == old_value)
                            i.left.id = new_value;
                        if (i.right.id == old_value)
                            i.right.id = new_value;
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        if (i.operand.id == old_value)
                            i.operand.id = new_value;
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        if (i.ptr.id == old_value)
                            i.ptr.id = new_value;
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        if (i.ptr.id == old_value)
                            i.ptr.id = new_value;
                        if (i.value.id == old_value)
                            i.value.id = new_value;
                    } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                        if (i.base.id == old_value)
                            i.base.id = new_value;
                        for (auto& idx : i.indices) {
                            if (idx.id == old_value)
                                idx.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                        if (i.aggregate.id == old_value)
                            i.aggregate.id = new_value;
                    } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                        if (i.aggregate.id == old_value)
                            i.aggregate.id = new_value;
                        if (i.value.id == old_value)
                            i.value.id = new_value;
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (auto& arg : i.args) {
                            if (arg.id == old_value)
                                arg.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                        if (i.receiver.id == old_value)
                            i.receiver.id = new_value;
                        for (auto& arg : i.args) {
                            if (arg.id == old_value)
                                arg.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, CastInst>) {
                        if (i.operand.id == old_value)
                            i.operand.id = new_value;
                    } else if constexpr (std::is_same_v<T, PhiInst>) {
                        for (auto& [val, _] : i.incoming) {
                            if (val.id == old_value)
                                val.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        if (i.condition.id == old_value)
                            i.condition.id = new_value;
                        if (i.true_val.id == old_value)
                            i.true_val.id = new_value;
                        if (i.false_val.id == old_value)
                            i.false_val.id = new_value;
                    } else if constexpr (std::is_same_v<T, StructInitInst>) {
                        for (auto& field : i.fields) {
                            if (field.id == old_value)
                                field.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, EnumInitInst>) {
                        for (auto& p : i.payload) {
                            if (p.id == old_value)
                                p.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, TupleInitInst>) {
                        for (auto& elem : i.elements) {
                            if (elem.id == old_value)
                                elem.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, ArrayInitInst>) {
                        for (auto& elem : i.elements) {
                            if (elem.id == old_value)
                                elem.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, AwaitInst>) {
                        if (i.poll_value.id == old_value)
                            i.poll_value.id = new_value;
                    } else if constexpr (std::is_same_v<T, ClosureInitInst>) {
                        for (auto& cap : i.captures) {
                            if (cap.second.id == old_value)
                                cap.second.id = new_value;
                        }
                    }
                },
                inst.inst);
        }

        // Update terminators
        if (block.terminator) {
            std::visit(
                [old_value, new_value](auto& t) {
                    using T = std::decay_t<decltype(t)>;

                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (t.value && t.value->id == old_value) {
                            t.value->id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        if (t.condition.id == old_value)
                            t.condition.id = new_value;
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        if (t.discriminant.id == old_value)
                            t.discriminant.id = new_value;
                    }
                },
                *block.terminator);
        }
    }
}

auto Mem2RegPass::remove_instruction(BasicBlock& block, size_t idx) -> void {
    if (idx < block.instructions.size()) {
        block.instructions.erase(block.instructions.begin() +
                                 static_cast<std::ptrdiff_t>(idx));
    }
}

} // namespace tml::mir
