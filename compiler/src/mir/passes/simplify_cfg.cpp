//! # Control Flow Graph Simplification Pass
//!
//! Simplifies CFG by merging blocks, removing empty jumps, and
//! eliminating unreachable code.

#include "mir/passes/simplify_cfg.hpp"

#include <algorithm>

namespace tml::mir {

auto SimplifyCfgPass::run_on_function(Function& func) -> bool {
    if (func.blocks.empty()) {
        return false;
    }

    bool changed = false;
    bool made_progress = true;

    // Iterate until no more changes (passes may enable each other)
    while (made_progress) {
        made_progress = false;

        // Order matters: simplify branches first, then merge, then remove empty
        made_progress |= simplify_constant_branches(func);
        made_progress |= merge_blocks(func);
        made_progress |= remove_empty_blocks(func);
        made_progress |= remove_unreachable_blocks(func);
        made_progress |= cleanup_phi_nodes(func);

        changed |= made_progress;
    }

    return changed;
}

auto SimplifyCfgPass::build_predecessor_map(const Function& func)
    -> std::unordered_map<uint32_t, std::vector<uint32_t>> {
    std::unordered_map<uint32_t, std::vector<uint32_t>> preds;

    // Initialize all blocks with empty predecessor lists
    for (const auto& block : func.blocks) {
        preds[block.id] = {};
    }

    // Build predecessor lists from terminators
    for (const auto& block : func.blocks) {
        if (!block.terminator) {
            continue;
        }

        std::visit(
            [&preds, &block](const auto& term) {
                using T = std::decay_t<decltype(term)>;

                if constexpr (std::is_same_v<T, BranchTerm>) {
                    preds[term.target].push_back(block.id);
                } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                    preds[term.true_block].push_back(block.id);
                    if (term.false_block != term.true_block) {
                        preds[term.false_block].push_back(block.id);
                    }
                } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                    preds[term.default_block].push_back(block.id);
                    for (const auto& [_, target] : term.cases) {
                        if (target != term.default_block) {
                            preds[target].push_back(block.id);
                        }
                    }
                }
                // ReturnTerm and UnreachableTerm have no successors
            },
            *block.terminator);
    }

    return preds;
}

auto SimplifyCfgPass::build_successor_map(const Function& func)
    -> std::unordered_map<uint32_t, std::vector<uint32_t>> {
    std::unordered_map<uint32_t, std::vector<uint32_t>> succs;

    for (const auto& block : func.blocks) {
        succs[block.id] = {};

        if (!block.terminator) {
            continue;
        }

        std::visit(
            [&succs, &block](const auto& term) {
                using T = std::decay_t<decltype(term)>;

                if constexpr (std::is_same_v<T, BranchTerm>) {
                    succs[block.id].push_back(term.target);
                } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                    succs[block.id].push_back(term.true_block);
                    if (term.false_block != term.true_block) {
                        succs[block.id].push_back(term.false_block);
                    }
                } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                    succs[block.id].push_back(term.default_block);
                    for (const auto& [_, target] : term.cases) {
                        succs[block.id].push_back(target);
                    }
                }
            },
            *block.terminator);
    }

    return succs;
}

auto SimplifyCfgPass::get_block_index(const Function& func, uint32_t block_id) -> int {
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        if (func.blocks[i].id == block_id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void SimplifyCfgPass::redirect_block_references(Function& func, uint32_t old_id, uint32_t new_id) {
    for (auto& block : func.blocks) {
        if (!block.terminator) {
            continue;
        }

        std::visit(
            [old_id, new_id](auto& term) {
                using T = std::decay_t<decltype(term)>;

                if constexpr (std::is_same_v<T, BranchTerm>) {
                    if (term.target == old_id) {
                        term.target = new_id;
                    }
                } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                    if (term.true_block == old_id) {
                        term.true_block = new_id;
                    }
                    if (term.false_block == old_id) {
                        term.false_block = new_id;
                    }
                } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                    if (term.default_block == old_id) {
                        term.default_block = new_id;
                    }
                    for (auto& [_, target] : term.cases) {
                        if (target == old_id) {
                            target = new_id;
                        }
                    }
                }
            },
            *block.terminator);

        // Also update phi nodes
        for (auto& inst : block.instructions) {
            if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                for (auto& [val, from_block] : phi->incoming) {
                    if (from_block == old_id) {
                        from_block = new_id;
                    }
                }
            }
        }
    }
}

