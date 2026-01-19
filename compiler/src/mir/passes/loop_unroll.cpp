//! # Loop Unrolling Pass
//!
//! Unrolls small loops with known trip counts.

#include "mir/passes/loop_unroll.hpp"

#include <algorithm>
#include <queue>
#include <unordered_map>

namespace tml::mir {

auto LoopUnrollPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    auto loops = find_unrollable_loops(func);

    // Collect all loop headers to detect nesting
    std::unordered_set<uint32_t> all_loop_headers;
    for (const auto& loop : loops) {
        all_loop_headers.insert(loop.header_id);
    }

    for (const auto& loop : loops) {
        // Skip nested inner loops: if this loop's body contains another loop header,
        // this is an outer loop containing an inner loop. We should not unroll inner
        // loops because the outer loop may depend on values defined by the inner loop.
        bool is_nested_inner = false;
        for (uint32_t block_id : loop.body_blocks) {
            if (block_id != loop.header_id && all_loop_headers.count(block_id) > 0) {
                // This loop contains another loop header in its body
                // This loop is an OUTER loop, which is fine to skip
            }
        }

        // Check if this loop's header is inside another loop's body
        for (const auto& other_loop : loops) {
            if (other_loop.header_id == loop.header_id) {
                continue; // Same loop
            }
            // If this loop's header is in another loop's body, this is an inner loop
            if (other_loop.body_blocks.count(loop.header_id) > 0) {
                is_nested_inner = true;
                break;
            }
        }

        if (is_nested_inner) {
            // Don't unroll inner loops in nested scenarios - the outer loop
            // may depend on values defined by inner loop phi nodes
            continue;
        }

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
            // Check if any phi node result is used outside the loop body
            // If so, we can't safely unroll because the outer code depends
            // on the final value which would be lost
            bool phi_used_outside = false;
            const auto* header_block = get_block(func, loop.header_id);
            if (header_block) {
                for (const auto& inst : header_block->instructions) {
                    if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                        // Check if this phi result is used anywhere outside the loop
                        for (const auto& block : func.blocks) {
                            if (loop.body_blocks.count(block.id) > 0) {
                                continue; // Inside loop is fine
                            }
                            for (const auto& use_inst : block.instructions) {
                                auto check_uses_phi =
                                    [phi_result = inst.result](const InstructionData& check_inst) {
                                        if (auto* bin = std::get_if<BinaryInst>(&check_inst.inst)) {
                                            return bin->left.id == phi_result ||
                                                   bin->right.id == phi_result;
                                        }
                                        if (auto* phi = std::get_if<PhiInst>(&check_inst.inst)) {
                                            for (const auto& [val, _] : phi->incoming) {
                                                if (val.id == phi_result)
                                                    return true;
                                            }
                                        }
                                        return false;
                                    };
                                bool uses_phi = check_uses_phi(use_inst);
                                if (uses_phi) {
                                    phi_used_outside = true;
                                    break;
                                }
                            }
                            if (phi_used_outside)
                                break;
                        }
                        if (phi_used_outside)
                            break;
                    }
                }
            }

            if (phi_used_outside) {
                continue;
            }

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
    // Calculate trip count
    int64_t trip_count = 0;
    if (loop.is_increment) {
        trip_count = (loop.end_value - loop.start_value + loop.step - 1) / loop.step;
    } else {
        trip_count = (loop.start_value - loop.end_value - loop.step - 1) / (-loop.step);
    }

    if (trip_count <= 0 || trip_count > options_.max_full_unroll_count) {
        return false;
    }

    // Find exit block (the block we branch to when loop condition is false)
    uint32_t exit_block_id = UINT32_MAX;
    const auto* header = get_block(func, loop.header_id);
    if (!header || !header->terminator) {
        return false;
    }

