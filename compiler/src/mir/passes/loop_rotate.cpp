//! # Loop Rotation Pass
//!
//! Transforms loops to expose more optimization opportunities.

#include "mir/passes/loop_rotate.hpp"

#include <algorithm>
#include <queue>

namespace tml::mir {

auto LoopRotatePass::run_on_function(Function& func) -> bool {
    bool changed = false;

    // Find all loops
    auto loops = find_loops(func);

    // Try to rotate each loop
    for (const auto& loop : loops) {
        if (is_rotatable(func, loop)) {
            if (rotate_loop(func, loop)) {
                changed = true;
            }
        }
    }

    return changed;
}

auto LoopRotatePass::find_loops(const Function& func) -> std::vector<LoopInfo> {
    std::vector<LoopInfo> loops;

    // Simple loop detection: find back edges (edges to blocks earlier in the CFG)
    // A back edge indicates a loop

    for (size_t i = 0; i < func.blocks.size(); ++i) {
        const auto& block = func.blocks[i];

        for (uint32_t succ : block.successors) {
            // Find successor index
            int succ_idx = get_block_index(func, succ);
            if (succ_idx < 0)
                continue;

            // Back edge if successor comes before current block (or is the same)
            if (static_cast<size_t>(succ_idx) <= i) {
                // Found a potential loop
                LoopInfo loop;
                loop.header = succ;
                loop.latch = block.id;

                // Collect loop blocks using reverse DFS from latch to header
                std::unordered_set<uint32_t> visited;
                std::queue<uint32_t> worklist;

                visited.insert(loop.header);
                worklist.push(block.id);

                while (!worklist.empty()) {
                    uint32_t curr = worklist.front();
                    worklist.pop();

                    if (visited.count(curr))
                        continue;
                    visited.insert(curr);

                    // Add predecessors
                    int curr_idx = get_block_index(func, curr);
                    if (curr_idx >= 0) {
                        for (uint32_t pred :
                             func.blocks[static_cast<size_t>(curr_idx)].predecessors) {
                            if (!visited.count(pred)) {
                                worklist.push(pred);
                            }
                        }
                    }
                }

                loop.blocks = std::move(visited);
                loops.push_back(std::move(loop));
            }
        }
    }

    return loops;
}

auto LoopRotatePass::is_rotatable(const Function& func, const LoopInfo& loop) -> bool {
    // A loop is rotatable if:
    // 1. The header has a conditional branch
    // 2. One branch exits the loop
    // 3. The loop is simple (single latch)

    int header_idx = get_block_index(func, loop.header);
    if (header_idx < 0)
        return false;

    const auto& header = func.blocks[static_cast<size_t>(header_idx)];

    // Check for conditional branch
    if (!header.terminator)
        return false;

    auto* cond = std::get_if<CondBranchTerm>(&*header.terminator);
    if (!cond)
        return false;

    // Check that one successor is outside the loop (exit)
    bool has_exit = false;
    bool has_body = false;

    if (loop.blocks.count(cond->true_block) == 0) {
        has_exit = true;
    } else {
        has_body = true;
    }

    if (loop.blocks.count(cond->false_block) == 0) {
        has_exit = true;
    } else {
        has_body = true;
    }

    return has_exit && has_body;
}

auto LoopRotatePass::rotate_loop(Function& func, const LoopInfo& loop) -> bool {
    // For now, this is a placeholder - full loop rotation is complex
    // It requires:
    // 1. Duplicating the header condition as a pre-loop check
    // 2. Moving the condition to the end of the loop
    // 3. Updating phi nodes
    // 4. Fixing up the CFG

    // This is a simplified version that just verifies the loop structure
    // Full implementation would require significant CFG manipulation

    int header_idx = get_block_index(func, loop.header);
    if (header_idx < 0)
        return false;

    const auto& header = func.blocks[static_cast<size_t>(header_idx)];

    // Only rotate very simple loops for safety
    // Header should have only the condition check
    if (header.instructions.size() > 3) {
        return false; // Too complex to rotate safely
    }

    // For safety, don't actually rotate in this implementation
    // Just mark that we could have
    (void)header;

    return false;
}

auto LoopRotatePass::get_block_index(const Function& func, uint32_t id) -> int {
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        if (func.blocks[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace tml::mir