auto SimplifyCfgPass::merge_blocks(Function& func) -> bool {
    bool changed = false;
    auto preds = build_predecessor_map(func);
    auto succs = build_successor_map(func);

    // Find blocks that can be merged: B has single predecessor A,
    // and A has single successor B, and B is not the entry block
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        auto& block_a = func.blocks[i];

        // Skip if A doesn't have exactly one successor
        auto& a_succs = succs[block_a.id];
        if (a_succs.size() != 1) {
            continue;
        }

        // A's terminator must be unconditional branch
        if (!block_a.terminator) {
            continue;
        }
        auto* branch = std::get_if<BranchTerm>(&*block_a.terminator);
        if (!branch) {
            continue;
        }

        uint32_t block_b_id = branch->target;

        // B must have exactly one predecessor (A)
        auto& b_preds = preds[block_b_id];
        if (b_preds.size() != 1 || b_preds[0] != block_a.id) {
            continue;
        }

        // Don't merge if B is the entry block (index 0)
        int block_b_idx = get_block_index(func, block_b_id);
        if (block_b_idx <= 0) {
            continue;
        }

        // Don't merge a block with itself
        if (block_a.id == block_b_id) {
            continue;
        }

        auto& block_b = func.blocks[static_cast<size_t>(block_b_idx)];

        // B should not have phi nodes (since it has single predecessor)
        bool has_phi = false;
        for (const auto& inst : block_b.instructions) {
            if (std::holds_alternative<PhiInst>(inst.inst)) {
                has_phi = true;
                break;
            }
        }
        if (has_phi) {
            continue;
        }

        // Merge: append B's instructions to A, take B's terminator
        for (auto& inst : block_b.instructions) {
            block_a.instructions.push_back(std::move(inst));
        }
        block_a.terminator = block_b.terminator;

        // Update references from B to A
        redirect_block_references(func, block_b_id, block_a.id);

        // Mark B for removal by clearing it
        block_b.instructions.clear();
        block_b.terminator = std::nullopt;

        changed = true;

        // Rebuild maps after modification
        preds = build_predecessor_map(func);
        succs = build_successor_map(func);

        // Restart from beginning since indices may have changed conceptually
        i = static_cast<size_t>(-1); // Will be incremented to 0
    }

    // Remove empty blocks
    func.blocks.erase(std::remove_if(func.blocks.begin(), func.blocks.end(),
                                     [](const BasicBlock& b) {
                                         return b.instructions.empty() && !b.terminator.has_value();
                                     }),
                      func.blocks.end());

    return changed;
}

