//! # Loop Unrolling Pass
//!
//! Unrolls small loops with known trip counts.

#include "mir/passes/loop_unroll.hpp"

#include <queue>

namespace tml::mir {

auto LoopUnrollPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    auto loops = find_unrollable_loops(func);

    for (const auto& loop : loops) {
        if (!is_loop_body_small(func, loop)) {
            continue;
        }

        int64_t trip_count = 0;
        if (loop.is_increment) {
            trip_count = (loop.end_value - loop.start_value + loop.step - 1) / loop.step;
        } else {
            trip_count = (loop.start_value - loop.end_value - loop.step - 1) / (-loop.step);
        }

        if (trip_count <= 0 || trip_count > options_.max_trip_count) {
            continue;
        }

        if (trip_count <= options_.max_full_unroll_count) {
            if (fully_unroll(func, loop)) {
                changed = true;
            }
        }
        // Partial unrolling would go here for larger loops
    }

    return changed;
}

auto LoopUnrollPass::find_unrollable_loops(const Function& func) -> std::vector<LoopInfo> {
    std::vector<LoopInfo> loops;

    auto back_edges = find_back_edges(func);

    for (const auto& [latch, header] : back_edges) {
        auto info = analyze_loop(func, header, latch);
        if (info) {
            loops.push_back(*info);
        }
    }

    return loops;
}

