TML_MODULE("compiler")

//! # Instruction Simplification Pass (Peephole Optimizations)
//!
//! Performs algebraic simplifications and pattern matching to simplify
//! individual instructions.

#include "mir/passes/inst_simplify.hpp"

namespace tml::mir {

auto InstSimplifyPass::run_on_function(Function& func) -> bool {
    bool changed = false;
    bool made_progress = true;

    while (made_progress) {
        made_progress = false;

        for (auto& block : func.blocks) {
            std::vector<size_t> to_remove;

            for (size_t i = 0; i < block.instructions.size(); ++i) {
                auto& inst = block.instructions[i];
                ValueId simplified = INVALID_VALUE;

                std::visit(
                    [&](const auto& inner) {
                        using T = std::decay_t<decltype(inner)>;

                        if constexpr (std::is_same_v<T, BinaryInst>) {
                            simplified = simplify_binary(inner, func);
                        } else if constexpr (std::is_same_v<T, UnaryInst>) {
                            simplified = simplify_unary(inner, func);
                        } else if constexpr (std::is_same_v<T, SelectInst>) {
                            simplified = simplify_select(inner, func);
                        }
                    },
                    inst.inst);

                if (simplified != INVALID_VALUE && simplified != inst.result) {
                    // Replace all uses of this result with the simplified value
                    replace_uses(func, inst.result, simplified);
                    to_remove.push_back(i);
                    made_progress = true;
                }
            }

            // Remove simplified instructions (reverse order)
            for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
                block.instructions.erase(block.instructions.begin() +
                                         static_cast<std::ptrdiff_t>(*it));
            }

            changed |= made_progress;
        }
    }

    return changed;
}

auto InstSimplifyPass::simplify_binary(const BinaryInst& inst, const Function& func) -> ValueId {
    auto left_const = get_const_int(func, inst.left.id);
    auto right_const = get_const_int(func, inst.right.id);
    bool same_operands = are_same_value(func, inst.left.id, inst.right.id);

    switch (inst.op) {
    case BinOp::Add:
        // x + 0 = x
        if (right_const && *right_const == 0) {
            return inst.left.id;
        }
        // 0 + x = x
        if (left_const && *left_const == 0) {
            return inst.right.id;
        }
        break;

    case BinOp::Sub:
        // x - 0 = x
        if (right_const && *right_const == 0) {
            return inst.left.id;
        }
        // x - x = 0 (handled by creating constant, but we can't here - skip)
        break;

    case BinOp::Mul:
        // x * 0 = 0, 0 * x = 0
        if ((right_const && *right_const == 0) || (left_const && *left_const == 0)) {
            // Need to return a zero constant - for now skip
            // This would require creating a new constant instruction
        }
        // x * 1 = x
        if (right_const && *right_const == 1) {
            return inst.left.id;
        }
        // 1 * x = x
        if (left_const && *left_const == 1) {
            return inst.right.id;
        }
        break;

    case BinOp::Div:
        // x / 1 = x
        if (right_const && *right_const == 1) {
            return inst.left.id;
        }
        break;

    case BinOp::BitAnd:
        // x & x = x
        if (same_operands) {
            return inst.left.id;
        }
        // x & 0 = 0 (skip - need constant)
        // x & -1 = x
        if (right_const && *right_const == -1) {
            return inst.left.id;
        }
        if (left_const && *left_const == -1) {
            return inst.right.id;
        }
        break;

    case BinOp::BitOr:
        // x | x = x
        if (same_operands) {
            return inst.left.id;
        }
        // x | 0 = x
        if (right_const && *right_const == 0) {
            return inst.left.id;
        }
        if (left_const && *left_const == 0) {
            return inst.right.id;
        }
        break;

    case BinOp::BitXor:
        // x ^ 0 = x
        if (right_const && *right_const == 0) {
            return inst.left.id;
        }
        if (left_const && *left_const == 0) {
            return inst.right.id;
        }
        // x ^ x = 0 (skip - need constant)
        break;

    case BinOp::Shl:
    case BinOp::Shr:
        // x << 0 = x, x >> 0 = x
        if (right_const && *right_const == 0) {
            return inst.left.id;
        }
        break;

    case BinOp::Eq:
        // x == x = true (skip - need bool constant)
        break;

    case BinOp::Ne:
        // x != x = false (skip - need bool constant)
        break;

    default:
        break;
    }

    return INVALID_VALUE;
}

auto InstSimplifyPass::simplify_unary(const UnaryInst& inst, const Function& func) -> ValueId {
    // not (not x) = x
    if (inst.op == UnaryOp::Not) {
        // Find the instruction that defines the operand
        for (const auto& block : func.blocks) {
            for (const auto& other : block.instructions) {
                if (other.result == inst.operand.id) {
                    if (auto* inner = std::get_if<UnaryInst>(&other.inst)) {
                        if (inner->op == UnaryOp::Not) {
                            // Double negation - return the inner operand
                            return inner->operand.id;
                        }
                    }
                    break;
                }
            }
        }
    }

    return INVALID_VALUE;
}