auto SimplifyCfgPass::remove_empty_blocks(Function& func) -> bool {
    bool changed = false;

    // Build predecessor map to know who jumps to each block
    auto preds = build_predecessor_map(func);

    // Find blocks that only contain an unconditional branch
    for (size_t i = 1; i < func.blocks.size(); ++i) { // Skip entry block
        auto& block = func.blocks[i];

        // Must be empty (no instructions)
        if (!block.instructions.empty()) {
            continue;
        }

        // If block has a terminator (unconditional branch), redirect to its target
        if (block.terminator) {
            auto* branch = std::get_if<BranchTerm>(&*block.terminator);
            if (!branch) {
                continue;
            }

            // Don't remove if it branches to itself
            if (branch->target == block.id) {
                continue;
            }

            uint32_t target_id = branch->target;
            uint32_t removed_id = block.id;

            // Get predecessors of the block being removed
            auto& block_preds = preds[removed_id];

            // Update terminators in predecessors to jump directly to target
            for (auto& other_block : func.blocks) {
                if (!other_block.terminator) {
                    continue;
                }

                std::visit(
                    [removed_id, target_id](auto& term) {
                        using T = std::decay_t<decltype(term)>;

                        if constexpr (std::is_same_v<T, BranchTerm>) {
                            if (term.target == removed_id) {
                                term.target = target_id;
                            }
                        } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                            if (term.true_block == removed_id) {
                                term.true_block = target_id;
                            }
                            if (term.false_block == removed_id) {
                                term.false_block = target_id;
                            }
                        } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                            if (term.default_block == removed_id) {
                                term.default_block = target_id;
                            }
                            for (auto& [_, t] : term.cases) {
                                if (t == removed_id) {
                                    t = target_id;
                                }
                            }
                        }
                    },
                    *other_block.terminator);
            }

            // Update PHI nodes in target block: replace entries from removed_id
            // with entries from each predecessor of removed_id
            int target_idx = get_block_index(func, target_id);
            if (target_idx >= 0) {
                auto& target_block = func.blocks[static_cast<size_t>(target_idx)];
                for (auto& inst : target_block.instructions) {
                    if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                        // Find and replace entries from removed_id
                        std::vector<std::pair<Value, uint32_t>> new_incoming;
                        for (const auto& [val, from_block] : phi->incoming) {
                            if (from_block == removed_id) {
                                // Replace with entries from each predecessor of removed block
                                for (uint32_t pred_id : block_preds) {
                                    new_incoming.push_back({val, pred_id});
                                }
                            } else {
                                new_incoming.push_back({val, from_block});
                            }
                        }
                        phi->incoming = std::move(new_incoming);
                    }
                }
            }

            // Mark for removal
            block.terminator = std::nullopt;
            changed = true;

            // Rebuild predecessor map after changes
            preds = build_predecessor_map(func);
        } else {
            // Block has no terminator - this is a dangling block, skip it
            // (will be removed by the cleanup below)
        }
    }

    // Remove marked blocks
    func.blocks.erase(std::remove_if(func.blocks.begin(), func.blocks.end(),
                                     [](const BasicBlock& b) {
                                         return b.instructions.empty() && !b.terminator.has_value();
                                     }),
                      func.blocks.end());

    return changed;
}

auto SimplifyCfgPass::simplify_constant_branches(Function& func) -> bool {
    bool changed = false;

    for (auto& block : func.blocks) {
        if (!block.terminator) {
            continue;
        }

        auto* cond_branch = std::get_if<CondBranchTerm>(&*block.terminator);
        if (!cond_branch) {
            continue;
        }

        // Check if condition is a constant
        std::optional<bool> const_cond;

        // Look for the instruction that defines the condition
        for (const auto& inst : block.instructions) {
            if (inst.result == cond_branch->condition.id) {
                if (auto* ci = std::get_if<ConstantInst>(&inst.inst)) {
                    if (auto* bool_val = std::get_if<ConstBool>(&ci->value)) {
                        const_cond = bool_val->value;
                    }
                }
                break;
            }
        }

        // Also check if both branches go to the same block
        if (cond_branch->true_block == cond_branch->false_block) {
            block.terminator = BranchTerm{cond_branch->true_block};
            changed = true;
            continue;
        }

        if (const_cond.has_value()) {
            // Determine which branch is taken and which is removed
            uint32_t target = *const_cond ? cond_branch->true_block : cond_branch->false_block;
            uint32_t removed_target =
                *const_cond ? cond_branch->false_block : cond_branch->true_block;

            // Remove PHI entries in the removed target that come from this block
            int removed_idx = get_block_index(func, removed_target);
            if (removed_idx >= 0) {
                auto& removed_block = func.blocks[static_cast<size_t>(removed_idx)];
                for (auto& inst : removed_block.instructions) {
                    if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                        phi->incoming.erase(std::remove_if(phi->incoming.begin(),
                                                           phi->incoming.end(),
                                                           [&block](const auto& entry) {
                                                               return entry.second == block.id;
                                                           }),
                                            phi->incoming.end());
                    }
                }
            }

            // Replace with unconditional branch
            block.terminator = BranchTerm{target};
            changed = true;
        }
    }

    return changed;
}

