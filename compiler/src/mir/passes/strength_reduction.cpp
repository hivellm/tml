//! # Strength Reduction Pass
//!
//! Replaces expensive operations with cheaper equivalents.

#include "mir/passes/strength_reduction.hpp"

namespace tml::mir {

auto StrengthReductionPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    for (auto& block : func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& inst = block.instructions[i];

            if (auto* bin = std::get_if<BinaryInst>(&inst.inst)) {
                switch (bin->op) {
                case BinOp::Mul:
                    if (reduce_multiply(func, block, i, *bin)) {
                        changed = true;
                    }
                    break;
                case BinOp::Div:
                    if (reduce_divide(func, block, i, *bin)) {
                        changed = true;
                    }
                    break;
                case BinOp::Mod:
                    if (reduce_modulo(func, block, i, *bin)) {
                        changed = true;
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    return changed;
}

auto StrengthReductionPass::is_power_of_two(int64_t n) -> bool {
    return n > 0 && (n & (n - 1)) == 0;
}

auto StrengthReductionPass::log2_of(int64_t n) -> int {
    int log = 0;
    while (n > 1) {
        n >>= 1;
        log++;
    }
    return log;
}

auto StrengthReductionPass::get_const_int(const Function& func, ValueId id)
    -> std::optional<int64_t> {
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

auto StrengthReductionPass::reduce_multiply(Function& func, BasicBlock& block, size_t inst_idx,
                                            const BinaryInst& mul) -> bool {
    auto& inst = block.instructions[inst_idx];

    // Check for multiplication by power of 2
    auto right_const = get_const_int(func, mul.right.id);
    auto left_const = get_const_int(func, mul.left.id);

    int64_t multiplier = 0;
    ValueId operand = INVALID_VALUE;

    if (right_const && is_power_of_two(*right_const)) {
        multiplier = *right_const;
        operand = mul.left.id;
    } else if (left_const && is_power_of_two(*left_const)) {
        multiplier = *left_const;
        operand = mul.right.id;
    }

    if (multiplier > 0 && operand != INVALID_VALUE) {
        if (multiplier == 1) {
            // x * 1 = x (should be handled by InstSimplify, but just in case)
            return false;
        }

        // Replace mul with shl
        int shift = log2_of(multiplier);

        // Create a constant for the shift amount
        ValueId shift_const_id = func.fresh_value();
        ConstantInst const_inst;
        const_inst.value = ConstInt{shift, false, 32};

        InstructionData const_data;
        const_data.result = shift_const_id;
        const_data.type = make_i32_type();
        const_data.inst = const_inst;

        // Insert constant before the mul
        block.instructions.insert(
            block.instructions.begin() + static_cast<std::ptrdiff_t>(inst_idx), const_data);

        // Update the mul to be a shl
        BinaryInst shl;
        shl.op = BinOp::Shl;
        shl.left = mul.left;
        shl.left.id = operand;
        shl.right.id = shift_const_id;
        shl.right.type = make_i32_type();
        shl.result_type = mul.result_type;

        // The mul instruction is now at inst_idx + 1
        block.instructions[inst_idx + 1].inst = shl;

        return true;
    }

    // x * 2 -> x + x (might be faster on some architectures)
    if ((right_const && *right_const == 2) || (left_const && *left_const == 2)) {
        ValueId op = (right_const && *right_const == 2) ? mul.left.id : mul.right.id;

        BinaryInst add;
        add.op = BinOp::Add;
        add.left.id = op;
        add.left.type = mul.left.type;
        add.right.id = op;
        add.right.type = mul.right.type;
        add.result_type = mul.result_type;

        inst.inst = add;
        return true;
    }

    return false;
}

auto StrengthReductionPass::reduce_divide(Function& func, BasicBlock& block, size_t inst_idx,
                                          const BinaryInst& div) -> bool {
    // x / 2^n -> x >> n (for unsigned integers)
    auto right_const = get_const_int(func, div.right.id);

    if (!right_const || !is_power_of_two(*right_const)) {
        return false;
    }

    // Check if the type is unsigned (for signed, need arithmetic shift and adjustment)
    // For simplicity, we only handle this for power of 2 divisors
    // A more complete implementation would check signedness

    if (*right_const == 1) {
        // x / 1 = x (should be handled by InstSimplify)
        return false;
    }

    int shift = log2_of(*right_const);

    // Create a constant for the shift amount
    ValueId shift_const_id = func.fresh_value();
    ConstantInst const_inst;
    const_inst.value = ConstInt{shift, false, 32};

    InstructionData const_data;
    const_data.result = shift_const_id;
    const_data.type = make_i32_type();
    const_data.inst = const_inst;

    // Insert constant before the div
    block.instructions.insert(block.instructions.begin() + static_cast<std::ptrdiff_t>(inst_idx),
                              const_data);

    // Replace div with shr
    BinaryInst shr;
    shr.op = BinOp::Shr;
    shr.left = div.left;
    shr.right.id = shift_const_id;
    shr.right.type = make_i32_type();
    shr.result_type = div.result_type;

    // The div instruction is now at inst_idx + 1
    block.instructions[inst_idx + 1].inst = shr;

    return true;
}

auto StrengthReductionPass::reduce_modulo(Function& func, BasicBlock& block, size_t inst_idx,
                                          const BinaryInst& mod) -> bool {
    // x % 2^n -> x & (2^n - 1) (for unsigned integers)
    auto right_const = get_const_int(func, mod.right.id);

    if (!right_const || !is_power_of_two(*right_const)) {
        return false;
    }

    if (*right_const == 1) {
        // x % 1 = 0 (should create constant 0, but let other passes handle it)
        return false;
    }

    int64_t mask = *right_const - 1;

    // Create a constant for the mask
    ValueId mask_const_id = func.fresh_value();
    ConstantInst const_inst;
    const_inst.value = ConstInt{mask, false, 32};

    InstructionData const_data;
    const_data.result = mask_const_id;
    const_data.type = make_i32_type();
    const_data.inst = const_inst;

    // Insert constant before the mod
    block.instructions.insert(block.instructions.begin() + static_cast<std::ptrdiff_t>(inst_idx),
                              const_data);

    // Replace mod with bitand
    BinaryInst bitand_inst;
    bitand_inst.op = BinOp::BitAnd;
    bitand_inst.left = mod.left;
    bitand_inst.right.id = mask_const_id;
    bitand_inst.right.type = make_i32_type();
    bitand_inst.result_type = mod.result_type;

    // The mod instruction is now at inst_idx + 1
    block.instructions[inst_idx + 1].inst = bitand_inst;

    return true;
}

} // namespace tml::mir
