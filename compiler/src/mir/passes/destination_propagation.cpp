TML_MODULE("compiler")

//! # Destination Propagation Pass
//!
//! Eliminates intermediate copies by finding single-use alloca temporaries
//! and propagating the original value directly to where the loaded value
//! is used.
//!
//! Pattern: alloca -> store val -> load -> use loaded
//!      =>  use val directly (remove alloca, store, load)

#include "mir/passes/destination_propagation.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

namespace {

struct AllocaUseInfo {
    ValueId alloca_id = INVALID_VALUE;
    // Store info: the instruction that stores into this alloca
    size_t store_block = 0;
    size_t store_index = 0;
    ValueId stored_value = INVALID_VALUE;
    // Load info: the instruction that loads from this alloca
    size_t load_block = 0;
    size_t load_index = 0;
    ValueId loaded_value = INVALID_VALUE;
    // Counts
    int store_count = 0;
    int load_count = 0;
    int other_use_count = 0; // GEP, call arg, etc.
    bool is_volatile = false;
};

/// Check if a value is used as an operand in any instruction or terminator.
auto count_value_uses(const Function& func, ValueId value_id) -> int {
    int count = 0;

    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            std::visit(
                [&](const auto& i) {
                    using T = std::decay_t<decltype(i)>;
                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        if (i.left.id == value_id)
                            count++;
                        if (i.right.id == value_id)
                            count++;
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        if (i.operand.id == value_id)
                            count++;
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        if (i.ptr.id == value_id)
                            count++;
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        if (i.ptr.id == value_id)
                            count++;
                        if (i.value.id == value_id)
                            count++;
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (const auto& arg : i.args) {
                            if (arg.id == value_id)
                                count++;
                        }
                    } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                        if (i.receiver.id == value_id)
                            count++;
                        for (const auto& arg : i.args) {
                            if (arg.id == value_id)
                                count++;
                        }
                    } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                        if (i.base.id == value_id)
                            count++;
                        for (const auto& idx : i.indices) {
                            if (idx.id == value_id)
                                count++;
                        }
                    } else if constexpr (std::is_same_v<T, CastInst>) {
                        if (i.operand.id == value_id)
                            count++;
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        if (i.condition.id == value_id)
                            count++;
                        if (i.true_val.id == value_id)
                            count++;
                        if (i.false_val.id == value_id)
                            count++;
                    } else if constexpr (std::is_same_v<T, PhiInst>) {
                        for (const auto& [val, _] : i.incoming) {
                            if (val.id == value_id)
                                count++;
                        }
                    }
                },
                inst.inst);
        }

        // Check terminators
        if (block.terminator.has_value()) {
            std::visit(
                [&](const auto& t) {
                    using T = std::decay_t<decltype(t)>;
                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (t.value.has_value() && t.value->id == value_id)
                            count++;
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        if (t.condition.id == value_id)
                            count++;
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        if (t.discriminant.id == value_id)
                            count++;
                    }
                },
                *block.terminator);
        }
    }

    return count;
}

/// Replace all uses of old_value with new_value in the function.
void replace_all_uses(Function& func, ValueId old_value, ValueId new_value) {
    auto replace = [&](Value& val) {
        if (val.id == old_value) {
            val.id = new_value;
        }
    };

    for (auto& block : func.blocks) {
        for (auto& inst : block.instructions) {
            std::visit(
                [&](auto& i) {
                    using T = std::decay_t<decltype(i)>;
                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        replace(i.left);
                        replace(i.right);
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        replace(i.operand);
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        replace(i.ptr);
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        replace(i.ptr);
                        replace(i.value);
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (auto& arg : i.args) {
                            replace(arg);
                        }
                    } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                        replace(i.receiver);
                        for (auto& arg : i.args) {
                            replace(arg);
                        }
                    } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                        replace(i.base);
                        for (auto& idx : i.indices) {
                            replace(idx);
                        }
                    } else if constexpr (std::is_same_v<T, CastInst>) {
                        replace(i.operand);
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        replace(i.condition);
                        replace(i.true_val);
                        replace(i.false_val);
                    } else if constexpr (std::is_same_v<T, PhiInst>) {
                        for (auto& [val, _] : i.incoming) {
                            replace(val);
                        }
                    } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                        replace(i.aggregate);
                    } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                        replace(i.aggregate);
                        replace(i.value);
                    }
                },
                inst.inst);
        }

        // Also update terminators
        if (block.terminator.has_value()) {
            std::visit(
                [&](auto& t) {
                    using T = std::decay_t<decltype(t)>;
                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (t.value.has_value()) {
                            replace(*t.value);
                        }
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        replace(t.condition);
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        replace(t.discriminant);
                    }
                },
                *block.terminator);
        }
    }
}

} // namespace

