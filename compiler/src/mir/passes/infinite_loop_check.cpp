TML_MODULE("compiler")

//! # Infinite Loop Detection Pass
//!
//! Static analysis pass to detect potential infinite loops.

#include "mir/passes/infinite_loop_check.hpp"

#include "log/log.hpp"

#include <algorithm>
#include <queue>

namespace tml::mir {

auto InfiniteLoopCheckPass::run(Module& module) -> bool {
    warnings_.clear();

    for (const auto& func : module.functions) {
        analyze_function(func);
    }

    return false; // This pass doesn't modify the IR
}

void InfiniteLoopCheckPass::print_warnings() const {
    for (const auto& warning : warnings_) {
        TML_LOG_WARN("mir", "potential infinite loop in function '"
                                << warning.function_name << "' at block '" << warning.block_name
                                << "' (id=" << warning.block_id << "): " << warning.reason);
    }
}

void InfiniteLoopCheckPass::analyze_function(const Function& func) {
    // Find all loop headers (blocks with back-edges)
    for (const auto& block : func.blocks) {
        if (is_loop_header(func, block)) {
            auto loop_blocks = get_loop_blocks(func, block.id);

            // Check 1: Loop with no exit (no break/return/branch outside loop)
            if (!loop_has_exit(func, block.id, loop_blocks)) {
                // Check if condition is always true
                if (is_condition_always_true(func, block)) {
                    warnings_.push_back({func.name, block.name, block.id,
                                         "loop condition is always true with no exit path"});
                }
                // Check if loop variables are never modified
                else if (!loop_modifies_condition_vars(func, block, loop_blocks)) {
                    warnings_.push_back(
                        {func.name, block.name, block.id,
                         "loop condition variables are never modified inside the loop"});
                }
            }
        }
    }
}

auto InfiniteLoopCheckPass::is_loop_header(const Function& func, const BasicBlock& block) -> bool {
    // A block is a loop header if any of its predecessors has a higher index
    // (i.e., comes after it in the CFG - a back-edge)
    const BasicBlock* current = &block;
    size_t current_idx = 0;

    // Find current block index
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        if (func.blocks[i].id == block.id) {
            current_idx = i;
            break;
        }
    }

    // Check if any predecessor is a back-edge (comes after current block)
    for (uint32_t pred_id : current->predecessors) {
        for (size_t i = 0; i < func.blocks.size(); ++i) {
            if (func.blocks[i].id == pred_id && i > current_idx) {
                return true; // Back-edge found
            }
        }
    }

    return false;
}

auto InfiniteLoopCheckPass::get_loop_blocks(const Function& func, uint32_t header_id)
    -> std::unordered_set<uint32_t> {
    std::unordered_set<uint32_t> loop_blocks;
    loop_blocks.insert(header_id);

    // Find all blocks that can reach the header (natural loop)
    std::queue<uint32_t> worklist;

    // Start from blocks that have header as successor
    for (const auto& block : func.blocks) {
        for (uint32_t succ : block.successors) {
            if (succ == header_id && block.id != header_id) {
                // This is a back-edge source
                worklist.push(block.id);
                loop_blocks.insert(block.id);
            }
        }
    }

    // Work backwards to find all blocks in the loop
    while (!worklist.empty()) {
        uint32_t block_id = worklist.front();
        worklist.pop();

        const BasicBlock* block = find_block(func, block_id);
        if (!block)
            continue;

        for (uint32_t pred_id : block->predecessors) {
            if (loop_blocks.find(pred_id) == loop_blocks.end()) {
                loop_blocks.insert(pred_id);
                worklist.push(pred_id);
            }
        }
    }

    return loop_blocks;
}

auto InfiniteLoopCheckPass::loop_has_exit(const Function& func, uint32_t /*header_id*/,
                                          const std::unordered_set<uint32_t>& loop_blocks) -> bool {
    for (uint32_t block_id : loop_blocks) {
        const BasicBlock* block = find_block(func, block_id);
        if (!block || !block->terminator)
            continue;

        // Check if terminator is a return
        if (std::holds_alternative<ReturnTerm>(*block->terminator)) {
            return true;
        }

        // Check successors - if any successor is outside the loop, we have an exit
        auto successors = get_successors(*block->terminator);
        for (uint32_t succ : successors) {
            if (loop_blocks.find(succ) == loop_blocks.end()) {
                return true; // Exit found
            }
        }
    }

    return false;
}