auto LoopUnrollPass::analyze_loop(const Function& func, uint32_t header, uint32_t latch)
    -> std::optional<LoopInfo> {
    // This is a simplified analysis that looks for canonical loop patterns
    // A full implementation would use scalar evolution analysis

    const auto* header_block = get_block(func, header);
    const auto* latch_block = get_block(func, latch);

    if (!header_block || !latch_block) {
        return std::nullopt;
    }

    // Look for a phi node that represents the induction variable
    ValueId induction_var = INVALID_VALUE;
    int64_t start_value = 0;

    for (const auto& inst : header_block->instructions) {
        if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
            // Check if this looks like an induction variable
            // (has exactly 2 incoming values, one from outside loop, one from latch)
            if (phi->incoming.size() == 2) {
                for (const auto& [val, block_id] : phi->incoming) {
                    if (block_id != latch) {
                        // This is the initial value
                        // Try to find if it's a constant
                        for (const auto& blk : func.blocks) {
                            for (const auto& def_inst : blk.instructions) {
                                if (def_inst.result == val.id) {
                                    if (auto* ci = std::get_if<ConstantInst>(&def_inst.inst)) {
                                        if (auto* int_val = std::get_if<ConstInt>(&ci->value)) {
                                            induction_var = inst.result;
                                            start_value = int_val->value;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (induction_var == INVALID_VALUE) {
        return std::nullopt;
    }

    // Look for the loop bound check in the header's terminator
    if (!header_block->terminator) {
        return std::nullopt;
    }

    int64_t end_value = 0;
    bool found_bound = false;

    if (auto* cond_br = std::get_if<CondBranchTerm>(&*header_block->terminator)) {
        // Find the comparison instruction
        for (const auto& inst : header_block->instructions) {
            if (inst.result == cond_br->condition.id) {
                if (auto* bin = std::get_if<BinaryInst>(&inst.inst)) {
                    if (bin->op == BinOp::Lt || bin->op == BinOp::Le || bin->op == BinOp::Gt ||
                        bin->op == BinOp::Ge) {
                        // Check if comparing against induction var
                        bool left_is_iv = bin->left.id == induction_var;
                        bool right_is_iv = bin->right.id == induction_var;

                        if (left_is_iv || right_is_iv) {
                            ValueId bound_id = left_is_iv ? bin->right.id : bin->left.id;

                            // Find the bound constant
                            for (const auto& blk : func.blocks) {
                                for (const auto& def_inst : blk.instructions) {
                                    if (def_inst.result == bound_id) {
                                        if (auto* ci = std::get_if<ConstantInst>(&def_inst.inst)) {
                                            if (auto* int_val = std::get_if<ConstInt>(&ci->value)) {
                                                end_value = int_val->value;
                                                found_bound = true;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (!found_bound) {
        return std::nullopt;
    }

    // Look for increment in the latch
    int64_t step = 0;
    bool is_increment = true;

    for (const auto& inst : latch_block->instructions) {
        if (auto* bin = std::get_if<BinaryInst>(&inst.inst)) {
            if ((bin->op == BinOp::Add || bin->op == BinOp::Sub) && bin->left.id == induction_var) {
                // Find the step constant
                for (const auto& blk : func.blocks) {
                    for (const auto& def_inst : blk.instructions) {
                        if (def_inst.result == bin->right.id) {
                            if (auto* ci = std::get_if<ConstantInst>(&def_inst.inst)) {
                                if (auto* int_val = std::get_if<ConstInt>(&ci->value)) {
                                    step = int_val->value;
                                    is_increment = (bin->op == BinOp::Add);
                                    if (!is_increment) {
                                        step = -step;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (step == 0) {
        return std::nullopt;
    }

    // Build loop body block set
    std::unordered_set<uint32_t> body_blocks;
    body_blocks.insert(header);

    std::queue<uint32_t> worklist;
    worklist.push(latch);

    while (!worklist.empty()) {
        uint32_t block_id = worklist.front();
        worklist.pop();

        if (body_blocks.count(block_id) > 0) {
            continue;
        }

        body_blocks.insert(block_id);

        int idx = get_block_index(func, block_id);
        if (idx >= 0) {
            for (uint32_t pred : func.blocks[static_cast<size_t>(idx)].predecessors) {
                if (body_blocks.count(pred) == 0) {
                    worklist.push(pred);
                }
            }
        }
    }

    LoopInfo info;
    info.header_id = header;
    info.latch_id = latch;
    info.body_blocks = body_blocks;
    info.induction_var = induction_var;
    info.start_value = start_value;
    info.end_value = end_value;
    info.step = step;
    info.is_increment = is_increment;

    return info;
}

auto LoopUnrollPass::is_loop_body_small(const Function& func, const LoopInfo& loop) -> bool {
    int total_instructions = 0;

    for (uint32_t block_id : loop.body_blocks) {
        const auto* block = get_block(func, block_id);
        if (block) {
            total_instructions += static_cast<int>(block->instructions.size());
        }
    }

    return total_instructions <= options_.max_loop_body_size;
}

auto LoopUnrollPass::fully_unroll(Function& func, const LoopInfo& loop) -> bool {
    // Full loop unrolling is complex - it requires:
    // 1. Cloning the loop body for each iteration
    // 2. Replacing induction variable uses with constants
    // 3. Removing the loop control flow
    // 4. Connecting the cloned bodies sequentially

    // For now, we just mark this as a candidate for unrolling
    // The actual transformation would be done by a more sophisticated pass
    // or by LLVM's loop unroller in the backend

    (void)func;
    (void)loop;

    // TODO: Implement full loop unrolling
    // This is a placeholder that signals the loop was analyzed
    return false;
}

auto LoopUnrollPass::find_back_edges(const Function& func)
    -> std::vector<std::pair<uint32_t, uint32_t>> {
    std::vector<std::pair<uint32_t, uint32_t>> back_edges;

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

auto LoopUnrollPass::get_block(const Function& func, uint32_t id) -> const BasicBlock* {
    for (const auto& block : func.blocks) {
        if (block.id == id) {
            return &block;
        }
    }
    return nullptr;
}

auto LoopUnrollPass::get_block_mut(Function& func, uint32_t id) -> BasicBlock* {
    for (auto& block : func.blocks) {
        if (block.id == id) {
            return &block;
        }
    }
    return nullptr;
}

auto LoopUnrollPass::get_block_index(const Function& func, uint32_t id) -> int {
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        if (func.blocks[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace tml::mir