    if (auto* cond_br = std::get_if<CondBranchTerm>(&*header->terminator)) {
        // Determine which branch is the exit
        if (loop.body_blocks.count(cond_br->true_block) == 0) {
            exit_block_id = cond_br->true_block;
        } else if (loop.body_blocks.count(cond_br->false_block) == 0) {
            exit_block_id = cond_br->false_block;
        }
    }

    if (exit_block_id == UINT32_MAX) {
        return false;
    }

    // Find the next available block and value IDs
    uint32_t next_block_id = 0;
    ValueId next_value_id = 0;
    for (const auto& block : func.blocks) {
        next_block_id = std::max(next_block_id, block.id + 1);
        for (const auto& inst : block.instructions) {
            if (inst.result != INVALID_VALUE) {
                next_value_id = std::max(next_value_id, inst.result + 1);
            }
        }
    }

    // Collect loop body blocks in order (excluding header for first iteration)
    std::vector<uint32_t> body_block_ids;
    for (uint32_t id : loop.body_blocks) {
        if (id != loop.header_id) {
            body_block_ids.push_back(id);
        }
    }

    // Sort body blocks to maintain a reasonable order
    std::sort(body_block_ids.begin(), body_block_ids.end());

    // Create unrolled blocks
    std::vector<BasicBlock> new_blocks;
    uint32_t first_unrolled_block = next_block_id;

    for (int64_t iter = 0; iter < trip_count; ++iter) {
        int64_t iter_value = loop.start_value + iter * loop.step;

        // Value remapping for this iteration
        std::unordered_map<ValueId, ValueId> value_map;

        // Map induction variable to the constant for this iteration
        // We'll create a constant instruction for this
        ValueId iter_const_id = next_value_id++;
        value_map[loop.induction_var] = iter_const_id;

        // Map block IDs for this iteration
        std::unordered_map<uint32_t, uint32_t> block_map;
        for (uint32_t old_id : body_block_ids) {
            block_map[old_id] = next_block_id++;
        }
        // Also map header and latch
        block_map[loop.header_id] = next_block_id++;
        block_map[loop.latch_id] =
            block_map.count(loop.latch_id) > 0 ? block_map[loop.latch_id] : next_block_id++;

        // Create block for iteration constant
        BasicBlock const_block;
        const_block.id = next_block_id++;
        const_block.name = "unroll_iter_" + std::to_string(iter);

        // Add constant instruction for iteration value
        InstructionData const_inst;
        const_inst.result = iter_const_id;
        const_inst.type = make_i64_type();
        const_inst.inst = ConstantInst{ConstInt{iter_value, true, 64}};
        const_block.instructions.push_back(const_inst);

        // Branch to the first body block (or header content)
        uint32_t first_body =
            body_block_ids.empty() ? block_map[loop.header_id] : block_map[body_block_ids[0]];
        const_block.terminator = BranchTerm{first_body};
        new_blocks.push_back(std::move(const_block));

        // Clone body blocks
        for (uint32_t old_id : body_block_ids) {
            const auto* old_block = get_block(func, old_id);
            if (!old_block)
                continue;

            BasicBlock new_block;
            new_block.id = block_map[old_id];
            new_block.name = old_block->name + "_unroll_" + std::to_string(iter);

            // Clone instructions with value remapping
            for (const auto& old_inst : old_block->instructions) {
                InstructionData new_inst = old_inst;

                // Remap result
                if (old_inst.result != INVALID_VALUE) {
                    ValueId new_result = next_value_id++;
                    value_map[old_inst.result] = new_result;
                    new_inst.result = new_result;
                }

                // Remap operand values in instruction
                std::visit(
                    [&value_map](auto& inst) {
                        using T = std::decay_t<decltype(inst)>;
                        if constexpr (std::is_same_v<T, BinaryInst>) {
                            if (value_map.count(inst.left.id)) {
                                inst.left.id = value_map[inst.left.id];
                            }
                            if (value_map.count(inst.right.id)) {
                                inst.right.id = value_map[inst.right.id];
                            }
                        } else if constexpr (std::is_same_v<T, UnaryInst>) {
                            if (value_map.count(inst.operand.id)) {
                                inst.operand.id = value_map[inst.operand.id];
                            }
                        } else if constexpr (std::is_same_v<T, LoadInst>) {
                            if (value_map.count(inst.ptr.id)) {
                                inst.ptr.id = value_map[inst.ptr.id];
                            }
                        } else if constexpr (std::is_same_v<T, StoreInst>) {
                            if (value_map.count(inst.ptr.id)) {
                                inst.ptr.id = value_map[inst.ptr.id];
                            }
                            if (value_map.count(inst.value.id)) {
                                inst.value.id = value_map[inst.value.id];
                            }
                        } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                            if (value_map.count(inst.base.id)) {
                                inst.base.id = value_map[inst.base.id];
                            }
                            for (auto& idx : inst.indices) {
                                if (value_map.count(idx.id)) {
                                    idx.id = value_map[idx.id];
                                }
                            }
                        } else if constexpr (std::is_same_v<T, CallInst>) {
                            for (auto& arg : inst.args) {
                                if (value_map.count(arg.id)) {
                                    arg.id = value_map[arg.id];
                                }
                            }
                        }
                        // Other instruction types handled similarly
                    },
                    new_inst.inst);

                new_block.instructions.push_back(std::move(new_inst));
            }

            // Clone terminator with block remapping
            if (old_block->terminator) {
                new_block.terminator = *old_block->terminator;
                std::visit(
                    [&block_map, &value_map, iter, trip_count, exit_block_id, &next_block_id,
                     &new_blocks](auto& term) {
                        using T = std::decay_t<decltype(term)>;
                        if constexpr (std::is_same_v<T, BranchTerm>) {
                            if (block_map.count(term.target)) {
                                term.target = block_map[term.target];
                            }
                        } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                            if (value_map.count(term.condition.id)) {
                                term.condition.id = value_map[term.condition.id];
                            }
                            if (block_map.count(term.true_block)) {
                                term.true_block = block_map[term.true_block];
                            }
                            if (block_map.count(term.false_block)) {
                                term.false_block = block_map[term.false_block];
                            }
                        }
                    },
                    *new_block.terminator);
            }

