//! # Code Sinking Pass
//!
//! Moves computations closer to their uses.

#include "mir/passes/sinking.hpp"

namespace tml::mir {

auto SinkingPass::run_on_function(Function& func) -> bool {
    bool changed = false;
    bool made_progress = true;

    // Iterate until no more sinking can be done
    while (made_progress) {
        made_progress = false;

        for (size_t block_idx = 0; block_idx < func.blocks.size(); ++block_idx) {
            auto& block = func.blocks[block_idx];

            // Skip blocks with only one successor or no successors
            if (block.successors.size() <= 1) {
                continue;
            }

            // Try to sink instructions from this block
            std::vector<size_t> to_sink;
            std::vector<uint32_t> sink_targets;

            for (size_t i = 0; i < block.instructions.size(); ++i) {
                auto& inst = block.instructions[i];

                if (!can_sink(inst)) {
                    continue;
                }

                auto target = find_single_use_block(func, inst.result, block.id);
                if (!target) {
                    continue;
                }

                // Check that target is a successor
                bool is_successor = false;
                for (uint32_t succ : block.successors) {
                    if (succ == *target) {
                        is_successor = true;
                        break;
                    }
                }

                if (!is_successor) {
                    continue;
                }

                // Check operands are available
                if (!operands_available_in(func, inst, *target, block.id)) {
                    continue;
                }

                to_sink.push_back(i);
                sink_targets.push_back(*target);
            }

            // Sink instructions (in reverse order to maintain indices)
            for (size_t j = to_sink.size(); j > 0; --j) {
                size_t i = to_sink[j - 1];
                uint32_t target = sink_targets[j - 1];

                int target_idx = get_block_index(func, target);
                if (target_idx < 0) {
                    continue;
                }

                auto& target_block = func.blocks[static_cast<size_t>(target_idx)];

                // Insert at beginning of target block
                auto inst = block.instructions[i];
                target_block.instructions.insert(target_block.instructions.begin(), inst);

                // Remove from source block
                block.instructions.erase(block.instructions.begin() +
                                         static_cast<std::ptrdiff_t>(i));

                made_progress = true;
                changed = true;
            }
        }
    }

    return changed;
}

auto SinkingPass::can_sink(const InstructionData& inst) -> bool {
    return std::visit(
        [](const auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;
            (void)i;

            // Only sink pure, side-effect-free instructions
            if constexpr (std::is_same_v<T, BinaryInst> || std::is_same_v<T, UnaryInst> ||
                          std::is_same_v<T, CastInst> || std::is_same_v<T, SelectInst>) {
                return true;
            } else {
                // Don't sink loads, stores, calls, phis, constants, etc.
                return false;
            }
        },
        inst.inst);
}

auto SinkingPass::find_single_use_block(const Function& func, ValueId value, uint32_t def_block)
    -> std::optional<uint32_t> {
    std::unordered_set<uint32_t> use_blocks;

    for (const auto& block : func.blocks) {
        if (block.id == def_block) {
            // Check if used in the same block (after definition)
            // If so, we can't sink it
            for (const auto& inst : block.instructions) {
                bool uses = std::visit(
                    [value](const auto& i) -> bool {
                        using T = std::decay_t<decltype(i)>;
                        (void)i;

                        if constexpr (std::is_same_v<T, BinaryInst>) {
                            return i.left.id == value || i.right.id == value;
                        } else if constexpr (std::is_same_v<T, UnaryInst>) {
                            return i.operand.id == value;
                        } else if constexpr (std::is_same_v<T, CastInst>) {
                            return i.operand.id == value;
                        } else if constexpr (std::is_same_v<T, SelectInst>) {
                            return i.condition.id == value || i.true_val.id == value ||
                                   i.false_val.id == value;
                        } else if constexpr (std::is_same_v<T, LoadInst>) {
                            return i.ptr.id == value;
                        } else if constexpr (std::is_same_v<T, StoreInst>) {
                            return i.ptr.id == value || i.value.id == value;
                        } else if constexpr (std::is_same_v<T, CallInst>) {
                            for (const auto& arg : i.args) {
                                if (arg.id == value)
                                    return true;
                            }
                            return false;
                        } else {
                            return false;
                        }
                    },
                    inst.inst);

                // If used after definition in same block, return - can't sink
                if (uses && inst.result != value) {
                    // Check this is not the defining instruction
                    return std::nullopt;
                }
            }

            // Check terminator
            if (block.terminator) {
                bool term_uses = std::visit(
                    [value](const auto& t) -> bool {
                        using T = std::decay_t<decltype(t)>;

                        if constexpr (std::is_same_v<T, ReturnTerm>) {
                            return t.value && t.value->id == value;
                        } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                            return t.condition.id == value;
                        } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                            return t.discriminant.id == value;
                        } else {
                            return false;
                        }
                    },
                    *block.terminator);

                if (term_uses) {
                    return std::nullopt; // Used in same block's terminator
                }
            }

            continue;
        }

