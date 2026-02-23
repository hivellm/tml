TML_MODULE("compiler")

//! # Global Value Numbering (GVN) Pass
//!
//! Eliminates redundant computations across basic blocks.

#include "mir/passes/gvn.hpp"

#include "mir/passes/alias_analysis.hpp"

#include <sstream>

namespace tml::mir {

auto GVNPass::run_on_function(Function& func) -> bool {
    reset();
    bool changed = false;

    // Process blocks in dominator order
    auto order = compute_dominator_order(func);

    for (size_t block_idx : order) {
        auto& block = func.blocks[block_idx];
        std::vector<size_t> to_remove;

        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& inst = block.instructions[i];

            // Assign value numbers to all results
            if (inst.result != INVALID_VALUE &&
                value_numbers_.find(inst.result) == value_numbers_.end()) {
                value_numbers_[inst.result] = next_vn_++;
            }

            // Handle stores: invalidate loads that may alias
            if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
                invalidate_loads_for_store(store->ptr.id);
                continue;
            }

            // Handle calls: conservatively invalidate all loads
            if (std::holds_alternative<CallInst>(inst.inst) ||
                std::holds_alternative<MethodCallInst>(inst.inst)) {
                load_table_.clear(); // Calls may modify any memory
                continue;
            }

            // Handle loads with alias analysis (Load GVN)
            if (auto* load = std::get_if<LoadInst>(&inst.inst)) {
                if (alias_analysis_) {
                    auto available = find_available_load(load->ptr.id, block_idx);
                    if (available) {
                        // Found a redundant load!
                        replace_uses(func, inst.result, *available);
                        to_remove.push_back(i);
                        changed = true;
                        continue;
                    }

                    // Record this load for future elimination
                    ValueNumber ptr_vn = get_value_number(load->ptr.id);
                    load_table_[ptr_vn] = LoadInfo{inst.result, load->ptr.id, block_idx};
                }
                continue;
            }

            if (!can_gvn(inst.inst)) {
                continue;
            }

            auto expr = make_expression(inst.inst);
            if (!expr) {
                continue;
            }

            auto it = expr_table_.find(*expr);
            if (it != expr_table_.end()) {
                // Found a redundant expression!
                ValueId existing_value = it->second.second;

                // Replace uses of this result with the existing value
                replace_uses(func, inst.result, existing_value);
                to_remove.push_back(i);
                changed = true;
            } else {
                // First time seeing this expression
                ValueNumber vn = value_numbers_[inst.result];
                expr_table_[*expr] = {vn, inst.result};
            }
        }

        // Remove redundant instructions (in reverse order)
        for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
            block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(*it));
        }
    }

    return changed;
}

auto GVNPass::compute_dominator_order(const Function& func) -> std::vector<size_t> {
    // Simplified: assume blocks are already in rough dominator order
    // (entry block dominates all, structured control flow)
    std::vector<size_t> order;
    order.reserve(func.blocks.size());
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        order.push_back(i);
    }
    return order;
}

auto GVNPass::get_value_number(ValueId id) -> ValueNumber {
    auto it = value_numbers_.find(id);
    if (it != value_numbers_.end()) {
        return it->second;
    }
    // Assign a new value number
    ValueNumber vn = next_vn_++;
    value_numbers_[id] = vn;
    return vn;
}

