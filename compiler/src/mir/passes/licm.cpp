//! # Loop Invariant Code Motion (LICM) Pass
//!
//! Moves loop-invariant computations out of loops.

#include "mir/passes/licm.hpp"

#include <algorithm>
#include <queue>

namespace tml::mir {

auto LICMPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    auto loops = find_loops(func);

    for (auto& loop : loops) {
        if (hoist_invariants(func, loop)) {
            changed = true;
        }
    }

    return changed;
}

auto LICMPass::find_loops(const Function& func) -> std::vector<Loop> {
    std::vector<Loop> loops;

    auto back_edges = find_back_edges(func);

    for (const auto& [latch, header] : back_edges) {
        loops.push_back(build_loop(func, header, latch));
    }

    return loops;
}

auto LICMPass::find_back_edges(const Function& func) -> std::vector<std::pair<uint32_t, uint32_t>> {
    std::vector<std::pair<uint32_t, uint32_t>> back_edges;

    // Simple heuristic: a back edge is when a branch goes to an earlier block
    // (assuming blocks are roughly in dominance order)
    std::unordered_map<uint32_t, size_t> block_order;
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        block_order[func.blocks[i].id] = i;
    }

    for (const auto& block : func.blocks) {
        if (!block.terminator)
            continue;

        std::visit(
            [&](const auto& t) {
                using T = std::decay_t<decltype(t)>;

                if constexpr (std::is_same_v<T, BranchTerm>) {
                    if (block_order[t.target] <= block_order[block.id]) {
                        back_edges.emplace_back(block.id, t.target);
                    }
                } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                    if (block_order[t.true_block] <= block_order[block.id]) {
                        back_edges.emplace_back(block.id, t.true_block);
                    }
                    if (block_order[t.false_block] <= block_order[block.id]) {
                        back_edges.emplace_back(block.id, t.false_block);
                    }
                }
            },
            *block.terminator);
    }

    return back_edges;
}

auto LICMPass::build_loop(const Function& func, uint32_t header, uint32_t latch) -> Loop {
    Loop loop;
    loop.header_id = header;
    loop.blocks.insert(header);

    // Build loop body using reverse CFG traversal from latch to header
    std::queue<uint32_t> worklist;
    worklist.push(latch);

    while (!worklist.empty()) {
        uint32_t block_id = worklist.front();
        worklist.pop();

        if (loop.blocks.count(block_id) > 0) {
            continue;
        }

        loop.blocks.insert(block_id);

        // Add predecessors
        int idx = get_block_index(func, block_id);
        if (idx >= 0) {
            for (uint32_t pred : func.blocks[static_cast<size_t>(idx)].predecessors) {
                if (loop.blocks.count(pred) == 0) {
                    worklist.push(pred);
                }
            }
        }
    }

    // Find exit blocks (blocks in loop with successors outside loop)
    for (uint32_t block_id : loop.blocks) {
        int idx = get_block_index(func, block_id);
        if (idx < 0)
            continue;

        const auto& block = func.blocks[static_cast<size_t>(idx)];
        for (uint32_t succ : block.successors) {
            if (loop.blocks.count(succ) == 0) {
                loop.exit_blocks.insert(block_id);
                break;
            }
        }
    }

    return loop;
}

auto LICMPass::is_loop_invariant(const Function& func, const InstructionData& inst,
                                 const Loop& loop,
                                 const std::unordered_set<ValueId>& invariant_values) -> bool {
    // Check if all operands are either:
    // 1. Constants
    // 2. Defined outside the loop
    // 3. Already marked as loop-invariant

    return std::visit(
        [&](const auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;

            auto check_operand = [&](ValueId val) -> bool {
                // Check if it's a constant or defined outside loop
                if (!is_defined_in_loop(func, val, loop)) {
                    return true;
                }
                // Check if it's already marked as invariant
                return invariant_values.count(val) > 0;
            };

            if constexpr (std::is_same_v<T, BinaryInst>) {
                return check_operand(i.left.id) && check_operand(i.right.id);
            } else if constexpr (std::is_same_v<T, UnaryInst>) {
                return check_operand(i.operand.id);
            } else if constexpr (std::is_same_v<T, CastInst>) {
                return check_operand(i.operand.id);
            } else if constexpr (std::is_same_v<T, ConstantInst>) {
                return true; // Constants are always invariant
            } else if constexpr (std::is_same_v<T, SelectInst>) {
                return check_operand(i.condition.id) && check_operand(i.true_val.id) &&
                       check_operand(i.false_val.id);
            } else {
                // Other instructions (loads, stores, calls) are not safe to hoist
                return false;
            }
        },
        inst.inst);
}

