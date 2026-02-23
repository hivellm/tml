TML_MODULE("compiler")

//! # Reassociation Pass
//!
//! Reorders associative operations to expose optimization opportunities.

#include "mir/passes/reassociate.hpp"

#include <algorithm>

namespace tml::mir {

auto ReassociatePass::run_on_function(Function& func) -> bool {
    bool changed = false;

    // Compute ranks for all values
    compute_ranks(func);

    // Try to reassociate binary operations
    for (auto& block : func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            if (try_reassociate(func, block, i)) {
                changed = true;
            }
        }
    }

    return changed;
}

auto ReassociatePass::compute_ranks(Function& func) -> void {
    ranks_.clear();

    // Parameters get rank 0
    for (const auto& param : func.params) {
        ranks_[param.value_id] = 0;
    }

    // Process blocks in order
    int next_rank = 1;
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result != INVALID_VALUE) {
                // Constants get highest rank (so they sort to the end)
                if (is_constant(func, inst.result)) {
                    ranks_[inst.result] = INT32_MAX;
                } else {
                    ranks_[inst.result] = next_rank++;
                }
            }
        }
    }
}

auto ReassociatePass::get_rank(ValueId id) -> int {
    auto it = ranks_.find(id);
    if (it != ranks_.end()) {
        return it->second;
    }
    return 0;
}

auto ReassociatePass::is_associative(BinOp op) -> bool {
    switch (op) {
    case BinOp::Add:
    case BinOp::Mul:
    case BinOp::BitAnd:
    case BinOp::BitOr:
    case BinOp::BitXor:
        return true;
    default:
        return false;
    }
}

auto ReassociatePass::try_reassociate(Function& func, BasicBlock& block, size_t inst_idx) -> bool {
    auto& inst = block.instructions[inst_idx];
    auto* bin = std::get_if<BinaryInst>(&inst.inst);
    if (!bin) {
        return false;
    }

    if (!is_associative(bin->op)) {
        return false;
    }

    // Check if we can benefit from reassociation
    // Simple case: (x + c1) + c2 -> x + (c1 + c2)
    auto* left_def = find_def(func, bin->left.id);
    if (left_def && left_def->op == bin->op) {
        // We have a chain of at least 2 operations
        bool left_has_const = is_constant(func, left_def->right.id);
        bool right_is_const = is_constant(func, bin->right.id);

        if (left_has_const && right_is_const) {
            // Both have constants - opportunity for constant folding
            // Linearize and rebuild
            std::vector<ValueId> operands;
            linearize(func, inst.result, bin->op, operands);

            if (operands.size() > 2) {
                // Sort: non-constants first (by rank), constants last
                std::sort(operands.begin(), operands.end(), [this, &func](ValueId a, ValueId b) {
                    bool a_const = is_constant(func, a);
                    bool b_const = is_constant(func, b);
                    if (a_const != b_const) {
                        return !a_const; // Non-constants first
                    }
                    return get_rank(a) < get_rank(b);
                });

                // For now, just ensure constants are grouped at the end
                // A more complete implementation would rebuild the tree
                return false; // Let constant folding handle it
            }
        }
    }

    // Simple canonicalization: ensure lower rank operand is on the left
    // This helps CSE find common subexpressions
    int left_rank = get_rank(bin->left.id);
    int right_rank = get_rank(bin->right.id);

    if (right_rank < left_rank) {
        // Swap operands (operation is commutative)
        std::swap(bin->left, bin->right);
        return true;
    }

    return false;
}

auto ReassociatePass::linearize(Function& func, ValueId root, BinOp op,
                                std::vector<ValueId>& operands) -> void {
    auto* def = find_def(func, root);
    if (!def || def->op != op) {
        // Leaf node
        operands.push_back(root);
        return;
    }

    // Recurse on both operands
    linearize(func, def->left.id, op, operands);
    linearize(func, def->right.id, op, operands);
}

auto ReassociatePass::rebuild_tree(Function& func, BasicBlock& block, size_t insert_pos,
                                   const std::vector<ValueId>& operands, BinOp op,
                                   MirTypePtr result_type) -> ValueId {
    if (operands.empty()) {
        return INVALID_VALUE;
    }
    if (operands.size() == 1) {
        return operands[0];
    }

    // Build a left-associative tree
    ValueId result = operands[0];
    for (size_t i = 1; i < operands.size(); ++i) {
        ValueId new_result = func.fresh_value();

        BinaryInst bin;
        bin.op = op;
        bin.left.id = result;
        bin.left.type = result_type;
        bin.right.id = operands[i];
        bin.right.type = result_type;
        bin.result_type = result_type;

        InstructionData data;
        data.result = new_result;
        data.type = result_type;
        data.inst = bin;

        block.instructions.insert(
            block.instructions.begin() + static_cast<std::ptrdiff_t>(insert_pos + i - 1), data);

        result = new_result;
    }

    return result;
}

auto ReassociatePass::get_const_int(const Function& func, ValueId id) -> std::optional<int64_t> {
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

auto ReassociatePass::is_constant(const Function& func, ValueId id) -> bool {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == id) {
                return std::holds_alternative<ConstantInst>(inst.inst);
            }
        }
    }
    return false;
}

auto ReassociatePass::find_def(const Function& func, ValueId id) -> const BinaryInst* {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == id) {
                return std::get_if<BinaryInst>(&inst.inst);
            }
        }
    }
    return nullptr;
}

} // namespace tml::mir