auto InstSimplifyPass::simplify_select(const SelectInst& inst, const Function& func) -> ValueId {
    // select true, x, y = x
    // select false, x, y = y
    auto cond_const = get_const_bool(func, inst.condition.id);
    if (cond_const.has_value()) {
        return *cond_const ? inst.true_val.id : inst.false_val.id;
    }

    // select c, x, x = x
    if (are_same_value(func, inst.true_val.id, inst.false_val.id)) {
        return inst.true_val.id;
    }

    return INVALID_VALUE;
}

auto InstSimplifyPass::get_const_int(const Function& func, ValueId id) -> std::optional<int64_t> {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == id) {
                if (auto* ci = std::get_if<ConstantInst>(&inst.inst)) {
                    if (auto* int_val = std::get_if<ConstInt>(&ci->value)) {
                        return int_val->value;
                    }
                }
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

auto InstSimplifyPass::get_const_bool(const Function& func, ValueId id) -> std::optional<bool> {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == id) {
                if (auto* ci = std::get_if<ConstantInst>(&inst.inst)) {
                    if (auto* bool_val = std::get_if<ConstBool>(&ci->value)) {
                        return bool_val->value;
                    }
                }
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

auto InstSimplifyPass::are_same_value(const Function& func, ValueId a, ValueId b) -> bool {
    // Same ID
    if (a == b) {
        return true;
    }

    // Check if both are the same constant
    auto const_a = get_const_int(func, a);
    auto const_b = get_const_int(func, b);
    if (const_a && const_b && *const_a == *const_b) {
        return true;
    }

    auto bool_a = get_const_bool(func, a);
    auto bool_b = get_const_bool(func, b);
    if (bool_a && bool_b && *bool_a == *bool_b) {
        return true;
    }

    return false;
}

void InstSimplifyPass::replace_uses(Function& func, ValueId old_value, ValueId new_value) {
    for (auto& block : func.blocks) {
        for (auto& inst : block.instructions) {
            std::visit(
                [old_value, new_value](auto& i) {
                    using T = std::decay_t<decltype(i)>;

                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        if (i.left.id == old_value)
                            i.left.id = new_value;
                        if (i.right.id == old_value)
                            i.right.id = new_value;
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        if (i.operand.id == old_value)
                            i.operand.id = new_value;
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        if (i.ptr.id == old_value)
                            i.ptr.id = new_value;
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        if (i.ptr.id == old_value)
                            i.ptr.id = new_value;
                        if (i.value.id == old_value)
                            i.value.id = new_value;
                    } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                        if (i.base.id == old_value)
                            i.base.id = new_value;
                        for (auto& idx : i.indices) {
                            if (idx.id == old_value)
                                idx.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                        if (i.aggregate.id == old_value)
                            i.aggregate.id = new_value;
                    } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                        if (i.aggregate.id == old_value)
                            i.aggregate.id = new_value;
                        if (i.value.id == old_value)
                            i.value.id = new_value;
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (auto& arg : i.args) {
                            if (arg.id == old_value)
                                arg.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                        if (i.receiver.id == old_value)
                            i.receiver.id = new_value;
                        for (auto& arg : i.args) {
                            if (arg.id == old_value)
                                arg.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, CastInst>) {
                        if (i.operand.id == old_value)
                            i.operand.id = new_value;
                    } else if constexpr (std::is_same_v<T, PhiInst>) {
                        for (auto& [val, _] : i.incoming) {
                            if (val.id == old_value)
                                val.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        if (i.condition.id == old_value)
                            i.condition.id = new_value;
                        if (i.true_val.id == old_value)
                            i.true_val.id = new_value;
                        if (i.false_val.id == old_value)
                            i.false_val.id = new_value;
                    } else if constexpr (std::is_same_v<T, StructInitInst>) {
                        for (auto& field : i.fields) {
                            if (field.id == old_value)
                                field.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, EnumInitInst>) {
                        for (auto& p : i.payload) {
                            if (p.id == old_value)
                                p.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, TupleInitInst>) {
                        for (auto& elem : i.elements) {
                            if (elem.id == old_value)
                                elem.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, ArrayInitInst>) {
                        for (auto& elem : i.elements) {
                            if (elem.id == old_value)
                                elem.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, AwaitInst>) {
                        if (i.poll_value.id == old_value)
                            i.poll_value.id = new_value;
                    } else if constexpr (std::is_same_v<T, ClosureInitInst>) {
                        for (auto& cap : i.captures) {
                            if (cap.second.id == old_value)
                                cap.second.id = new_value;
                        }
                    }
                },
                inst.inst);
        }

        // Update terminators
        if (block.terminator) {
            std::visit(
                [old_value, new_value](auto& t) {
                    using T = std::decay_t<decltype(t)>;

                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (t.value && t.value->id == old_value) {
                            t.value->id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        if (t.condition.id == old_value)
                            t.condition.id = new_value;
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        if (t.discriminant.id == old_value)
                            t.discriminant.id = new_value;
                    }
                },
                *block.terminator);
        }
    }
}

} // namespace tml::mir