auto InfiniteLoopCheckPass::is_condition_always_true(const Function& func, const BasicBlock& header)
    -> bool {
    if (!header.terminator)
        return false;

    // Check for unconditional branch back to self or loop body
    if (auto* branch = std::get_if<BranchTerm>(&*header.terminator)) {
        (void)branch; // BranchTerm detected - check loop structure
        // Unconditional loop - check if it loops back
        for (uint32_t pred : header.predecessors) {
            if (pred == header.id || find_block(func, pred) != nullptr) {
                // Self-loop or loop structure detected
                return true;
            }
        }
    }

    // Check for conditional branch with constant true condition
    if (auto* cond_branch = std::get_if<CondBranchTerm>(&*header.terminator)) {
        // Look for the condition value
        for (const auto& inst : header.instructions) {
            if (inst.result == cond_branch->condition.id) {
                if (auto* constant = std::get_if<ConstantInst>(&inst.inst)) {
                    // Check if it's a constant true
                    if (auto* bool_val = std::get_if<ConstBool>(&constant->value)) {
                        return bool_val->value;
                    }
                    if (auto* int_val = std::get_if<ConstInt>(&constant->value)) {
                        return int_val->value != 0;
                    }
                }
            }
        }
    }

    return false;
}

auto InfiniteLoopCheckPass::loop_modifies_condition_vars(
    const Function& func, const BasicBlock& header, const std::unordered_set<uint32_t>& loop_blocks)
    -> bool {
    if (!header.terminator)
        return true; // Assume modified if we can't analyze

    // Get the condition value for conditional branches
    ValueId condition_id = 0;

    if (auto* cond_branch = std::get_if<CondBranchTerm>(&*header.terminator)) {
        condition_id = cond_branch->condition.id;
    } else {
        return true; // Not a conditional loop, can't analyze
    }

    // Collect all values that the condition depends on
    std::unordered_set<ValueId> condition_deps;
    std::queue<ValueId> worklist;
    worklist.push(condition_id);

    while (!worklist.empty()) {
        ValueId val_id = worklist.front();
        worklist.pop();

        if (condition_deps.find(val_id) != condition_deps.end())
            continue;
        condition_deps.insert(val_id);

        // Find the instruction that defines this value
        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (inst.result == val_id) {
                    // Add operands to worklist
                    std::visit(
                        [&](const auto& i) {
                            using T = std::decay_t<decltype(i)>;
                            if constexpr (std::is_same_v<T, BinaryInst>) {
                                worklist.push(i.left.id);
                                worklist.push(i.right.id);
                            } else if constexpr (std::is_same_v<T, UnaryInst>) {
                                worklist.push(i.operand.id);
                            } else if constexpr (std::is_same_v<T, LoadInst>) {
                                worklist.push(i.ptr.id);
                            } else if constexpr (std::is_same_v<T, PhiInst>) {
                                for (const auto& [val, _] : i.incoming) {
                                    worklist.push(val.id);
                                }
                            }
                        },
                        inst.inst);
                }
            }
        }
    }

    // Check if any value in the condition dependencies is modified in the loop
    for (uint32_t block_id : loop_blocks) {
        const BasicBlock* block = find_block(func, block_id);
        if (!block)
            continue;

        for (const auto& inst : block->instructions) {
            // Check for stores to addresses that condition depends on
            if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
                if (condition_deps.find(store->ptr.id) != condition_deps.end()) {
                    return true; // Condition variable is modified
                }
            }

            // Check for phi nodes (indicate SSA value changes across iterations)
            if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                (void)phi; // PhiInst detected
                if (condition_deps.find(inst.result) != condition_deps.end()) {
                    return true; // Condition uses a phi, which changes each iteration
                }
            }
        }
    }

    return false;
}

auto InfiniteLoopCheckPass::get_successors(const Terminator& term) -> std::vector<uint32_t> {
    std::vector<uint32_t> successors;

    std::visit(
        [&](const auto& t) {
            using T = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<T, BranchTerm>) {
                successors.push_back(t.target);
            } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                successors.push_back(t.true_block);
                successors.push_back(t.false_block);
            } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                for (const auto& [_, target] : t.cases) {
                    successors.push_back(target);
                }
                successors.push_back(t.default_block);
            }
            // ReturnTerm and UnreachableTerm have no successors
        },
        term);

    return successors;
}

auto InfiniteLoopCheckPass::find_block(const Function& func, uint32_t id) -> const BasicBlock* {
    for (const auto& block : func.blocks) {
        if (block.id == id) {
            return &block;
        }
    }
    return nullptr;
}

} // namespace tml::mir