            new_blocks.push_back(std::move(new_block));
        }
    }

    // Connect iterations: last block of iteration N jumps to first block of iteration N+1
    // Last iteration jumps to exit block
    for (size_t i = 0; i < new_blocks.size(); ++i) {
        auto& block = new_blocks[i];
        if (block.terminator) {
            std::visit(
                [exit_block_id, &new_blocks, i, trip_count](auto& term) {
                    using T = std::decay_t<decltype(term)>;
                    if constexpr (std::is_same_v<T, BranchTerm>) {
                        // Check if this branches back to loop header (latch behavior)
                        // If it's the last iteration, branch to exit instead
                    }
                },
                *block.terminator);
        }
    }

    // Modify the original header to branch directly to first unrolled block
    auto* header_mut = get_block_mut(func, loop.header_id);
    if (header_mut) {
        header_mut->terminator = BranchTerm{first_unrolled_block};
        // Clear phi nodes (no longer needed after unrolling)
        header_mut->instructions.erase(
            std::remove_if(header_mut->instructions.begin(), header_mut->instructions.end(),
                           [](const InstructionData& inst) {
                               return std::holds_alternative<PhiInst>(inst.inst);
                           }),
            header_mut->instructions.end());
    }

    // Add all new blocks to the function
    for (auto& block : new_blocks) {
        func.blocks.push_back(std::move(block));
    }

    // Mark original body blocks for removal (except header which we modified)
    for (uint32_t block_id : loop.body_blocks) {
        if (block_id != loop.header_id) {
            auto* block = get_block_mut(func, block_id);
            if (block) {
                block->instructions.clear();
                block->terminator = UnreachableTerm{};
            }
        }
    }

    return true;
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
