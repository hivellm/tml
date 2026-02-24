TML_MODULE("compiler")

//! # Unreachable Code Elimination (UCE) Pass
//!
//! This pass removes basic blocks not reachable from the entry block.
//!
//! ## Algorithm
//!
//! 1. Simplify constant conditional branches
//! 2. BFS/DFS from entry block to find reachable blocks
//! 3. Remove unreachable blocks
//! 4. Update predecessor lists and phi nodes
//!
//! ## Constant Branch Simplification
//!
//! ```text
//! br %const_true, bb1, bb2   →   br bb1
//! br %const_false, bb1, bb2  →   br bb2
//! ```
//!
//! This may make additional blocks unreachable.
//!
//! ## Cleanup Steps
//!
//! After removing blocks:
//! - Remove unreachable predecessors from remaining blocks
//! - Remove phi incoming edges from deleted blocks

#include "mir/passes/unreachable_code_elimination.hpp"

#include <algorithm>
#include <queue>

namespace tml::mir {

auto UnreachableCodeEliminationPass::run_on_function(Function& func) -> bool {
    if (func.blocks.empty()) {
        return false;
    }

    bool changed = false;

    // First, simplify constant conditional branches
    changed |= simplify_constant_branches(func);

    // Propagate unreachable status through branches.
    // If a branch target is a trivially unreachable block, simplify the branch.
    changed |= propagate_unreachable(func);

    // Compute reachable blocks
    auto reachable = compute_reachable_blocks(func);

    // Remove unreachable blocks
    changed |= remove_unreachable_blocks(func, reachable);

    return changed;
}

auto UnreachableCodeEliminationPass::compute_reachable_blocks(const Function& func)
    -> std::unordered_set<uint32_t> {
    std::unordered_set<uint32_t> reachable;

    if (func.blocks.empty()) {
        return reachable;
    }

    // BFS from entry block
    std::queue<uint32_t> worklist;
    worklist.push(func.blocks[0].id);
    reachable.insert(func.blocks[0].id);

    while (!worklist.empty()) {
        uint32_t block_id = worklist.front();
        worklist.pop();

        // Find the block
        const BasicBlock* block = nullptr;
        for (const auto& b : func.blocks) {
            if (b.id == block_id) {
                block = &b;
                break;
            }
        }

        if (!block || !block->terminator.has_value()) {
            continue;
        }

        // Get successors from terminator
        std::vector<uint32_t> successors;

        std::visit(
            [&successors](const auto& term) {
                using T = std::decay_t<decltype(term)>;

                if constexpr (std::is_same_v<T, BranchTerm>) {
                    successors.push_back(term.target);
                } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                    successors.push_back(term.true_block);
                    successors.push_back(term.false_block);
                } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                    for (const auto& [_, target] : term.cases) {
                        successors.push_back(target);
                    }
                    successors.push_back(term.default_block);
                }
                // ReturnTerm and UnreachableTerm have no successors
            },
            *block->terminator);

        // Add unvisited successors to worklist
        for (uint32_t succ : successors) {
            if (reachable.find(succ) == reachable.end()) {
                reachable.insert(succ);
                worklist.push(succ);
            }
        }
    }

    return reachable;
}

auto UnreachableCodeEliminationPass::remove_unreachable_blocks(
    Function& func, const std::unordered_set<uint32_t>& reachable) -> bool {
    size_t original_count = func.blocks.size();

    // Remove unreachable blocks
    func.blocks.erase(std::remove_if(func.blocks.begin(), func.blocks.end(),
                                     [&reachable](const BasicBlock& block) {
                                         return reachable.find(block.id) == reachable.end();
                                     }),
                      func.blocks.end());

    // Update predecessor lists for remaining blocks
    for (auto& block : func.blocks) {
        block.predecessors.erase(std::remove_if(block.predecessors.begin(),
                                                block.predecessors.end(),
                                                [&reachable](uint32_t pred) {
                                                    return reachable.find(pred) == reachable.end();
                                                }),
                                 block.predecessors.end());

        // Also update phi nodes to remove entries from removed blocks
        for (auto& inst : block.instructions) {
            if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                phi->incoming.erase(
                    std::remove_if(phi->incoming.begin(), phi->incoming.end(),
                                   [&reachable](const std::pair<Value, uint32_t>& entry) {
                                       return reachable.find(entry.second) == reachable.end();
                                   }),
                    phi->incoming.end());
            }
        }
    }

    return func.blocks.size() < original_count;
}

