//! # Narrowing Pass
//!
//! Replaces operations on wider types with narrower types when safe.

#include "mir/passes/narrowing.hpp"

namespace tml::mir {

auto NarrowingPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    for (auto& block : func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            // Look for trunc instructions - they indicate potential narrowing
            auto& inst = block.instructions[i];
            if (auto* cast = std::get_if<CastInst>(&inst.inst)) {
                if (cast->kind == CastKind::Trunc) {
                    // See if the operand is a binary op on zext'd values
                    if (try_narrow_zext_pattern(func, block, i)) {
                        changed = true;
                    }
                }
            }
        }
    }

    return changed;
}

auto NarrowingPass::try_narrow_zext_pattern(Function& func, BasicBlock& block, size_t inst_idx)
    -> bool {
    auto& trunc_inst = block.instructions[inst_idx];
    auto* trunc = std::get_if<CastInst>(&trunc_inst.inst);
    if (!trunc || trunc->kind != CastKind::Trunc) {
        return false;
    }

    // Find the definition of the truncated value
    auto* op_def = find_def_inst(func, trunc->operand.id);
    if (!op_def) {
        return false;
    }

    // Check if it's a binary operation
    auto* bin_op = std::get_if<BinaryInst>(&op_def->inst);
    if (!bin_op) {
        return false;
    }

    // Check if the operation is safe to narrow
    if (!is_safe_to_narrow(bin_op->op)) {
        return false;
    }

    // Check if both operands come from zext
    auto* left_def = find_def_inst(func, bin_op->left.id);
    auto* right_def = find_def_inst(func, bin_op->right.id);

    if (!left_def || !right_def) {
        return false;
    }

    auto* left_zext = std::get_if<CastInst>(&left_def->inst);
    auto* right_zext = std::get_if<CastInst>(&right_def->inst);

    // At least one must be a zext (the other could be a constant)
    bool left_is_zext = left_zext && left_zext->kind == CastKind::ZExt;
    bool right_is_zext = right_zext && right_zext->kind == CastKind::ZExt;

    if (!left_is_zext && !right_is_zext) {
        return false;
    }

    // Get the target (narrow) bit width
    int target_bits = get_bit_width(trunc->target_type);
    if (target_bits <= 0) {
        return false;
    }

    // Check that the zext sources have the same width as the target
    if (left_is_zext) {
        int src_bits = get_bit_width(left_zext->source_type);
        if (src_bits != target_bits) {
            return false;
        }
    }
    if (right_is_zext) {
        int src_bits = get_bit_width(right_zext->source_type);
        if (src_bits != target_bits) {
            return false;
        }
    }

    // Check that the binary op result is only used by this trunc
    if (!is_only_used_by_trunc(func, op_def->result, target_bits)) {
        return false;
    }

    // We can narrow!
    // Replace the trunc with a narrow binary op
    ValueId left_narrow = left_is_zext ? left_zext->operand.id : bin_op->left.id;
    ValueId right_narrow = right_is_zext ? right_zext->operand.id : bin_op->right.id;

    BinaryInst narrow_bin;
    narrow_bin.op = bin_op->op;
    narrow_bin.left.id = left_narrow;
    narrow_bin.left.type = trunc->target_type;
    narrow_bin.right.id = right_narrow;
    narrow_bin.right.type = trunc->target_type;
    narrow_bin.result_type = trunc->target_type;

    // Replace the trunc instruction with the narrow binary
    trunc_inst.inst = narrow_bin;

    return true;
}

auto NarrowingPass::try_narrow_sext_pattern(Function& /*func*/, BasicBlock& /*block*/,
                                            size_t /*inst_idx*/) -> bool {
    // Similar to zext pattern but for signed values
    // More complex due to sign extension semantics
    return false;
}

auto NarrowingPass::is_only_used_by_trunc(const Function& func, ValueId value, int target_bits)
    -> bool {
    int use_count = 0;
    int trunc_use_count = 0;

    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            bool uses_value = std::visit(
                [value](const auto& i) -> bool {
                    using T = std::decay_t<decltype(i)>;
                    (void)i;

                    if constexpr (std::is_same_v<T, CastInst>) {
                        return i.operand.id == value;
                    } else if constexpr (std::is_same_v<T, BinaryInst>) {
                        return i.left.id == value || i.right.id == value;
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        return i.operand.id == value;
                    } else {
                        return false;
                    }
                },
                inst.inst);

            if (uses_value) {
                use_count++;

                if (auto* cast = std::get_if<CastInst>(&inst.inst)) {
                    if (cast->kind == CastKind::Trunc) {
                        int trunc_bits = get_bit_width(cast->target_type);
                        if (trunc_bits == target_bits) {
                            trunc_use_count++;
                        }
                    }
                }
            }
        }

        // Check terminator
        if (block.terminator) {
            std::visit(
                [value, &use_count](const auto& t) {
                    using T = std::decay_t<decltype(t)>;

                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (t.value && t.value->id == value) {
                            use_count++;
                        }
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        if (t.condition.id == value) {
                            use_count++;
                        }
                    }
                },
                *block.terminator);
        }
    }

    return use_count > 0 && use_count == trunc_use_count;
}

auto NarrowingPass::find_def_inst(const Function& func, ValueId id) -> const InstructionData* {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == id) {
                return &inst;
            }
        }
    }
    return nullptr;
}

auto NarrowingPass::get_bit_width(const MirTypePtr& type) -> int {
    if (!type)
        return -1;

    if (auto* prim = std::get_if<MirPrimitiveType>(&type->kind)) {
        switch (prim->kind) {
        case PrimitiveType::I8:
        case PrimitiveType::U8:
            return 8;
        case PrimitiveType::I16:
        case PrimitiveType::U16:
            return 16;
        case PrimitiveType::I32:
        case PrimitiveType::U32:
            return 32;
        case PrimitiveType::I64:
        case PrimitiveType::U64:
            return 64;
        case PrimitiveType::I128:
        case PrimitiveType::U128:
            return 128;
        default:
            return -1;
        }
    }

    return -1;
}

auto NarrowingPass::is_safe_to_narrow(BinOp op) -> bool {
    switch (op) {
    case BinOp::Add:
    case BinOp::Sub:
    case BinOp::Mul:
    case BinOp::BitAnd:
    case BinOp::BitOr:
    case BinOp::BitXor:
        return true;
    default:
        // Div, Mod, shifts, comparisons need more careful handling
        return false;
    }
}

auto NarrowingPass::replace_uses(Function& func, ValueId old_value, ValueId new_value) -> void {
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
                    }
                    // Add more as needed
                },
                inst.inst);
        }

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
                    }
                },
                *block.terminator);
        }
    }
}

} // namespace tml::mir
