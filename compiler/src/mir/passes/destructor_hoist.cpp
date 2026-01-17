//! # Destructor Loop Hoisting Pass Implementation
//!
//! Hoists loop-local object allocations outside the loop to reduce
//! allocation overhead.

#include "mir/passes/destructor_hoist.hpp"

#include <algorithm>

namespace tml::mir {

auto DestructorHoistPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    // Detect all loops in the function
    auto loops = detect_loops(func);
    stats_.loops_analyzed = loops.size();

    for (const auto& [header, loop_blocks] : loops) {
        // Find allocations within this loop
        auto allocations = find_loop_allocations(func, loop_blocks);
        stats_.allocations_found += allocations.size();

        // Build set for fast lookup
        std::unordered_set<size_t> loop_block_set(loop_blocks.begin(), loop_blocks.end());

        // Analyze each allocation
        for (auto& alloc : allocations) {
            // Check if object escapes the loop
            alloc.escapes_loop = escapes_loop(func, alloc.alloc_value, loop_block_set);
            if (alloc.escapes_loop)
                continue;

            // Check if class has reset() method
            alloc.has_reset_method = has_reset_method(alloc.class_name);

            // Find drop call within loop
            auto drop_info = find_drop_in_loop(func, alloc.alloc_value, loop_blocks);
            if (drop_info) {
                alloc.drop_block = drop_info->first;
                alloc.drop_inst_idx = drop_info->second;
            } else {
                continue; // No drop found, can't optimize
            }

            // Mark as hoistable if all conditions met
            alloc.can_hoist = !alloc.escapes_loop && alloc.has_reset_method;

            if (alloc.can_hoist) {
                // Find preheader block (block that jumps to loop header)
                // For simplicity, use header - 1 if it exists
                size_t preheader = header > 0 ? header - 1 : header;

                // Find exit block (successor of loop that's not in loop)
                size_t exit_block = 0;
                for (size_t block_idx : loop_blocks) {
                    const auto& block = func.blocks[block_idx];
                    if (block.terminator.has_value()) {
                        if (auto* br = std::get_if<BranchTerm>(&*block.terminator)) {
                            if (loop_block_set.find(br->target) == loop_block_set.end()) {
                                exit_block = br->target;
                                break;
                            }
                        } else if (auto* cond = std::get_if<CondBranchTerm>(&*block.terminator)) {
                            if (loop_block_set.find(cond->true_block) == loop_block_set.end()) {
                                exit_block = cond->true_block;
                                break;
                            }
                            if (loop_block_set.find(cond->false_block) == loop_block_set.end()) {
                                exit_block = cond->false_block;
                                break;
                            }
                        }
                    }
                }

                // Perform the optimization
                if (hoist_allocation(func, alloc, preheader)) {
                    if (replace_with_reset(func, alloc)) {
                        if (move_drop_after_loop(func, alloc, exit_block)) {
                            stats_.allocations_hoisted++;
                            stats_.drops_moved++;
                            changed = true;
                        }
                    }
                }
            }
        }
    }

    return changed;
}

auto DestructorHoistPass::detect_loops(const Function& func)
    -> std::vector<std::pair<size_t, std::vector<size_t>>> {
    std::vector<std::pair<size_t, std::vector<size_t>>> loops;

    // Simple back-edge detection for natural loops
    // A back edge is an edge where the target dominates the source
    // For simplicity, we detect blocks that jump back to earlier blocks

    for (size_t i = 0; i < func.blocks.size(); ++i) {
        const auto& block = func.blocks[i];
        if (!block.terminator.has_value())
            continue;

        // Check for back edges
        std::vector<size_t> targets;
        if (auto* br = std::get_if<BranchTerm>(&*block.terminator)) {
            targets.push_back(br->target);
        } else if (auto* cond = std::get_if<CondBranchTerm>(&*block.terminator)) {
            targets.push_back(cond->true_block);
            targets.push_back(cond->false_block);
        }

        for (size_t target : targets) {
            if (target < i) {
                // Back edge found: target is loop header
                // Collect all blocks between header and this block
                std::vector<size_t> loop_blocks;
                for (size_t j = target; j <= i; ++j) {
                    loop_blocks.push_back(j);
                }
                loops.emplace_back(target, std::move(loop_blocks));
            }
        }
    }

    return loops;
}

