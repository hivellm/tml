//! # Basic Block Merging Pass
//!
//! Merges consecutive basic blocks when possible.

#include "mir/passes/block_merge.hpp"

#include <algorithm>

namespace tml::mir {

auto BlockMergePass::run_on_function(Function& func) -> bool {
    bool changed = false;
    bool made_progress = true;

    while (made_progress) {
        made_progress = false;

        for (size_t i = 0; i < func.blocks.size(); ++i) {
            auto& block = func.blocks[i];

            // Check if this block has exactly one successor via unconditional branch
            if (!block.terminator)
                continue;

            auto* branch = std::get_if<BranchTerm>(&*block.terminator);
            if (!branch)
                continue;

            // Find the successor block
            int succ_idx = get_block_index(func, branch->target);
            if (succ_idx < 0)
                continue;

            auto& succ = func.blocks[static_cast<size_t>(succ_idx)];

            if (can_merge(func, block, succ)) {
                merge_blocks(func, i, static_cast<size_t>(succ_idx));
                made_progress = true;
                changed = true;
                break; // Restart iteration after modification
            }
        }
    }

    return changed;
}

auto BlockMergePass::can_merge(const Function& func, const BasicBlock& pred, const BasicBlock& succ)
    -> bool {
    // Pred must have exactly one successor
    if (pred.successors.size() != 1) {
        return false;
    }

    // Succ must have exactly one predecessor
    if (succ.predecessors.size() != 1) {
        return false;
    }

    // The single predecessor of succ must be pred
    if (succ.predecessors[0] != pred.id) {
        return false;
    }

    // The single successor of pred must be succ
    if (pred.successors[0] != succ.id) {
        return false;
    }

    // Pred's terminator must be an unconditional branch
    if (!pred.terminator) {
        return false;
    }

    if (!std::holds_alternative<BranchTerm>(*pred.terminator)) {
        return false;
    }

    // Don't merge the entry block with its successor if the entry block is empty
    // (keeps the entry block clear for phi nodes if needed)
    if (pred.id == func.blocks[0].id && pred.instructions.empty()) {
        return false;
    }

    // Don't merge if succ has phi nodes (would need to resolve them first)
    for (const auto& inst : succ.instructions) {
        if (std::holds_alternative<PhiInst>(inst.inst)) {
            return false;
        }
    }

    (void)func; // Unused in current implementation
    return true;
}

auto BlockMergePass::merge_blocks(Function& func, size_t pred_idx, size_t succ_idx) -> void {
    auto& pred = func.blocks[pred_idx];
    auto& succ = func.blocks[succ_idx];

    // Append succ's instructions to pred
    pred.instructions.insert(pred.instructions.end(), succ.instructions.begin(),
                             succ.instructions.end());

    // Take succ's terminator
    pred.terminator = succ.terminator;

    // Update successor list
    pred.successors = succ.successors;

    // Update predecessors of succ's successors to point to pred
    for (uint32_t succ_succ_id : succ.successors) {
        int idx = get_block_index(func, succ_succ_id);
        if (idx >= 0) {
            auto& succ_succ = func.blocks[static_cast<size_t>(idx)];
            for (auto& p : succ_succ.predecessors) {
                if (p == succ.id) {
                    p = pred.id;
                }
            }
        }
    }

    // Update phi nodes in successor's successors
    update_phi_nodes(func, succ.id, pred.id);

    // Remove the successor block
    func.blocks.erase(func.blocks.begin() + static_cast<std::ptrdiff_t>(succ_idx));
}

auto BlockMergePass::update_phi_nodes(Function& func, uint32_t old_block, uint32_t new_block)
    -> void {
    for (auto& block : func.blocks) {
        for (auto& inst : block.instructions) {
            if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                for (auto& [val, block_id] : phi->incoming) {
                    if (block_id == old_block) {
                        block_id = new_block;
                    }
                }
            }
        }
    }
}

auto BlockMergePass::get_block_index(const Function& func, uint32_t id) -> int {
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        if (func.blocks[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace tml::mir