auto LICMPass::can_hoist(const InstructionData& inst) -> bool {
    return std::visit(
        [](const auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;
            (void)i;

            // Only pure, side-effect-free instructions can be hoisted
            if constexpr (std::is_same_v<T, BinaryInst> || std::is_same_v<T, UnaryInst> ||
                          std::is_same_v<T, CastInst> || std::is_same_v<T, ConstantInst> ||
                          std::is_same_v<T, SelectInst>) {
                return true;
            } else {
                return false;
            }
        },
        inst.inst);
}

auto LICMPass::hoist_invariants(Function& func, Loop& loop) -> bool {
    bool changed = false;

    // Find or create preheader
    uint32_t preheader_id = get_or_create_preheader(func, loop);
    if (preheader_id == UINT32_MAX) {
        return false; // Couldn't create preheader
    }

    int preheader_idx = get_block_index(func, preheader_id);
    if (preheader_idx < 0) {
        return false;
    }

    // Iteratively find and hoist invariant instructions
    std::unordered_set<ValueId> invariant_values;
    bool made_progress = true;

    while (made_progress) {
        made_progress = false;

        for (uint32_t block_id : loop.blocks) {
            int block_idx = get_block_index(func, block_id);
            if (block_idx < 0)
                continue;

            auto& block = func.blocks[static_cast<size_t>(block_idx)];
            std::vector<size_t> to_hoist;

            for (size_t i = 0; i < block.instructions.size(); ++i) {
                auto& inst = block.instructions[i];

                // Skip if already marked as invariant
                if (invariant_values.count(inst.result) > 0) {
                    continue;
                }

                if (!can_hoist(inst)) {
                    continue;
                }

                if (is_loop_invariant(func, inst, loop, invariant_values)) {
                    invariant_values.insert(inst.result);
                    to_hoist.push_back(i);
                    made_progress = true;
                }
            }

            // Hoist instructions (in reverse order to maintain indices)
            auto& preheader = func.blocks[static_cast<size_t>(preheader_idx)];
            for (auto it = to_hoist.rbegin(); it != to_hoist.rend(); ++it) {
                // Insert before terminator in preheader
                auto inst = block.instructions[*it];
                preheader.instructions.push_back(inst);

                // Remove from loop block
                block.instructions.erase(block.instructions.begin() +
                                         static_cast<std::ptrdiff_t>(*it));
                changed = true;
            }
        }
    }

    return changed;
}

auto LICMPass::get_or_create_preheader(Function& func, Loop& loop) -> uint32_t {
    // Find the unique predecessor of the header that's not in the loop
    int header_idx = get_block_index(func, loop.header_id);
    if (header_idx < 0) {
        return UINT32_MAX;
    }

    const auto& header = func.blocks[static_cast<size_t>(header_idx)];

    uint32_t preheader_id = UINT32_MAX;
    int outside_preds = 0;

    for (uint32_t pred : header.predecessors) {
        if (loop.blocks.count(pred) == 0) {
            preheader_id = pred;
            outside_preds++;
        }
    }

    // If there's exactly one predecessor outside the loop, use it as preheader
    if (outside_preds == 1) {
        loop.preheader_id = preheader_id;
        return preheader_id;
    }

    // Multiple outside predecessors - would need to create a new preheader
    // For simplicity, we don't create new blocks here
    return UINT32_MAX;
}

auto LICMPass::is_defined_in_loop(const Function& func, ValueId value, const Loop& loop) -> bool {
    // Check if value is defined by an instruction in the loop
    for (uint32_t block_id : loop.blocks) {
        int idx = get_block_index(func, block_id);
        if (idx < 0)
            continue;

        const auto& block = func.blocks[static_cast<size_t>(idx)];
        for (const auto& inst : block.instructions) {
            if (inst.result == value) {
                return true;
            }
        }
    }

    return false;
}

auto LICMPass::get_block_index(const Function& func, uint32_t block_id) -> int {
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        if (func.blocks[i].id == block_id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace tml::mir
