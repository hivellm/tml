//! # Early Common Subexpression Elimination Pass
//!
//! Performs local CSE early in the pipeline.

#include "mir/passes/early_cse.hpp"

#include <algorithm>

namespace tml::mir {

auto EarlyCSEPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    for (auto& block : func.blocks) {
        changed |= process_block(block);
    }

    return changed;
}

auto EarlyCSEPass::process_block(BasicBlock& block) -> bool {
    bool changed = false;
    std::unordered_map<ExprKey, ValueId, ExprKeyHash> expr_map;
    std::vector<size_t> dead_insts;

    for (size_t i = 0; i < block.instructions.size(); ++i) {
        auto& inst = block.instructions[i];

        auto key = get_expr_key(inst.inst);
        if (!key)
            continue;

        auto it = expr_map.find(*key);
        if (it != expr_map.end()) {
            // Found a duplicate expression - replace uses
            ValueId old_val = inst.result;
            ValueId new_val = it->second;

            replace_uses_in_block(block, i + 1, old_val, new_val);
            dead_insts.push_back(i);
            changed = true;
        } else {
            // First time seeing this expression
            expr_map[*key] = inst.result;
        }
    }

    // Remove dead instructions in reverse order
    std::sort(dead_insts.begin(), dead_insts.end(), std::greater<size_t>());
    for (size_t idx : dead_insts) {
        block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(idx));
    }

    return changed;
}

auto EarlyCSEPass::get_expr_key(const Instruction& inst) -> std::optional<ExprKey> {
    return std::visit(
        [](const auto& i) -> std::optional<ExprKey> {
            using T = std::decay_t<decltype(i)>;

            if constexpr (std::is_same_v<T, BinaryInst>) {
                ExprKey key;
                key.op = "binary_" + std::to_string(static_cast<int>(i.op));
                key.operands = {i.left.id, i.right.id};

                // For commutative operations, sort operands for canonical form
                if (i.op == BinOp::Add || i.op == BinOp::Mul || i.op == BinOp::BitAnd ||
                    i.op == BinOp::BitOr || i.op == BinOp::BitXor || i.op == BinOp::Eq ||
                    i.op == BinOp::Ne) {
                    if (key.operands[0] > key.operands[1]) {
                        std::swap(key.operands[0], key.operands[1]);
                    }
                }

                return key;
            } else if constexpr (std::is_same_v<T, UnaryInst>) {
                ExprKey key;
                key.op = "unary_" + std::to_string(static_cast<int>(i.op));
                key.operands = {i.operand.id};
                return key;
            } else if constexpr (std::is_same_v<T, CastInst>) {
                ExprKey key;
                key.op = "cast_" + std::to_string(static_cast<int>(i.kind));
                key.operands = {i.operand.id};
                return key;
            } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                ExprKey key;
                key.op = "gep";
                key.operands.push_back(i.base.id);
                for (const auto& idx : i.indices) {
                    key.operands.push_back(idx.id);
                }
                return key;
            } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                ExprKey key;
                key.op = "extract";
                key.operands.push_back(i.aggregate.id);
                // Include indices in the key
                for (uint32_t idx : i.indices) {
                    key.op += "_" + std::to_string(idx);
                }
                return key;
            } else {
                // Don't CSE: loads, stores, calls, allocas, phis, etc.
                return std::nullopt;
            }
        },
        inst);
}

auto EarlyCSEPass::replace_uses_in_block(BasicBlock& block, size_t start_idx, ValueId old_val,
                                         ValueId new_val) -> void {
    for (size_t i = start_idx; i < block.instructions.size(); ++i) {
        std::visit(
            [old_val, new_val](auto& inst) {
                using T = std::decay_t<decltype(inst)>;

                if constexpr (std::is_same_v<T, BinaryInst>) {
                    if (inst.left.id == old_val)
                        inst.left.id = new_val;
                    if (inst.right.id == old_val)
                        inst.right.id = new_val;
                } else if constexpr (std::is_same_v<T, UnaryInst>) {
                    if (inst.operand.id == old_val)
                        inst.operand.id = new_val;
                } else if constexpr (std::is_same_v<T, CastInst>) {
                    if (inst.operand.id == old_val)
                        inst.operand.id = new_val;
                } else if constexpr (std::is_same_v<T, LoadInst>) {
                    if (inst.ptr.id == old_val)
                        inst.ptr.id = new_val;
                } else if constexpr (std::is_same_v<T, StoreInst>) {
                    if (inst.ptr.id == old_val)
                        inst.ptr.id = new_val;
                    if (inst.value.id == old_val)
                        inst.value.id = new_val;
                } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                    if (inst.base.id == old_val)
                        inst.base.id = new_val;
                    for (auto& idx : inst.indices) {
                        if (idx.id == old_val)
                            idx.id = new_val;
                    }
                } else if constexpr (std::is_same_v<T, CallInst>) {
                    for (auto& arg : inst.args) {
                        if (arg.id == old_val)
                            arg.id = new_val;
                    }
                } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                    if (inst.receiver.id == old_val)
                        inst.receiver.id = new_val;
                    for (auto& arg : inst.args) {
                        if (arg.id == old_val)
                            arg.id = new_val;
                    }
                } else if constexpr (std::is_same_v<T, SelectInst>) {
                    if (inst.condition.id == old_val)
                        inst.condition.id = new_val;
                    if (inst.true_val.id == old_val)
                        inst.true_val.id = new_val;
                    if (inst.false_val.id == old_val)
                        inst.false_val.id = new_val;
                } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                    if (inst.aggregate.id == old_val)
                        inst.aggregate.id = new_val;
                } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                    if (inst.aggregate.id == old_val)
                        inst.aggregate.id = new_val;
                    if (inst.value.id == old_val)
                        inst.value.id = new_val;
                }
            },
            block.instructions[i].inst);
    }

    // Also update terminator
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

} // namespace tml::mir