auto DestructorHoistPass::find_loop_allocations(const Function& func,
                                                const std::vector<size_t>& loop_blocks)
    -> std::vector<DestructorLoopAllocation> {
    std::vector<DestructorLoopAllocation> allocations;

    for (size_t block_idx : loop_blocks) {
        const auto& block = func.blocks[block_idx];

        for (size_t inst_idx = 0; inst_idx < block.instructions.size(); ++inst_idx) {
            const auto& inst = block.instructions[inst_idx];

            // Look for allocation calls (malloc, new, constructor calls)
            if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                // Check if this is a constructor call (ClassName_new or ClassName_create)
                const std::string& fn = call->func_name;
                size_t pos = fn.rfind("_new");
                if (pos == std::string::npos) {
                    pos = fn.rfind("_create");
                }

                if (pos != std::string::npos && pos > 0) {
                    DestructorLoopAllocation alloc;
                    alloc.alloc_value = inst.result;
                    alloc.class_name = fn.substr(0, pos);
                    alloc.alloc_block = block_idx;
                    alloc.alloc_inst_idx = inst_idx;
                    alloc.has_reset_method = false;
                    alloc.escapes_loop = false;
                    alloc.can_hoist = false;
                    allocations.push_back(alloc);
                }
            }
        }
    }

    return allocations;
}

auto DestructorHoistPass::escapes_loop(const Function& func, ValueId value,
                                       const std::unordered_set<size_t>& loop_block_set) -> bool {
    // Check all uses of the value to see if any escape the loop
    for (size_t block_idx = 0; block_idx < func.blocks.size(); ++block_idx) {
        bool in_loop = loop_block_set.find(block_idx) != loop_block_set.end();

        for (const auto& inst : func.blocks[block_idx].instructions) {
            // Check if this instruction uses the value
            bool uses_value = false;

            if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
                uses_value = (store->value.id == value || store->ptr.id == value);
            } else if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                for (const auto& arg : call->args) {
                    if (arg.id == value) {
                        uses_value = true;
                        // If passed to a function outside the loop, it escapes
                        if (!in_loop)
                            return true;
                    }
                }
            }

            // If used outside loop (except for drop), it escapes
            if (uses_value && !in_loop) {
                // Check if it's a drop call - that's OK
                if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                    if (call->func_name.find("_drop") != std::string::npos) {
                        continue; // Drop is OK
                    }
                }
                return true;
            }
        }

        // Check if returned from the block's terminator
        const auto& blk = func.blocks[block_idx];
        if (blk.terminator.has_value()) {
            if (auto* ret = std::get_if<ReturnTerm>(&*blk.terminator)) {
                if (ret->value && ret->value->id == value) {
                    return true; // Returned from function = escapes
                }
            }
        }
    }

    return false;
}

auto DestructorHoistPass::has_reset_method(const std::string& class_name) -> bool {
    // Check if the class has a reset() method
    auto class_def = env_.lookup_class(class_name);
    if (!class_def)
        return false;

    for (const auto& method : class_def->methods) {
        if (method.sig.name == "reset") {
            return true;
        }
    }

    return false;
}

