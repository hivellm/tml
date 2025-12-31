// Constant Folding Optimization Pass Implementation

#include "mir/passes/constant_folding.hpp"

#include <cmath>
#include <limits>

namespace tml::mir {

auto ConstantFoldingPass::run_on_block(BasicBlock& block, Function& func) -> bool {
    bool changed = false;

    for (auto& inst : block.instructions) {
        // Try to fold binary operations
        if (auto* bin = std::get_if<BinaryInst>(&inst.inst)) {
            auto left_const = get_constant_for_value(func, bin->left.id);
            auto right_const = get_constant_for_value(func, bin->right.id);

            if (left_const && right_const) {
                auto folded = try_fold_binary(bin->op, *left_const, *right_const);
                if (folded) {
                    // Replace with constant instruction
                    inst.inst = ConstantInst{*folded};
                    changed = true;
                }
            }
        }
        // Try to fold unary operations
        else if (auto* un = std::get_if<UnaryInst>(&inst.inst)) {
            auto operand_const = get_constant_for_value(func, un->operand.id);

            if (operand_const) {
                auto folded = try_fold_unary(un->op, *operand_const);
                if (folded) {
                    inst.inst = ConstantInst{*folded};
                    changed = true;
                }
            }
        }
        // Try to fold select with constant condition
        else if (auto* sel = std::get_if<SelectInst>(&inst.inst)) {
            auto cond_const = get_constant_for_value(func, sel->condition.id);

            if (cond_const) {
                if (std::get_if<ConstBool>(&*cond_const) != nullptr) {
                    // Replace select with the appropriate value
                    // We can't directly replace with a value reference,
                    // but we can mark this for copy propagation
                    // For now, just note that this could be optimized
                }
            }
        }
    }

    return changed;
}

auto ConstantFoldingPass::try_fold_binary(BinOp op, const Constant& left, const Constant& right)
    -> std::optional<Constant> {
    // Integer operations
    if (auto* left_int = std::get_if<ConstInt>(&left)) {
        if (auto* right_int = std::get_if<ConstInt>(&right)) {
            int64_t l = left_int->value;
            int64_t r = right_int->value;
            int64_t result = 0;
            bool is_comparison = false;

            switch (op) {
            case BinOp::Add:
                result = l + r;
                break;
            case BinOp::Sub:
                result = l - r;
                break;
            case BinOp::Mul:
                result = l * r;
                break;
            case BinOp::Div:
                if (r == 0)
                    return std::nullopt; // Avoid division by zero
                result =
                    left_int->is_signed
                        ? l / r
                        : static_cast<int64_t>(static_cast<uint64_t>(l) / static_cast<uint64_t>(r));
                break;
            case BinOp::Mod:
                if (r == 0)
                    return std::nullopt;
                result =
                    left_int->is_signed
                        ? l % r
                        : static_cast<int64_t>(static_cast<uint64_t>(l) % static_cast<uint64_t>(r));
                break;
            case BinOp::Eq:
                is_comparison = true;
                result = l == r ? 1 : 0;
                break;
            case BinOp::Ne:
                is_comparison = true;
                result = l != r ? 1 : 0;
                break;
            case BinOp::Lt:
                is_comparison = true;
                if (left_int->is_signed) {
                    result = l < r ? 1 : 0;
                } else {
                    result = static_cast<uint64_t>(l) < static_cast<uint64_t>(r) ? 1 : 0;
                }
                break;
            case BinOp::Le:
                is_comparison = true;
                if (left_int->is_signed) {
                    result = l <= r ? 1 : 0;
                } else {
                    result = static_cast<uint64_t>(l) <= static_cast<uint64_t>(r) ? 1 : 0;
                }
                break;
            case BinOp::Gt:
                is_comparison = true;
                if (left_int->is_signed) {
                    result = l > r ? 1 : 0;
                } else {
                    result = static_cast<uint64_t>(l) > static_cast<uint64_t>(r) ? 1 : 0;
                }
                break;
            case BinOp::Ge:
                is_comparison = true;
                if (left_int->is_signed) {
                    result = l >= r ? 1 : 0;
                } else {
                    result = static_cast<uint64_t>(l) >= static_cast<uint64_t>(r) ? 1 : 0;
                }
                break;
            case BinOp::BitAnd:
                result = l & r;
                break;
            case BinOp::BitOr:
                result = l | r;
                break;
            case BinOp::BitXor:
                result = l ^ r;
                break;
            case BinOp::Shl:
                result = l << r;
                break;
            case BinOp::Shr:
                if (left_int->is_signed) {
                    result = l >> r;
                } else {
                    result = static_cast<int64_t>(static_cast<uint64_t>(l) >> r);
                }
                break;
            default:
                return std::nullopt;
            }

            if (is_comparison) {
                return ConstBool{result != 0};
            } else {
                return ConstInt{result, left_int->is_signed, left_int->bit_width};
            }
        }
    }

    // Float operations
    if (auto* left_float = std::get_if<ConstFloat>(&left)) {
        if (auto* right_float = std::get_if<ConstFloat>(&right)) {
            double l = left_float->value;
            double r = right_float->value;
            double result = 0;
            bool is_comparison = false;

            switch (op) {
            case BinOp::Add:
                result = l + r;
                break;
            case BinOp::Sub:
                result = l - r;
                break;
            case BinOp::Mul:
                result = l * r;
                break;
            case BinOp::Div:
                if (r == 0.0)
                    return std::nullopt;
                result = l / r;
                break;
            case BinOp::Mod:
                if (r == 0.0)
                    return std::nullopt;
                result = std::fmod(l, r);
                break;
            case BinOp::Eq:
                is_comparison = true;
                result = l == r ? 1 : 0;
                break;
            case BinOp::Ne:
                is_comparison = true;
                result = l != r ? 1 : 0;
                break;
            case BinOp::Lt:
                is_comparison = true;
                result = l < r ? 1 : 0;
                break;
            case BinOp::Le:
                is_comparison = true;
                result = l <= r ? 1 : 0;
                break;
            case BinOp::Gt:
                is_comparison = true;
                result = l > r ? 1 : 0;
                break;
            case BinOp::Ge:
                is_comparison = true;
                result = l >= r ? 1 : 0;
                break;
            default:
                return std::nullopt;
            }

            if (is_comparison) {
                return ConstBool{result != 0};
            } else {
                return ConstFloat{result, left_float->is_f64};
            }
        }
    }

    // Boolean operations
    if (auto* left_bool = std::get_if<ConstBool>(&left)) {
        if (auto* right_bool = std::get_if<ConstBool>(&right)) {
            bool l = left_bool->value;
            bool r = right_bool->value;
            bool result = false;

            switch (op) {
            case BinOp::And:
                result = l && r;
                break;
            case BinOp::Or:
                result = l || r;
                break;
            case BinOp::Eq:
                result = l == r;
                break;
            case BinOp::Ne:
                result = l != r;
                break;
            default:
                return std::nullopt;
            }

            return ConstBool{result};
        }
    }

    return std::nullopt;
}

auto ConstantFoldingPass::try_fold_unary(UnaryOp op, const Constant& operand)
    -> std::optional<Constant> {
    // Integer operations
    if (auto* int_val = std::get_if<ConstInt>(&operand)) {
        switch (op) {
        case UnaryOp::Neg:
            return ConstInt{-int_val->value, int_val->is_signed, int_val->bit_width};
        case UnaryOp::BitNot:
            return ConstInt{~int_val->value, int_val->is_signed, int_val->bit_width};
        default:
            return std::nullopt;
        }
    }

    // Float operations
    if (auto* float_val = std::get_if<ConstFloat>(&operand)) {
        switch (op) {
        case UnaryOp::Neg:
            return ConstFloat{-float_val->value, float_val->is_f64};
        default:
            return std::nullopt;
        }
    }

    // Boolean operations
    if (auto* bool_val = std::get_if<ConstBool>(&operand)) {
        switch (op) {
        case UnaryOp::Not:
            return ConstBool{!bool_val->value};
        default:
            return std::nullopt;
        }
    }

    return std::nullopt;
}

auto ConstantFoldingPass::get_constant_for_value(const Function& func, ValueId id)
    -> std::optional<Constant> {
    // Search for the instruction that defines this value
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == id) {
                if (auto* ci = std::get_if<ConstantInst>(&inst.inst)) {
                    return ci->value;
                }
                return std::nullopt;
            }
        }
    }
    return std::nullopt;
}

} // namespace tml::mir