auto SimplifyCfgPass::remove_unreachable_blocks(Function& func) -> bool {
    if (func.blocks.empty()) {
        return false;
    }

    // BFS from entry block to find reachable blocks
    std::unordered_set<uint32_t> reachable;
    std::vector<uint32_t> worklist;

    uint32_t entry_id = func.blocks[0].id;
    worklist.push_back(entry_id);
    reachable.insert(entry_id);

    while (!worklist.empty()) {
        uint32_t block_id = worklist.back();
        worklist.pop_back();

        int idx = get_block_index(func, block_id);
        if (idx < 0) {
            continue;
        }

        const auto& block = func.blocks[static_cast<size_t>(idx)];
        if (!block.terminator) {
            continue;
        }

        std::visit(
            [&reachable, &worklist](const auto& term) {
                using T = std::decay_t<decltype(term)>;

                if constexpr (std::is_same_v<T, BranchTerm>) {
                    if (reachable.insert(term.target).second) {
                        worklist.push_back(term.target);
                    }
                } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                    if (reachable.insert(term.true_block).second) {
                        worklist.push_back(term.true_block);
                    }
                    if (reachable.insert(term.false_block).second) {
                        worklist.push_back(term.false_block);
                    }
                } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                    if (reachable.insert(term.default_block).second) {
                        worklist.push_back(term.default_block);
                    }
                    for (const auto& [_, target] : term.cases) {
                        if (reachable.insert(target).second) {
                            worklist.push_back(target);
                        }
                    }
                }
            },
            *block.terminator);
    }

    // Collect IDs of unreachable blocks
    std::unordered_set<uint32_t> unreachable_ids;
    for (const auto& block : func.blocks) {
        if (reachable.find(block.id) == reachable.end()) {
            unreachable_ids.insert(block.id);
        }
    }

    if (unreachable_ids.empty()) {
        return false;
    }

    // Clean up PHI nodes in reachable blocks: remove entries from unreachable blocks
    for (auto& block : func.blocks) {
        if (reachable.find(block.id) == reachable.end()) {
            continue; // Skip unreachable blocks
        }

        for (auto& inst : block.instructions) {
            if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                // Remove entries from unreachable predecessors
                phi->incoming.erase(std::remove_if(phi->incoming.begin(), phi->incoming.end(),
                                                   [&unreachable_ids](const auto& entry) {
                                                       return unreachable_ids.find(entry.second) !=
                                                              unreachable_ids.end();
                                                   }),
                                    phi->incoming.end());
            }
        }
    }

    // Remove unreachable blocks
    size_t original_size = func.blocks.size();
    func.blocks.erase(std::remove_if(func.blocks.begin(), func.blocks.end(),
                                     [&reachable](const BasicBlock& b) {
                                         return reachable.find(b.id) == reachable.end();
                                     }),
                      func.blocks.end());

    return func.blocks.size() < original_size;
}