auto GVNPass::make_expression(const Instruction& inst) -> std::optional<Expression> {
    std::ostringstream oss;

    std::visit(
        [&](const auto& i) {
            using T = std::decay_t<decltype(i)>;

            if constexpr (std::is_same_v<T, BinaryInst>) {
                // Use value numbers for operands to enable cross-block matching
                ValueNumber left_vn = get_value_number(i.left.id);
                ValueNumber right_vn = get_value_number(i.right.id);

                oss << "binary:" << static_cast<int>(i.op) << ":";

                // For commutative operations, canonicalize operand order
                bool commutative =
                    (i.op == BinOp::Add || i.op == BinOp::Mul || i.op == BinOp::BitAnd ||
                     i.op == BinOp::BitOr || i.op == BinOp::BitXor || i.op == BinOp::Eq ||
                     i.op == BinOp::Ne);

                if (commutative && left_vn > right_vn) {
                    oss << right_vn << ":" << left_vn;
                } else {
                    oss << left_vn << ":" << right_vn;
                }
            } else if constexpr (std::is_same_v<T, UnaryInst>) {
                ValueNumber operand_vn = get_value_number(i.operand.id);
                oss << "unary:" << static_cast<int>(i.op) << ":" << operand_vn;
            } else if constexpr (std::is_same_v<T, CastInst>) {
                ValueNumber operand_vn = get_value_number(i.operand.id);
                oss << "cast:" << static_cast<int>(i.kind) << ":" << operand_vn;
            } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                ValueNumber base_vn = get_value_number(i.base.id);
                oss << "gep:" << base_vn;
                for (const auto& idx : i.indices) {
                    oss << ":" << get_value_number(idx.id);
                }
            } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                ValueNumber agg_vn = get_value_number(i.aggregate.id);
                oss << "extract:" << agg_vn;
                for (auto idx : i.indices) {
                    oss << ":" << idx;
                }
            } else if constexpr (std::is_same_v<T, SelectInst>) {
                ValueNumber cond_vn = get_value_number(i.condition.id);
                ValueNumber true_vn = get_value_number(i.true_val.id);
                ValueNumber false_vn = get_value_number(i.false_val.id);
                oss << "select:" << cond_vn << ":" << true_vn << ":" << false_vn;
            } else {
                // Not a GVN-able instruction
                oss << "";
            }
        },
        inst);

    std::string key = oss.str();
    if (key.empty()) {
        return std::nullopt;
    }

    return Expression{key};
}

auto GVNPass::can_gvn(const Instruction& inst) -> bool {
    return std::visit(
        [](const auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;
            (void)i; // Silence unused parameter warning

            // Pure expressions that can be GVN'd
            if constexpr (std::is_same_v<T, BinaryInst> || std::is_same_v<T, UnaryInst> ||
                          std::is_same_v<T, CastInst> || std::is_same_v<T, GetElementPtrInst> ||
                          std::is_same_v<T, ExtractValueInst> || std::is_same_v<T, SelectInst>) {
                return true;
            } else {
                return false;
            }
        },
        inst);
}

auto GVNPass::replace_uses(Function& func, ValueId old_value, ValueId new_value) -> void {
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

auto GVNPass::reset() -> void {
    value_numbers_.clear();
    expr_table_.clear();
    load_table_.clear();
    next_vn_ = 0;
}

auto GVNPass::find_available_load(ValueId ptr, size_t /*current_block_idx*/)
    -> std::optional<ValueId> {
    // Get value number for the pointer
    ValueNumber ptr_vn = get_value_number(ptr);

    // Look for an existing load from the same pointer (by VN)
    auto it = load_table_.find(ptr_vn);
    if (it != load_table_.end()) {
        // Found a previous load from the same address
        return it->second.result;
    }

    return std::nullopt;
}

auto GVNPass::invalidate_loads_for_store(ValueId store_ptr) -> void {
    if (!alias_analysis_) {
        // Without alias analysis, conservatively clear all loads
        load_table_.clear();
        return;
    }

    // With alias analysis, only invalidate loads that may alias with the store
    std::vector<ValueNumber> to_invalidate;

    for (const auto& [ptr_vn, load_info] : load_table_) {
        // Check if the stored pointer may alias with this load's pointer
        auto result = alias_analysis_->alias(store_ptr, load_info.ptr);
        if (result != AliasResult::NoAlias) {
            to_invalidate.push_back(ptr_vn);
        }
    }

    for (ValueNumber vn : to_invalidate) {
        load_table_.erase(vn);
    }
}

} // namespace tml::mir