auto DestructorHoistPass::find_drop_in_loop(const Function& func, ValueId alloc,
                                            const std::vector<size_t>& loop_blocks)
    -> std::optional<std::pair<size_t, size_t>> {
    for (size_t block_idx : loop_blocks) {
        const auto& block = func.blocks[block_idx];

        for (size_t inst_idx = 0; inst_idx < block.instructions.size(); ++inst_idx) {
            const auto& inst = block.instructions[inst_idx];

            if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                // Check if this is a drop call for our allocation
                if (call->func_name.find("_drop") != std::string::npos) {
                    for (const auto& arg : call->args) {
                        if (arg.id == alloc) {
                            return std::make_pair(block_idx, inst_idx);
                        }
                    }
                }
            }
        }
    }

    return std::nullopt;
}

auto DestructorHoistPass::hoist_allocation(Function& func, const DestructorLoopAllocation& alloc,
                                           size_t preheader_block) -> bool {
    if (preheader_block >= func.blocks.size())
        return false;
    if (alloc.alloc_block >= func.blocks.size())
        return false;

    auto& src_block = func.blocks[alloc.alloc_block];
    auto& dst_block = func.blocks[preheader_block];

    if (alloc.alloc_inst_idx >= src_block.instructions.size())
        return false;

    // Move the allocation instruction to the preheader
    auto alloc_inst = std::move(src_block.instructions[alloc.alloc_inst_idx]);

    // Remove from source
    src_block.instructions.erase(src_block.instructions.begin() +
                                 static_cast<std::ptrdiff_t>(alloc.alloc_inst_idx));

    // Insert before terminator in preheader
    dst_block.instructions.push_back(std::move(alloc_inst));

    return true;
}

auto DestructorHoistPass::replace_with_reset(Function& func, const DestructorLoopAllocation& alloc)
    -> bool {
    // Find where the original allocation was and insert a reset call there
    // This is a simplified implementation - in practice we'd need to track
    // the new location after hoisting

    // Create a reset call instruction
    CallInst reset_call;
    reset_call.func_name = alloc.class_name + "_reset";
    reset_call.args.push_back(Value{alloc.alloc_value});

    InstructionData reset_inst;
    reset_inst.result = 0; // reset() typically returns void
    reset_inst.type = nullptr;
    reset_inst.inst = reset_call;

    // Find the original allocation location in the loop
    // Since we already removed the allocation, we insert at the beginning
    // of the original block
    if (alloc.alloc_block < func.blocks.size()) {
        auto& block = func.blocks[alloc.alloc_block];

        // Find a good insertion point (after phi nodes if any)
        size_t insert_pos = 0;
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            if (std::holds_alternative<PhiInst>(block.instructions[i].inst)) {
                insert_pos = i + 1;
            } else {
                break;
            }
        }

        block.instructions.insert(block.instructions.begin() +
                                      static_cast<std::ptrdiff_t>(insert_pos),
                                  std::move(reset_inst));
        return true;
    }

    return false;
}

auto DestructorHoistPass::move_drop_after_loop(Function& func,
                                               const DestructorLoopAllocation& alloc,
                                               size_t exit_block) -> bool {
    if (exit_block >= func.blocks.size())
        return false;
    if (alloc.drop_block >= func.blocks.size())
        return false;

    auto& src_block = func.blocks[alloc.drop_block];
    auto& dst_block = func.blocks[exit_block];

    // Find and move the drop instruction
    // Note: indices may have shifted due to earlier modifications
    // We search for the drop call for this allocation

    for (size_t i = 0; i < src_block.instructions.size(); ++i) {
        const auto& inst = src_block.instructions[i];
        if (auto* call = std::get_if<CallInst>(&inst.inst)) {
            if (call->func_name.find("_drop") != std::string::npos) {
                for (const auto& arg : call->args) {
                    if (arg.id == alloc.alloc_value) {
                        // Found it - move to exit block
                        auto drop_inst = std::move(src_block.instructions[i]);
                        src_block.instructions.erase(src_block.instructions.begin() +
                                                     static_cast<std::ptrdiff_t>(i));

                        // Insert at beginning of exit block
                        dst_block.instructions.insert(dst_block.instructions.begin(),
                                                      std::move(drop_inst));
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

} // namespace tml::mir