auto UnreachableCodeEliminationPass::simplify_constant_branches(Function& func) -> bool {
    bool changed = false;

    for (auto& block : func.blocks) {
        if (!block.terminator.has_value()) {
            continue;
        }

        // Check for conditional branch with constant condition
        if (auto* cond_br = std::get_if<CondBranchTerm>(&*block.terminator)) {
            // Look up the condition value
            std::optional<bool> const_cond;

            for (const auto& inst : block.instructions) {
                if (inst.result == cond_br->condition.id) {
                    const_cond = get_constant_bool(inst.inst);
                    break;
                }
            }

            // Also check other blocks for the condition definition
            if (!const_cond.has_value()) {
                for (const auto& other_block : func.blocks) {
                    for (const auto& inst : other_block.instructions) {
                        if (inst.result == cond_br->condition.id) {
                            const_cond = get_constant_bool(inst.inst);
                            break;
                        }
                    }
                    if (const_cond.has_value())
                        break;
                }
            }

            if (const_cond.has_value()) {
                // Replace conditional branch with unconditional branch
                uint32_t target = *const_cond ? cond_br->true_block : cond_br->false_block;
                block.terminator = BranchTerm{target};
                changed = true;
            }
        }
    }

    return changed;
}

auto UnreachableCodeEliminationPass::is_unreachable_block(const BasicBlock& block) const -> bool {
    if (!block.terminator.has_value()) {
        return false;
    }

    // Block must terminate with UnreachableTerm
    if (!std::holds_alternative<UnreachableTerm>(*block.terminator)) {
        return false;
    }

    // All instructions must be side-effect-free for safe removal
    for (const auto& inst : block.instructions) {
        if (has_side_effects(inst.inst)) {
            return false;
        }
    }

    return true;
}

auto UnreachableCodeEliminationPass::propagate_unreachable(Function& func) -> bool {
    bool changed = false;

    // Build a set of unreachable block IDs for fast lookup
    std::unordered_set<uint32_t> unreachable_blocks;
    for (const auto& block : func.blocks) {
        if (is_unreachable_block(block)) {
            unreachable_blocks.insert(block.id);
        }
    }

    if (unreachable_blocks.empty()) {
        return false;
    }

    for (auto& block : func.blocks) {
        if (!block.terminator.has_value()) {
            continue;
        }

        // CondBranch: if one target is unreachable, redirect to the other
        if (auto* cond_br = std::get_if<CondBranchTerm>(&*block.terminator)) {
            bool true_unreachable = unreachable_blocks.count(cond_br->true_block) > 0;
            bool false_unreachable = unreachable_blocks.count(cond_br->false_block) > 0;

            if (true_unreachable && false_unreachable) {
                // Both targets unreachable — this block is also unreachable
                block.terminator = UnreachableTerm{};
                changed = true;
            } else if (true_unreachable) {
                // True branch unreachable — always go to false branch
                block.terminator = BranchTerm{cond_br->false_block};
                changed = true;
            } else if (false_unreachable) {
                // False branch unreachable — always go to true branch
                block.terminator = BranchTerm{cond_br->true_block};
                changed = true;
            }
        }

        // Switch: remove cases targeting unreachable blocks
        if (auto* sw = std::get_if<SwitchTerm>(&*block.terminator)) {
            // Remove cases that target unreachable blocks
            auto orig_size = sw->cases.size();
            sw->cases.erase(std::remove_if(sw->cases.begin(), sw->cases.end(),
                                           [&unreachable_blocks](const auto& c) {
                                               return unreachable_blocks.count(c.second) > 0;
                                           }),
                            sw->cases.end());

            if (sw->cases.size() < orig_size) {
                changed = true;
            }

            // If default is unreachable and we have cases left, pick first case as default
            if (unreachable_blocks.count(sw->default_block) > 0 && !sw->cases.empty()) {
                sw->default_block = sw->cases[0].second;
                changed = true;
            }

            // If no cases left and default is unreachable, mark block as unreachable
            if (sw->cases.empty() && unreachable_blocks.count(sw->default_block) > 0) {
                block.terminator = UnreachableTerm{};
                changed = true;
            }

            // If only one case left and it matches default, simplify to unconditional branch
            if (sw->cases.empty()) {
                block.terminator = BranchTerm{sw->default_block};
                changed = true;
            }
        }

        // Unconditional branch to unreachable block: propagate unreachable
        if (auto* br = std::get_if<BranchTerm>(&*block.terminator)) {
            if (unreachable_blocks.count(br->target) > 0) {
                block.terminator = UnreachableTerm{};
                changed = true;
            }
        }
    }

    return changed;
}

} // namespace tml::mir
