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

    // Find blocks that only contain an unconditional branch
    for (size_t i = 1; i < func.blocks.size(); ++i) { // Skip entry block
        auto& block = func.blocks[i];

        // Must be empty (no instructions) and have unconditional branch
        if (!block.instructions.empty()) {
            continue;
        }
        if (!block.terminator) {
            continue;
        }

        auto* branch = std::get_if<BranchTerm>(&*block.terminator);
        if (!branch) {
            continue;
        }

        // Don't remove if it branches to itself
        if (branch->target == block.id) {
            continue;
        }

        // Redirect all predecessors to the target
        redirect_block_references(func, block.id, branch->target);

        // Mark for removal
        block.terminator = std::nullopt;
        changed = true;
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
            // Replace with unconditional branch
            uint32_t target = *const_cond ? cond_branch->true_block : cond_branch->false_block;
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

    // Remove unreachable blocks
    size_t original_size = func.blocks.size();
    func.blocks.erase(std::remove_if(func.blocks.begin(), func.blocks.end(),
                                     [&reachable](const BasicBlock& b) {
                                         return reachable.find(b.id) == reachable.end();
                                     }),
                      func.blocks.end());

    return func.blocks.size() < original_size;
}

} // namespace tml::mir
