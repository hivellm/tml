//! # Peephole Optimization Pass
//!
//! Local pattern-based instruction simplifications.

#include "mir/passes/peephole.hpp"

namespace tml::mir {

auto PeepholePass::run_on_function(Function& func) -> bool {
    bool changed = false;
    bool made_progress = true;

    while (made_progress) {
        made_progress = false;

        for (auto& block : func.blocks) {
            for (size_t i = 0; i < block.instructions.size(); ++i) {
                auto& inst = block.instructions[i];

                if (auto* bin = std::get_if<BinaryInst>(&inst.inst)) {
                    if (simplify_binary(func, block, i, *bin)) {
                        made_progress = true;
                        changed = true;
                    }
                } else if (auto* unary = std::get_if<UnaryInst>(&inst.inst)) {
                    if (simplify_unary(func, block, i, *unary)) {
                        made_progress = true;
                        changed = true;
                    }
                }
            }
        }
    }

    return changed;
}

auto PeepholePass::simplify_binary(Function& func, BasicBlock& block, size_t idx, BinaryInst& bin)
    -> bool {
    auto left_const = get_const_int(func, bin.left.id);
    auto right_const = get_const_int(func, bin.right.id);

    ValueId result = block.instructions[idx].result;

    switch (bin.op) {
    case BinOp::Add:
        // x + 0 → x
        if (right_const && *right_const == 0) {
            replace_uses(func, result, bin.left.id);
            return true;
        }
        // 0 + x → x
        if (left_const && *left_const == 0) {
            replace_uses(func, result, bin.right.id);
            return true;
        }
        break;

    case BinOp::Sub:
        // x - 0 → x
        if (right_const && *right_const == 0) {
            replace_uses(func, result, bin.left.id);
            return true;
        }
        // x - x → 0
        if (bin.left.id == bin.right.id) {
            ValueId zero = create_constant(func, 0, bin.result_type);
            replace_uses(func, result, zero);
            return true;
        }
        break;

    case BinOp::Mul:
        // x * 1 → x
        if (right_const && *right_const == 1) {
            replace_uses(func, result, bin.left.id);
            return true;
        }
        // 1 * x → x
        if (left_const && *left_const == 1) {
            replace_uses(func, result, bin.right.id);
            return true;
        }
        // x * 0 → 0
        if (right_const && *right_const == 0) {
            replace_uses(func, result, bin.right.id);
            return true;
        }
        // 0 * x → 0
        if (left_const && *left_const == 0) {
            replace_uses(func, result, bin.left.id);
            return true;
        }
        break;

    case BinOp::Div:
        // x / 1 → x
        if (right_const && *right_const == 1) {
            replace_uses(func, result, bin.left.id);
            return true;
        }
        break;

    case BinOp::BitAnd:
        // x & 0 → 0
        if (right_const && *right_const == 0) {
            replace_uses(func, result, bin.right.id);
            return true;
        }
        // 0 & x → 0
        if (left_const && *left_const == 0) {
            replace_uses(func, result, bin.left.id);
            return true;
        }
        // x & -1 → x (all bits set)
        if (right_const && *right_const == -1) {
            replace_uses(func, result, bin.left.id);
            return true;
        }
        // x & x → x
        if (bin.left.id == bin.right.id) {
            replace_uses(func, result, bin.left.id);
            return true;
        }
        break;

    case BinOp::BitOr:
        // x | 0 → x
        if (right_const && *right_const == 0) {
            replace_uses(func, result, bin.left.id);
            return true;
        }
        // 0 | x → x
        if (left_const && *left_const == 0) {
            replace_uses(func, result, bin.right.id);
            return true;
        }
        // x | x → x
        if (bin.left.id == bin.right.id) {
            replace_uses(func, result, bin.left.id);
            return true;
        }
        break;

    case BinOp::BitXor:
        // x ^ 0 → x
        if (right_const && *right_const == 0) {
            replace_uses(func, result, bin.left.id);
            return true;
        }
        // 0 ^ x → x
        if (left_const && *left_const == 0) {
            replace_uses(func, result, bin.right.id);
            return true;
        }
        // x ^ x → 0
        if (bin.left.id == bin.right.id) {
            ValueId zero = create_constant(func, 0, bin.result_type);
            replace_uses(func, result, zero);
            return true;
        }
        break;

    case BinOp::Shl:
    case BinOp::Shr:
        // x << 0 → x, x >> 0 → x
        if (right_const && *right_const == 0) {
            replace_uses(func, result, bin.left.id);
            return true;
        }
        break;

    default:
        break;
    }

    return false;
}

auto PeepholePass::simplify_unary(Function& func, BasicBlock& block, size_t idx, UnaryInst& unary)
    -> bool {
    // Look for double negation or double not
    // Find the definition of the operand
    for (const auto& blk : func.blocks) {
        for (const auto& inst : blk.instructions) {
            if (inst.result == unary.operand.id) {
                if (auto* inner_unary = std::get_if<UnaryInst>(&inst.inst)) {
                    // Double negation: --x → x
                    if (unary.op == UnaryOp::Neg && inner_unary->op == UnaryOp::Neg) {
                        ValueId result = block.instructions[idx].result;
                        replace_uses(func, result, inner_unary->operand.id);
                        return true;
                    }
                    // Double not: !!x → x (for boolean)
                    if (unary.op == UnaryOp::Not && inner_unary->op == UnaryOp::Not) {
                        ValueId result = block.instructions[idx].result;
                        replace_uses(func, result, inner_unary->operand.id);
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

auto PeepholePass::get_const_int(const Function& func, ValueId id) -> std::optional<int64_t> {
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

auto PeepholePass::replace_uses(Function& func, ValueId old_value, ValueId new_value) -> void {
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
                    } else if constexpr (std::is_same_v<T, CastInst>) {
                        if (i.operand.id == old_value)
                            i.operand.id = new_value;
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        if (i.condition.id == old_value)
                            i.condition.id = new_value;
                        if (i.true_val.id == old_value)
                            i.true_val.id = new_value;
                        if (i.false_val.id == old_value)
                            i.false_val.id = new_value;
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        if (i.ptr.id == old_value)
                            i.ptr.id = new_value;
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        if (i.ptr.id == old_value)
                            i.ptr.id = new_value;
                        if (i.value.id == old_value)
                            i.value.id = new_value;
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (auto& arg : i.args) {
                            if (arg.id == old_value)
                                arg.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, PhiInst>) {
                        for (auto& [val, _] : i.incoming) {
                            if (val.id == old_value)
                                val.id = new_value;
                        }
                    }
                },
                inst.inst);
        }

        // Update terminator
        if (block.terminator) {
            std::visit(
                [old_value, new_value](auto& t) {
                    using T = std::decay_t<decltype(t)>;

                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (t.value && t.value->id == old_value) {
                            t.value->id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        if (t.condition.id == old_value) {
                            t.condition.id = new_value;
                        }
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        if (t.discriminant.id == old_value) {
                            t.discriminant.id = new_value;
                        }
                    }
                },
                *block.terminator);
        }
    }
}

auto PeepholePass::create_constant(Function& func, int64_t value, const MirTypePtr& type)
    -> ValueId {
    // Find entry block
    if (func.blocks.empty()) {
        return INVALID_VALUE;
    }

    ValueId const_id = func.fresh_value();

    ConstantInst const_inst;
    const_inst.value = ConstInt{value, value < 0, 64};

    InstructionData const_data;
    const_data.result = const_id;
    const_data.type = type;
    const_data.inst = const_inst;

    // Insert at beginning of entry block
    func.blocks[0].instructions.insert(func.blocks[0].instructions.begin(), const_data);

    return const_id;
}

} // namespace tml::mir
