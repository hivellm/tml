TML_MODULE("compiler")

//! # SimplifySelect Pass
//!
//! Simplifies select (conditional) instructions.

#include "mir/passes/simplify_select.hpp"

namespace tml::mir {

auto SimplifySelectPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    for (auto& block : func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            if (auto* sel = std::get_if<SelectInst>(&block.instructions[i].inst)) {
                changed |= simplify_select(func, block, i, *sel);
            }
        }
    }

    return changed;
}

auto SimplifySelectPass::simplify_select(Function& func, BasicBlock& block, size_t idx,
                                         SelectInst& sel) -> bool {
    ValueId result = block.instructions[idx].result;

    // Check for constant condition
    auto cond_val = get_const_bool(func, sel.condition.id);
    if (cond_val) {
        // select(true, a, b) → a
        // select(false, a, b) → b
        ValueId replacement = *cond_val ? sel.true_val.id : sel.false_val.id;
        replace_uses(func, result, replacement);
        return true;
    }

    // select(c, a, a) → a (same value on both branches)
    if (sel.true_val.id == sel.false_val.id) {
        replace_uses(func, result, sel.true_val.id);
        return true;
    }

    // Check for boolean select patterns
    auto true_const = get_const_bool(func, sel.true_val.id);
    auto false_const = get_const_bool(func, sel.false_val.id);

    if (true_const && false_const) {
        if (*true_const && !*false_const) {
            // select(c, true, false) → c
            replace_uses(func, result, sel.condition.id);
            return true;
        } else if (!*true_const && *false_const) {
            // select(c, false, true) → not c
            // Create a new not instruction
            UnaryInst not_inst;
            not_inst.op = UnaryOp::Not;
            not_inst.operand = sel.condition;

            InstructionData new_mir;
            new_mir.result = func.next_value_id++;
            new_mir.inst = not_inst;

            // Replace the select with the not
            block.instructions[idx] = std::move(new_mir);
            replace_uses(func, result, block.instructions[idx].result);
            return true;
        }
    }

    // select(not c, a, b) → select(c, b, a)
    auto not_operand = is_not_instruction(func, sel.condition.id);
    if (not_operand) {
        sel.condition.id = *not_operand;
        std::swap(sel.true_val, sel.false_val);
        return true;
    }

    return false;
}

auto SimplifySelectPass::get_const_bool(const Function& func, ValueId id) -> std::optional<bool> {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == id) {
                if (auto* c = std::get_if<ConstantInst>(&inst.inst)) {
                    if (auto* b = std::get_if<ConstBool>(&c->value)) {
                        return b->value;
                    }
                }
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

auto SimplifySelectPass::is_not_instruction(const Function& func, ValueId id)
    -> std::optional<ValueId> {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == id) {
                if (auto* unary = std::get_if<UnaryInst>(&inst.inst)) {
                    if (unary->op == UnaryOp::Not) {
                        return unary->operand.id;
                    }
                }
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

auto SimplifySelectPass::replace_uses(Function& func, ValueId old_val, ValueId new_val) -> void {
    for (auto& block : func.blocks) {
        for (auto& inst : block.instructions) {
            std::visit(
                [old_val, new_val](auto& inner) {
                    using T = std::decay_t<decltype(inner)>;
                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        if (inner.left.id == old_val)
                            inner.left.id = new_val;
                        if (inner.right.id == old_val)
                            inner.right.id = new_val;
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        if (inner.operand.id == old_val)
                            inner.operand.id = new_val;
                    } else if constexpr (std::is_same_v<T, CastInst>) {
                        if (inner.operand.id == old_val)
                            inner.operand.id = new_val;
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        if (inner.ptr.id == old_val)
                            inner.ptr.id = new_val;
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        if (inner.ptr.id == old_val)
                            inner.ptr.id = new_val;
                        if (inner.value.id == old_val)
                            inner.value.id = new_val;
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (auto& arg : inner.args) {
                            if (arg.id == old_val)
                                arg.id = new_val;
                        }
                    } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                        if (inner.receiver.id == old_val)
                            inner.receiver.id = new_val;
                        for (auto& arg : inner.args) {
                            if (arg.id == old_val)
                                arg.id = new_val;
                        }
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        if (inner.condition.id == old_val)
                            inner.condition.id = new_val;
                        if (inner.true_val.id == old_val)
                            inner.true_val.id = new_val;
                        if (inner.false_val.id == old_val)
                            inner.false_val.id = new_val;
                    } else if constexpr (std::is_same_v<T, PhiInst>) {
                        for (auto& [val, _] : inner.incoming) {
                            if (val.id == old_val)
                                val.id = new_val;
                        }
                    } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                        if (inner.base.id == old_val)
                            inner.base.id = new_val;
                        for (auto& idx : inner.indices) {
                            if (idx.id == old_val)
                                idx.id = new_val;
                        }
                    }
                },
                inst.inst);
        }

        // Update terminator
        if (block.terminator) {
            std::visit(
                [old_val, new_val](auto& term) {
                    using T = std::decay_t<decltype(term)>;
                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (term.value && term.value->id == old_val) {
                            term.value->id = new_val;
                        }
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        if (term.condition.id == old_val) {
                            term.condition.id = new_val;
                        }
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        if (term.discriminant.id == old_val) {
                            term.discriminant.id = new_val;
                        }
                    }
                },
                *block.terminator);
        }
    }
}

} // namespace tml::mir
