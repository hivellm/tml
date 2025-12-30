// Unreachable Code Elimination Implementation
//
// This pass removes basic blocks that are not reachable from the entry block.
// It uses a simple DFS traversal from the entry block to compute reachability.

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

auto UnreachableCodeEliminationPass::remove_dead_instructions_after_terminator(Function& /*func*/)
    -> bool {
    // This is actually not needed in MIR because terminators are separate from instructions
    // In LLVM IR, a terminator is also an instruction, but in MIR they are stored separately
    // So there can never be instructions after a terminator
    return false;
}

} // namespace tml::mir
