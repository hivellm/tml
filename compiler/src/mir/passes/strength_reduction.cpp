TML_MODULE("compiler")

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
                                            const BinaryInst& /*mul_ref*/) -> bool {
    // IMPORTANT: Copy the BinaryInst by value. The reference points into
    // block.instructions which may reallocate on vector::insert(), causing UB.
    BinaryInst mul = *std::get_if<BinaryInst>(&block.instructions[inst_idx].inst);

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

        block.instructions[inst_idx].inst = add;
        return true;
    }

    // Determine the constant multiplier (from either side)
    int64_t const_val = 0;
    ValueId var_operand = INVALID_VALUE;
    if (right_const) {
        const_val = *right_const;
        var_operand = mul.left.id;
    } else if (left_const) {
        const_val = *left_const;
        var_operand = mul.right.id;
    }

    if (var_operand == INVALID_VALUE) {
        return false;
    }

    // x * -1 -> 0 - x (negate)
    if (const_val == -1) {
        ValueId zero_id = func.fresh_value();
        ConstantInst zero_inst;
        zero_inst.value = ConstInt{0, false, 32};
        InstructionData zero_data;
        zero_data.result = zero_id;
        zero_data.type = mul.result_type;
        zero_data.inst = zero_inst;
        block.instructions.insert(
            block.instructions.begin() + static_cast<std::ptrdiff_t>(inst_idx), zero_data);

        BinaryInst sub;
        sub.op = BinOp::Sub;
        sub.left.id = zero_id;
        sub.left.type = mul.result_type;
        sub.right.id = var_operand;
        sub.right.type = mul.result_type;
        sub.result_type = mul.result_type;
        block.instructions[inst_idx + 1].inst = sub;
        return true;
    }

    // x * (2^n + 1) -> (x << n) + x  for 3, 5, 9
    // x * (2^n - 1) -> (x << n) - x  for 7
    // These use LEA-friendly patterns and avoid mul latency.
    struct ShiftAddPattern {
        int64_t multiplier;
        int shift;
        BinOp combine_op;
    };
    static constexpr ShiftAddPattern patterns[] = {
        {3, 1, BinOp::Add}, // x*3 = (x<<1) + x
        {5, 2, BinOp::Add}, // x*5 = (x<<2) + x
        {7, 3, BinOp::Sub}, // x*7 = (x<<3) - x
        {9, 3, BinOp::Add}, // x*9 = (x<<3) + x
    };

    for (const auto& pat : patterns) {
        if (const_val == pat.multiplier) {
            // Create shift constant
            ValueId shift_const_id = func.fresh_value();
            ConstantInst shift_ci;
            shift_ci.value = ConstInt{pat.shift, false, 32};
            InstructionData shift_data;
            shift_data.result = shift_const_id;
            shift_data.type = make_i32_type();
            shift_data.inst = shift_ci;

            // Create shl instruction: x << n
            ValueId shl_id = func.fresh_value();
            BinaryInst shl;
            shl.op = BinOp::Shl;
            shl.left.id = var_operand;
            shl.left.type = mul.result_type;
            shl.right.id = shift_const_id;
            shl.right.type = make_i32_type();
            shl.result_type = mul.result_type;
            InstructionData shl_data;
            shl_data.result = shl_id;
            shl_data.type = mul.result_type;
            shl_data.inst = shl;

            // Insert shift constant and shl before the mul (order matters:
            // shift_data must come first since shl_data references shift_const_id).
            // Use index-based insertion since iterators are invalidated after insert.
            block.instructions.insert(
                block.instructions.begin() + static_cast<std::ptrdiff_t>(inst_idx), shift_data);
            block.instructions.insert(
                block.instructions.begin() + static_cast<std::ptrdiff_t>(inst_idx + 1), shl_data);

            // The mul is now at inst_idx + 2; replace with add/sub
            BinaryInst combine;
            combine.op = pat.combine_op;
            combine.left.id = shl_id;
            combine.left.type = mul.result_type;
            combine.right.id = var_operand;
            combine.right.type = mul.result_type;
            combine.result_type = mul.result_type;
            block.instructions[inst_idx + 2].inst = combine;
            return true;
        }
    }

    return false;
}

auto StrengthReductionPass::reduce_divide(Function& func, BasicBlock& block, size_t inst_idx,
                                          const BinaryInst& /*div_ref*/) -> bool {
    // IMPORTANT: Copy by value — reference may be invalidated by vector::insert().
    BinaryInst div = *std::get_if<BinaryInst>(&block.instructions[inst_idx].inst);

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
                                          const BinaryInst& /*mod_ref*/) -> bool {
    // IMPORTANT: Copy by value — reference may be invalidated by vector::insert().
    BinaryInst mod = *std::get_if<BinaryInst>(&block.instructions[inst_idx].inst);

    // x % 2^n -> x & (2^n - 1) (for unsigned integers)
    auto right_const = get_const_int(func, mod.right.id);

    if (!right_const || !is_power_of_two(*right_const)) {
        return false;
    }

    if (*right_const == 1) {
        // x % 1 = 0 — replace with constant zero
        ConstantInst zero_inst;
        zero_inst.value = ConstInt{0, false, 32};
        block.instructions[inst_idx].inst = zero_inst;
        return true;
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
