//! # MergeReturns Pass
//!
//! Combines multiple return statements into a single exit block.

#include "mir/passes/merge_returns.hpp"

namespace tml::mir {

auto MergeReturnsPass::run_on_function(Function& func) -> bool {
    auto return_blocks = find_return_blocks(func);

    // Only merge if there are multiple returns
    if (return_blocks.size() <= 1) {
        return false;
    }

    return create_unified_exit(func, return_blocks);
}

auto MergeReturnsPass::find_return_blocks(const Function& func) -> std::vector<size_t> {
    std::vector<size_t> result;

    for (size_t i = 0; i < func.blocks.size(); ++i) {
        const auto& block = func.blocks[i];
        if (block.terminator) {
            if (std::holds_alternative<ReturnTerm>(*block.terminator)) {
                result.push_back(i);
            }
        }
    }

    return result;
}

auto MergeReturnsPass::create_unified_exit(Function& func,
                                            const std::vector<size_t>& return_blocks) -> bool {
    // Create a new exit block
    BasicBlock exit_block;
    exit_block.id = func.next_block_id++;
    exit_block.name = "unified_exit";

    // Check if returns have values
    bool has_return_value = false;
    std::vector<std::pair<Value, uint32_t>> phi_incoming;

    for (size_t idx : return_blocks) {
        const auto& block = func.blocks[idx];
        if (auto* ret = std::get_if<ReturnTerm>(&*block.terminator)) {
            if (ret->value) {
                has_return_value = true;
                phi_incoming.emplace_back(*ret->value, block.id);
            }
        }
    }

    ValueId return_value_id = 0;

    if (has_return_value) {
        // Create phi node for return value
        PhiInst phi;
        for (const auto& [val, block_id] : phi_incoming) {
            phi.incoming.emplace_back(val, block_id);
        }

        // Get return type from first return
        if (!return_blocks.empty()) {
            const auto& first_block = func.blocks[return_blocks[0]];
            if (auto* ret = std::get_if<ReturnTerm>(&*first_block.terminator)) {
                if (ret->value) {
                    // Find the type of the return value from InstructionData.type
                    for (const auto& block : func.blocks) {
                        for (const auto& inst : block.instructions) {
                            if (inst.result == ret->value->id) {
                                phi.result_type = inst.type;
                                break;
                            }
                        }
                    }
                }
            }
        }

        InstructionData phi_mir;
        phi_mir.result = func.next_value_id++;
        phi_mir.inst = std::move(phi);

        return_value_id = phi_mir.result;
        exit_block.instructions.push_back(std::move(phi_mir));
    }

    // Set terminator for exit block
    ReturnTerm exit_ret;
    if (has_return_value) {
        exit_ret.value = Value{return_value_id};
    }
    exit_block.terminator = exit_ret;

    // Update predecessors for exit block
    for (size_t idx : return_blocks) {
        exit_block.predecessors.push_back(func.blocks[idx].id);
    }

    // Update return blocks to branch to exit
    for (size_t idx : return_blocks) {
        auto& block = func.blocks[idx];

        // Update successors
        block.successors.clear();
        block.successors.push_back(exit_block.id);

        // Replace return with unconditional branch
        BranchTerm branch;
        branch.target = exit_block.id;
        block.terminator = branch;
    }

    // Add exit block to function
    func.blocks.push_back(std::move(exit_block));

    return true;
}

} // namespace tml::mir
