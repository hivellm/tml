TML_MODULE("compiler")

//! # Common Subexpression Elimination (CSE) Pass
//!
//! This pass eliminates redundant computations within basic blocks.
//!
//! ## Algorithm
//!
//! For each block:
//! 1. Hash each eligible expression to a key
//! 2. If key seen before, replace uses with previous result
//! 3. Otherwise, record expression for future matches
//!
//! ## Expression Key Format
//!
//! | Instruction   | Key Format                        |
//! |---------------|-----------------------------------|
//! | Binary        | `binary:<op>:<left>:<right>`      |
//! | Unary         | `unary:<op>:<operand>`            |
//! | Cast          | `cast:<kind>:<operand>`           |
//! | GEP           | `gep:<base>:<idx1>:<idx2>:...`    |
//! | ExtractValue  | `extract:<agg>:<idx1>:<idx2>:...` |
//!
//! ## Not Eligible for CSE
//!
//! - Load: may read different values
//! - Store: has side effects
//! - Alloca: creates unique memory
//! - Call/MethodCall: may have side effects
//! - Phi: block-specific semantics

#include "mir/passes/common_subexpression_elimination.hpp"

#include <sstream>

namespace tml::mir {

auto CommonSubexpressionEliminationPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    for (auto& block : func.blocks) {
        // Map from expression key to the value that computes it
        std::unordered_map<ExprKey, ValueId, ExprKeyHash> expr_to_value;

        // Process instructions in order
        std::vector<size_t> to_remove;

        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& inst = block.instructions[i];

            if (!can_cse(inst.inst)) {
                continue;
            }

            auto key = make_expr_key(inst.inst);
            if (!key.has_value()) {
                continue;
            }

            auto it = expr_to_value.find(*key);
            if (it != expr_to_value.end()) {
                // Found a duplicate expression!
                // Replace all uses of this result with the previous result
                replace_uses(func, inst.result, it->second);
                to_remove.push_back(i);
                changed = true;
            } else {
                // First time seeing this expression
                expr_to_value[*key] = inst.result;
            }
        }

        // Remove redundant instructions (in reverse order to preserve indices)
        for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
            block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(*it));
        }
    }

    return changed;
}

auto CommonSubexpressionEliminationPass::make_expr_key(const Instruction& inst)
    -> std::optional<ExprKey> {
    std::ostringstream oss;

    std::visit(
        [&oss](const auto& i) {
            using T = std::decay_t<decltype(i)>;

            if constexpr (std::is_same_v<T, BinaryInst>) {
                // Key: "binary:<op>:<left>:<right>"
                oss << "binary:" << static_cast<int>(i.op) << ":" << i.left.id << ":" << i.right.id;
            } else if constexpr (std::is_same_v<T, UnaryInst>) {
                // Key: "unary:<op>:<operand>"
                oss << "unary:" << static_cast<int>(i.op) << ":" << i.operand.id;
            } else if constexpr (std::is_same_v<T, CastInst>) {
                // Key: "cast:<kind>:<operand>:<target_type_hash>"
                // For simplicity, use string representation of target type
                oss << "cast:" << static_cast<int>(i.kind) << ":" << i.operand.id;
            } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                // Key: "gep:<base>:<indices...>"
                oss << "gep:" << i.base.id;
                for (const auto& idx : i.indices) {
                    oss << ":" << idx.id;
                }
            } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                // Key: "extract:<aggregate>:<indices...>"
                oss << "extract:" << i.aggregate.id;
                for (auto idx : i.indices) {
                    oss << ":" << idx;
                }
            } else {
                // Not eligible for CSE
                oss << "";
            }
        },
        inst);

    std::string key = oss.str();
    if (key.empty()) {
        return std::nullopt;
    }

    return ExprKey{key};
}

auto CommonSubexpressionEliminationPass::can_cse(const Instruction& inst) -> bool {
    return std::visit(
        [](const auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;

            // Pure expressions that can be CSE'd
            if constexpr (std::is_same_v<T, BinaryInst>) {
                return true;
            } else if constexpr (std::is_same_v<T, UnaryInst>) {
                return true;
            } else if constexpr (std::is_same_v<T, CastInst>) {
                return true;
            } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                return true;
            } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                return true;
            } else {
                // Not eligible for CSE:
                // - LoadInst: might read different values
                // - StoreInst: has side effects
                // - AllocaInst: creates new memory
                // - CallInst: might have side effects
                // - MethodCallInst: might have side effects
                // - PhiInst: block-specific
                // - ConstantInst: already constant
                // - SelectInst: could be CSE'd but not worth it usually
                // - StructInitInst, EnumInitInst, etc.: create new values
                return false;
            }
        },
        inst);
}

auto CommonSubexpressionEliminationPass::replace_uses(Function& func, ValueId old_value,
                                                      ValueId new_value) -> void {
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
                    // ConstantInst and AllocaInst have no value operands
                },
                inst.inst);
        }

        // Also update terminators
        if (block.terminator.has_value()) {
            std::visit(
                [old_value, new_value](auto& t) {
                    using T = std::decay_t<decltype(t)>;

                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (t.value.has_value() && t.value->id == old_value) {
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