auto DestinationPropagationPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    // Collect alloca use information
    std::unordered_map<ValueId, AllocaUseInfo> alloca_info;

    // Step 1: Find all allocas
    for (size_t bi = 0; bi < func.blocks.size(); ++bi) {
        const auto& block = func.blocks[bi];
        for (size_t ii = 0; ii < block.instructions.size(); ++ii) {
            const auto& inst = block.instructions[ii];
            if (auto* alloca = std::get_if<AllocaInst>(&inst.inst)) {
                if (alloca->is_volatile) {
                    continue;
                }
                AllocaUseInfo info;
                info.alloca_id = inst.result;
                alloca_info[inst.result] = info;
            }
        }
    }

    if (alloca_info.empty()) {
        return false;
    }

    // Step 2: Scan all instructions to find stores to / loads from allocas
    for (size_t bi = 0; bi < func.blocks.size(); ++bi) {
        const auto& block = func.blocks[bi];
        for (size_t ii = 0; ii < block.instructions.size(); ++ii) {
            const auto& inst = block.instructions[ii];

            if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
                auto it = alloca_info.find(store->ptr.id);
                if (it != alloca_info.end()) {
                    it->second.store_count++;
                    it->second.store_block = bi;
                    it->second.store_index = ii;
                    it->second.stored_value = store->value.id;
                    if (store->is_volatile) {
                        it->second.is_volatile = true;
                    }
                }
                // Also check if the stored *value* is an alloca (address escapes)
                auto it2 = alloca_info.find(store->value.id);
                if (it2 != alloca_info.end()) {
                    it2->second.other_use_count++;
                }
            } else if (auto* load = std::get_if<LoadInst>(&inst.inst)) {
                auto it = alloca_info.find(load->ptr.id);
                if (it != alloca_info.end()) {
                    it->second.load_count++;
                    it->second.load_block = bi;
                    it->second.load_index = ii;
                    it->second.loaded_value = inst.result;
                    if (load->is_volatile) {
                        it->second.is_volatile = true;
                    }
                }
            } else {
                // Check if any alloca is used in a non-store/load context
                // (address escapes via call, GEP, etc.)
                std::visit(
                    [&](const auto& i) {
                        using T = std::decay_t<decltype(i)>;
                        if constexpr (std::is_same_v<T, CallInst>) {
                            for (const auto& arg : i.args) {
                                auto it = alloca_info.find(arg.id);
                                if (it != alloca_info.end()) {
                                    it->second.other_use_count++;
                                }
                            }
                        } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                            auto it = alloca_info.find(i.receiver.id);
                            if (it != alloca_info.end()) {
                                it->second.other_use_count++;
                            }
                            for (const auto& arg : i.args) {
                                auto it2 = alloca_info.find(arg.id);
                                if (it2 != alloca_info.end()) {
                                    it2->second.other_use_count++;
                                }
                            }
                        } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                            auto it = alloca_info.find(i.base.id);
                            if (it != alloca_info.end()) {
                                it->second.other_use_count++;
                            }
                        }
                    },
                    inst.inst);
            }
        }
    }

    // Step 3: Find candidates — allocas with exactly 1 store, 1 load,
    // no other uses, in the same block, store before load
    std::unordered_set<ValueId> dead_instructions;

    for (const auto& [alloca_id, info] : alloca_info) {
        if (info.is_volatile) {
            continue;
        }
        if (info.store_count != 1 || info.load_count != 1) {
            continue;
        }
        if (info.other_use_count > 0) {
            continue;
        }
        if (info.stored_value == INVALID_VALUE || info.loaded_value == INVALID_VALUE) {
            continue;
        }

        // Same block, store before load
        if (info.store_block != info.load_block) {
            continue;
        }
        if (info.store_index >= info.load_index) {
            continue;
        }

        // Don't propagate if the stored value itself is the alloca (self-reference)
        if (info.stored_value == alloca_id) {
            continue;
        }

        // Verify the stored value is not modified between the store and load
        bool safe = true;
        const auto& block = func.blocks[info.store_block];
        for (size_t i = info.store_index + 1; i < info.load_index; ++i) {
            // If any instruction between store and load redefines the stored value,
            // we can't propagate
            if (block.instructions[i].result == info.stored_value) {
                safe = false;
                break;
            }
        }

        if (!safe) {
            continue;
        }

        // Replace all uses of the loaded value with the stored value
        replace_all_uses(func, info.loaded_value, info.stored_value);

        // Mark the store, load, and alloca for removal
        dead_instructions.insert(info.loaded_value); // load result
        // We need to mark the store instruction too — stores have INVALID_VALUE result,
        // so we track by block/index below
        changed = true;
    }

    if (!changed) {
        return false;
    }

    // Step 4: Remove dead instructions (alloca, store, load)
    // Collect all alloca/store/load positions to remove
    for (auto& block : func.blocks) {
        std::vector<size_t> to_remove;

        for (size_t i = 0; i < block.instructions.size(); ++i) {
            const auto& inst = block.instructions[i];

            // Remove the alloca itself if it's a propagated candidate
            if (std::holds_alternative<AllocaInst>(inst.inst)) {
                auto it = alloca_info.find(inst.result);
                if (it != alloca_info.end() &&
                    dead_instructions.count(it->second.loaded_value) > 0) {
                    to_remove.push_back(i);
                    continue;
                }
            }

            // Remove loads whose result is in dead_instructions
            if (std::holds_alternative<LoadInst>(inst.inst) &&
                dead_instructions.count(inst.result) > 0) {
                to_remove.push_back(i);
                continue;
            }

            // Remove stores to propagated allocas
            if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
                auto it = alloca_info.find(store->ptr.id);
                if (it != alloca_info.end() &&
                    dead_instructions.count(it->second.loaded_value) > 0) {
                    to_remove.push_back(i);
                    continue;
                }
            }
        }

        // Remove in reverse order
        for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
            block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(*it));
        }
    }

    return changed;
}

} // namespace tml::mir
