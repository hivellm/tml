TML_MODULE("compiler")

//! # Aggressive Dead Code Elimination (ADCE) Pass
//!
//! Removes instructions that don't contribute to program output.

#include "mir/passes/adce.hpp"

#include <queue>

namespace tml::mir {

auto ADCEPass::run_on_function(Function& func) -> bool {
    std::unordered_set<ValueId> live_values;

    // Step 1: Mark all instructions with side effects as live
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (has_side_effects(inst)) {
                live_values.insert(inst.result);
                // Mark all operands as live
                for (ValueId op : get_operands(inst)) {
                    mark_live(func, op, live_values);
                }
            }
        }

        // Mark values used in terminators as live
        if (block.terminator) {
            std::visit(
                [&](const auto& t) {
                    using T = std::decay_t<decltype(t)>;

                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (t.value) {
                            mark_live(func, t.value->id, live_values);
                        }
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        mark_live(func, t.condition.id, live_values);
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        mark_live(func, t.discriminant.id, live_values);
                    }
                },
                *block.terminator);
        }
    }

    // Step 2: Remove dead instructions
    bool changed = false;

    for (auto& block : func.blocks) {
        std::vector<InstructionData> new_instructions;
        new_instructions.reserve(block.instructions.size());

        for (auto& inst : block.instructions) {
            if (live_values.count(inst.result) > 0 || has_side_effects(inst)) {
                new_instructions.push_back(std::move(inst));
            } else {
                changed = true;
            }
        }

        block.instructions = std::move(new_instructions);
    }

    return changed;
}

auto ADCEPass::mark_live(const Function& func, ValueId value,
                         std::unordered_set<ValueId>& live_values) -> void {
    std::queue<ValueId> worklist;
    worklist.push(value);

    while (!worklist.empty()) {
        ValueId v = worklist.front();
        worklist.pop();

        if (live_values.count(v) > 0) {
            continue; // Already marked
        }

        live_values.insert(v);

        // Find the instruction that defines this value
        const auto* def = find_def(func, v);
        if (def) {
            // Mark all operands as live
            for (ValueId op : get_operands(*def)) {
                if (live_values.count(op) == 0) {
                    worklist.push(op);
                }
            }
        }
    }
}

auto ADCEPass::has_side_effects(const InstructionData& inst) -> bool {
    return std::visit(
        [](const auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;
            (void)i;

            if constexpr (std::is_same_v<T, StoreInst>) {
                return true;
            } else if constexpr (std::is_same_v<T, CallInst>) {
                // Conservatively assume all calls have side effects
                // A more sophisticated analysis could track pure functions
                return true;
            } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                return true;
            } else if constexpr (std::is_same_v<T, AllocaInst>) {
                // Allocas are side effects (stack allocation)
                return true;
            } else {
                return false;
            }
        },
        inst.inst);
}

auto ADCEPass::get_operands(const InstructionData& inst) -> std::vector<ValueId> {
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
            } else if constexpr (std::is_same_v<T, LoadInst>) {
                operands.push_back(i.ptr.id);
            } else if constexpr (std::is_same_v<T, StoreInst>) {
                operands.push_back(i.ptr.id);
                operands.push_back(i.value.id);
            } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                operands.push_back(i.base.id);
                for (const auto& idx : i.indices) {
                    operands.push_back(idx.id);
                }
            } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                operands.push_back(i.aggregate.id);
            } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                operands.push_back(i.aggregate.id);
                operands.push_back(i.value.id);
            } else if constexpr (std::is_same_v<T, CallInst>) {
                for (const auto& arg : i.args) {
                    operands.push_back(arg.id);
                }
            } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                operands.push_back(i.receiver.id);
                for (const auto& arg : i.args) {
                    operands.push_back(arg.id);
                }
            } else if constexpr (std::is_same_v<T, PhiInst>) {
                for (const auto& [val, _] : i.incoming) {
                    operands.push_back(val.id);
                }
            } else if constexpr (std::is_same_v<T, StructInitInst>) {
                for (const auto& field : i.fields) {
                    operands.push_back(field.id);
                }
            } else if constexpr (std::is_same_v<T, EnumInitInst>) {
                for (const auto& p : i.payload) {
                    operands.push_back(p.id);
                }
            } else if constexpr (std::is_same_v<T, TupleInitInst>) {
                for (const auto& elem : i.elements) {
                    operands.push_back(elem.id);
                }
            } else if constexpr (std::is_same_v<T, ArrayInitInst>) {
                for (const auto& elem : i.elements) {
                    operands.push_back(elem.id);
                }
            } else if constexpr (std::is_same_v<T, AwaitInst>) {
                operands.push_back(i.poll_value.id);
            } else if constexpr (std::is_same_v<T, ClosureInitInst>) {
                for (const auto& cap : i.captures) {
                    operands.push_back(cap.second.id);
                }
            }
            // ConstantInst, AllocaInst have no operands
        },
        inst.inst);

    return operands;
}

auto ADCEPass::find_def(const Function& func, ValueId value) -> const InstructionData* {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == value) {
                return &inst;
            }
        }
    }
    return nullptr;
}

} // namespace tml::mir