        // Check if value is used in this block
        bool used_in_block = false;

        for (const auto& inst : block.instructions) {
            bool uses = std::visit(
                [value](const auto& i) -> bool {
                    using T = std::decay_t<decltype(i)>;
                    (void)i;

                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        return i.left.id == value || i.right.id == value;
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        return i.operand.id == value;
                    } else if constexpr (std::is_same_v<T, CastInst>) {
                        return i.operand.id == value;
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        return i.condition.id == value || i.true_val.id == value ||
                               i.false_val.id == value;
                    } else if constexpr (std::is_same_v<T, PhiInst>) {
                        for (const auto& [val, _] : i.incoming) {
                            if (val.id == value)
                                return true;
                        }
                        return false;
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        return i.ptr.id == value;
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        return i.ptr.id == value || i.value.id == value;
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (const auto& arg : i.args) {
                            if (arg.id == value)
                                return true;
                        }
                        return false;
                    } else {
                        return false;
                    }
                },
                inst.inst);

            if (uses) {
                used_in_block = true;
                break;
            }
        }

        // Also check terminator
        if (!used_in_block && block.terminator) {
            used_in_block = std::visit(
                [value](const auto& t) -> bool {
                    using T = std::decay_t<decltype(t)>;

                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        return t.value && t.value->id == value;
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        return t.condition.id == value;
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        return t.discriminant.id == value;
                    } else {
                        return false;
                    }
                },
                *block.terminator);
        }

        if (used_in_block) {
            use_blocks.insert(block.id);
        }
    }

    if (use_blocks.size() == 1) {
        return *use_blocks.begin();
    }

    return std::nullopt;
}

auto SinkingPass::operands_available_in(const Function& func, const InstructionData& inst,
                                        uint32_t target_block, uint32_t source_block) -> bool {
    // Get operand value IDs
    std::vector<ValueId> operands;

    std::visit(
        [&operands](const auto& i) {
            using T = std::decay_t<decltype(i)>;

            if constexpr (std::is_same_v<T, BinaryInst>) {
                operands.push_back(i.left.id);
                operands.push_back(i.right.id);
            } else if constexpr (std::is_same_v<T, UnaryInst>) {
                operands.push_back(i.operand.id);
            } else if constexpr (std::is_same_v<T, CastInst>) {
                operands.push_back(i.operand.id);
            } else if constexpr (std::is_same_v<T, SelectInst>) {
                operands.push_back(i.condition.id);
                operands.push_back(i.true_val.id);
                operands.push_back(i.false_val.id);
            }
        },
        inst.inst);

    // For each operand, check if it dominates the target block
    for (ValueId op : operands) {
        // If operand is defined in source block and is not a block argument,
        // it won't be available in target
        // (unless it's a parameter or defined before the branch point)

        // Simple check: operand must dominate target block
        if (!value_dominates_block(func, op, target_block)) {
            // Check if defined in source block - in that case it's still available
            // because source dominates target (it's a predecessor)
            bool defined_in_source = false;
            int source_idx = get_block_index(func, source_block);
            if (source_idx >= 0) {
                for (const auto& src_inst :
                     func.blocks[static_cast<size_t>(source_idx)].instructions) {
                    if (src_inst.result == op) {
                        defined_in_source = true;
                        break;
                    }
                }
            }

            if (!defined_in_source) {
                return false;
            }
        }
    }

    return true;
}

auto SinkingPass::value_dominates_block(const Function& func, ValueId value, uint32_t target_block)
    -> bool {
    // Simple domination check - value is available if:
    // 1. It's a function parameter
    // 2. It's defined in a block that comes before the target in the CFG

    // Check if it's a parameter
    for (const auto& param : func.params) {
        if (param.value_id == value) {
            return true;
        }
    }

    // Find where value is defined
    int def_block_idx = -1;
    for (size_t bi = 0; bi < func.blocks.size(); ++bi) {
        for (const auto& inst : func.blocks[bi].instructions) {
            if (inst.result == value) {
                def_block_idx = static_cast<int>(bi);
                break;
            }
        }
        if (def_block_idx >= 0)
            break;
    }

    if (def_block_idx < 0) {
        return false; // Value not found
    }

    int target_idx = get_block_index(func, target_block);
    if (target_idx < 0) {
        return false;
    }

    // Simple dominance approximation: def comes before target in block list
    // (This is a simplification - proper dominance would use dominator tree)
    return def_block_idx < target_idx;
}

auto SinkingPass::get_block_index(const Function& func, uint32_t id) -> int {
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        if (func.blocks[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace tml::mir