auto SimplifyCfgPass::cleanup_phi_nodes(Function& func) -> bool {
    bool changed = false;

    // IMPORTANT: Collect ALL replacements from ALL blocks first,
    // then apply them across the ENTIRE function.
    // This fixes a bug where single-incoming phis were removed but uses
    // in OTHER blocks were not updated, leaving undefined value references.

    // First pass: collect all single-incoming phi replacements
    std::unordered_map<ValueId, Value> all_replacements;
    std::vector<std::pair<size_t, std::vector<size_t>>>
        blocks_to_clean; // block index -> instruction indices

    for (size_t block_idx = 0; block_idx < func.blocks.size(); ++block_idx) {
        auto& block = func.blocks[block_idx];
        std::vector<size_t> to_remove;

        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto* phi = std::get_if<PhiInst>(&block.instructions[i].inst);
            if (!phi) {
                continue;
            }

            if (phi->incoming.empty()) {
                // Empty PHI - mark for removal
                to_remove.push_back(i);
                changed = true;
            } else if (phi->incoming.size() == 1) {
                // Single incoming value - replace uses with that value
                all_replacements[block.instructions[i].result] = phi->incoming[0].first;
                to_remove.push_back(i);
                changed = true;
            }
        }

        if (!to_remove.empty()) {
            blocks_to_clean.push_back({block_idx, std::move(to_remove)});
        }
    }

    // Second pass: apply replacements across ALL blocks
    if (!all_replacements.empty()) {
        auto replace_value = [&all_replacements](Value& v) {
            auto it = all_replacements.find(v.id);
            if (it != all_replacements.end()) {
                v = it->second;
            }
        };

        for (auto& block : func.blocks) {
            for (auto& inst : block.instructions) {
                std::visit(
                    [&replace_value](auto& i) {
                        using T = std::decay_t<decltype(i)>;

                        if constexpr (std::is_same_v<T, BinaryInst>) {
                            replace_value(i.left);
                            replace_value(i.right);
                        } else if constexpr (std::is_same_v<T, UnaryInst>) {
                            replace_value(i.operand);
                        } else if constexpr (std::is_same_v<T, LoadInst>) {
                            replace_value(i.ptr);
                        } else if constexpr (std::is_same_v<T, StoreInst>) {
                            replace_value(i.ptr);
                            replace_value(i.value);
                        } else if constexpr (std::is_same_v<T, CallInst>) {
                            for (auto& arg : i.args) {
                                replace_value(arg);
                            }
                        } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                            replace_value(i.receiver);
                            for (auto& arg : i.args) {
                                replace_value(arg);
                            }
                        } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                            replace_value(i.base);
                            for (auto& idx : i.indices) {
                                replace_value(idx);
                            }
                        } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                            replace_value(i.aggregate);
                        } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                            replace_value(i.aggregate);
                            replace_value(i.value);
                        } else if constexpr (std::is_same_v<T, CastInst>) {
                            replace_value(i.operand);
                        } else if constexpr (std::is_same_v<T, PhiInst>) {
                            for (auto& [val, _] : i.incoming) {
                                replace_value(val);
                            }
                        } else if constexpr (std::is_same_v<T, SelectInst>) {
                            replace_value(i.condition);
                            replace_value(i.true_val);
                            replace_value(i.false_val);
                        } else if constexpr (std::is_same_v<T, StructInitInst>) {
                            for (auto& field : i.fields) {
                                replace_value(field);
                            }
                        } else if constexpr (std::is_same_v<T, EnumInitInst>) {
                            for (auto& p : i.payload) {
                                replace_value(p);
                            }
                        } else if constexpr (std::is_same_v<T, TupleInitInst>) {
                            for (auto& elem : i.elements) {
                                replace_value(elem);
                            }
                        } else if constexpr (std::is_same_v<T, ArrayInitInst>) {
                            for (auto& elem : i.elements) {
                                replace_value(elem);
                            }
                        }
                    },
                    inst.inst);
            }

            // Also replace in terminator
            if (block.terminator) {
                std::visit(
                    [&replace_value](auto& term) {
                        using T = std::decay_t<decltype(term)>;

                        if constexpr (std::is_same_v<T, ReturnTerm>) {
                            if (term.value) {
                                replace_value(*term.value);
                            }
                        } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                            replace_value(term.condition);
                        } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                            replace_value(term.discriminant);
                        }
                    },
                    *block.terminator);
            }
        }
    }

    // Third pass: remove the marked phi instructions (in reverse order within each block)
    for (auto& [block_idx, to_remove] : blocks_to_clean) {
        auto& block = func.blocks[block_idx];
        for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
            block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(*it));
        }
    }

    return changed;
}

} // namespace tml::mir
