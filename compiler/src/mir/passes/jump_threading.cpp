TML_MODULE("compiler")

//! # Jump Threading Optimization Pass
//!
//! Threads jumps through blocks when conditions are known from incoming edges.

#include "mir/passes/jump_threading.hpp"

namespace tml::mir {

auto JumpThreadingPass::run_on_function(Function& func) -> bool {
    bool changed = false;
    bool made_progress = true;

    // Limit iterations to prevent infinite loops
    int max_iterations = 10;
    int iteration = 0;

    while (made_progress && iteration < max_iterations) {
        made_progress = false;
        iteration++;

        for (auto& block : func.blocks) {
            if (thread_branch(func, block)) {
                made_progress = true;
                changed = true;
            }
        }
    }

    return changed;
}

auto JumpThreadingPass::thread_branch(Function& func, BasicBlock& block) -> bool {
    if (!block.terminator) {
        return false;
    }

    auto* cond_branch = std::get_if<CondBranchTerm>(&*block.terminator);
    if (!cond_branch) {
        return false;
    }

    ValueId our_cond = cond_branch->condition.id;
    bool changed = false;

    // Check true branch target
    int true_idx = get_block_index(func, cond_branch->true_block);
    if (true_idx >= 0) {
        auto& true_block = func.blocks[static_cast<size_t>(true_idx)];

        // If the true target only has an unconditional branch, thread through
        if (true_block.instructions.empty() && true_block.terminator) {
            if (auto* target_branch = std::get_if<BranchTerm>(&*true_block.terminator)) {
                cond_branch->true_block = target_branch->target;
                changed = true;
            }
        }

        // If true target has a conditional branch on the same condition,
        // we know the condition is true, so thread to the true branch
        if (true_block.terminator) {
            if (auto* target_cond = std::get_if<CondBranchTerm>(&*true_block.terminator)) {
                if (target_cond->condition.id == our_cond && true_block.instructions.empty()) {
                    // Same condition, we came from true branch, so condition is true
                    cond_branch->true_block = target_cond->true_block;
                    changed = true;
                }
            }
        }
    }

    // Check false branch target
    int false_idx = get_block_index(func, cond_branch->false_block);
    if (false_idx >= 0) {
        auto& false_block = func.blocks[static_cast<size_t>(false_idx)];

        // If the false target only has an unconditional branch, thread through
        if (false_block.instructions.empty() && false_block.terminator) {
            if (auto* target_branch = std::get_if<BranchTerm>(&*false_block.terminator)) {
                cond_branch->false_block = target_branch->target;
                changed = true;
            }
        }

        // If false target has a conditional branch on the same condition,
        // we know the condition is false, so thread to the false branch
        if (false_block.terminator) {
            if (auto* target_cond = std::get_if<CondBranchTerm>(&*false_block.terminator)) {
                if (target_cond->condition.id == our_cond && false_block.instructions.empty()) {
                    // Same condition, we came from false branch, so condition is false
                    cond_branch->false_block = target_cond->false_block;
                    changed = true;
                }
            }
        }
    }

    return changed;
}

auto JumpThreadingPass::get_branch_condition(const BasicBlock& block) -> std::optional<ValueId> {
    if (!block.terminator) {
        return std::nullopt;
    }

    if (auto* cond = std::get_if<CondBranchTerm>(&*block.terminator)) {
        return cond->condition.id;
    }

    return std::nullopt;
}

auto JumpThreadingPass::get_block_index(const Function& func, uint32_t block_id) -> int {
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        if (func.blocks[i].id == block_id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace tml::mir
