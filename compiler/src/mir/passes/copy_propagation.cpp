TML_MODULE("compiler")

//! # Copy Propagation Pass
//!
//! This pass replaces uses of copied values with the original value.
//!
//! ## Copy Detection
//!
//! | Pattern                    | Copy From |
//! |----------------------------|-----------|
//! | Single-incoming phi        | That value|
//! | Phi with identical values  | That value|
//! | Select with same branches  | That value|
//!
//! ## Algorithm
//!
//! 1. Find all copies in the function
//! 2. Resolve transitive copies: if %2=%1 and %3=%2, then %3=%1
//! 3. Replace all uses of copies with originals
//! 4. Repeat until no changes (handles chains)
//!
//! ## Updated Locations
//!
//! Propagation updates:
//! - Instruction operands (binary, unary, load, store, etc.)
//! - Call arguments
//! - Terminator operands (return value, condition, discriminant)

#include "mir/passes/copy_propagation.hpp"

namespace tml::mir {

auto CopyPropagationPass::run_on_function(Function& func) -> bool {
    bool changed = false;
    bool any_changed = true;

    // Build value type map once (types don't change during propagation)
    auto value_types = build_value_types(func);

    // Iterate until no more changes (transitive propagation)
    while (any_changed) {
        any_changed = false;

        auto copies = find_copies(func);
        if (!copies.empty()) {
            any_changed = propagate_copies(func, copies, value_types);
            changed |= any_changed;
        }
    }

    return changed;
}

auto CopyPropagationPass::build_value_types(const Function& func)
    -> std::unordered_map<ValueId, MirTypePtr> {
    std::unordered_map<ValueId, MirTypePtr> value_types;

    // Add parameter types
    for (const auto& param : func.params) {
        value_types[param.value_id] = param.type;
    }

    // Add instruction result types
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result != INVALID_VALUE && inst.type) {
                value_types[inst.result] = inst.type;
            }
        }
    }

    return value_types;
}

auto CopyPropagationPass::find_copies(const Function& func)
    -> std::unordered_map<ValueId, ValueId> {
    std::unordered_map<ValueId, ValueId> copies;

    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == INVALID_VALUE) {
                continue;
            }

            auto original = is_copy(inst.inst);
            if (original.has_value()) {
                copies[inst.result] = *original;
            }
        }
    }

    // Resolve transitive copies: if %2 = %1 and %3 = %2, then %3 = %1
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto& [copy, original] : copies) {
            auto it = copies.find(original);
            if (it != copies.end() && it->second != original) {
                copies[copy] = it->second;
                changed = true;
            }
        }
    }

    return copies;
}

auto CopyPropagationPass::is_copy(const Instruction& inst) -> std::optional<ValueId> {
    return std::visit(
        [](const auto& i) -> std::optional<ValueId> {
            using T = std::decay_t<decltype(i)>;

            if constexpr (std::is_same_v<T, PhiInst>) {
                // NOTE: We intentionally do NOT treat single-incoming phis as copies.
                // While semantically they are copies, the original value may not
                // dominate the use site. For example:
                //   bb6: %13 = phi [...]
                //   bb8: br bb4
                //   bb4: %1 = phi [%13, bb8]  ; single incoming
                //        %3 = eq %1, %2      ; uses %1
                // If we replace %1 with %13, we get use-before-def because %13
                // is only defined in bb6, which may not dominate bb4.
                //
                // Only treat as copy when ALL incoming values are the same
                // (meaning the phi doesn't depend on control flow).
                if (i.incoming.size() >= 2) {
                    ValueId first = i.incoming[0].first.id;
                    bool all_same = true;
                    for (const auto& [val, _] : i.incoming) {
                        if (val.id != first) {
                            all_same = false;
                            break;
                        }
                    }
                    if (all_same) {
                        return first;
                    }
                }
            } else if constexpr (std::is_same_v<T, SelectInst>) {
                // If both branches have the same value, it's a copy
                if (i.true_val.id == i.false_val.id) {
                    return i.true_val.id;
                }
            } else if constexpr (std::is_same_v<T, CastInst>) {
                // Bitcast to same type is effectively a copy
                // (Type checking would be needed for proper detection)
                if (i.kind == CastKind::Bitcast) {
                    // For now, conservatively don't treat bitcast as copy
                    // since we don't have easy type comparison
                    return std::nullopt;
                }
            }

            return std::nullopt;
        },
        inst);
}

auto CopyPropagationPass::propagate_copies(
    Function& func, const std::unordered_map<ValueId, ValueId>& copies,
    const std::unordered_map<ValueId, MirTypePtr>& value_types) -> bool {
    bool changed = false;

    // Helper to replace a value if it's a known copy
    // Also updates the type to match the new value's type
    auto replace = [&copies, &value_types, &changed](Value& val) {
        auto it = copies.find(val.id);
        if (it != copies.end()) {
            val.id = it->second;
            // Update type to match the original value's type
            auto type_it = value_types.find(val.id);
            if (type_it != value_types.end()) {
                val.type = type_it->second;
            }
            changed = true;
        }
    };

    for (auto& block : func.blocks) {
        for (auto& inst : block.instructions) {
            std::visit(
                [&replace](auto& i) {
                    using T = std::decay_t<decltype(i)>;

                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        replace(i.left);
                        replace(i.right);
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        replace(i.operand);
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        replace(i.ptr);
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        replace(i.ptr);
                        replace(i.value);
                    } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                        replace(i.base);
                        for (auto& idx : i.indices) {
                            replace(idx);
                        }
                    } else if constexpr (std::is_same_v<T, ExtractValueInst>) {
                        replace(i.aggregate);
                    } else if constexpr (std::is_same_v<T, InsertValueInst>) {
                        replace(i.aggregate);
                        replace(i.value);
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (auto& arg : i.args) {
                            replace(arg);
                        }
                    } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                        replace(i.receiver);
                        for (auto& arg : i.args) {
                            replace(arg);
                        }
                    } else if constexpr (std::is_same_v<T, CastInst>) {
                        replace(i.operand);
                    } else if constexpr (std::is_same_v<T, PhiInst>) {
                        for (auto& [val, _] : i.incoming) {
                            replace(val);
                        }
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        replace(i.condition);
                        replace(i.true_val);
                        replace(i.false_val);
                    } else if constexpr (std::is_same_v<T, StructInitInst>) {
                        for (auto& field : i.fields) {
                            replace(field);
                        }
                    } else if constexpr (std::is_same_v<T, EnumInitInst>) {
                        for (auto& p : i.payload) {
                            replace(p);
                        }
                    } else if constexpr (std::is_same_v<T, TupleInitInst>) {
                        for (auto& elem : i.elements) {
                            replace(elem);
                        }
                    } else if constexpr (std::is_same_v<T, ArrayInitInst>) {
                        for (auto& elem : i.elements) {
                            replace(elem);
                        }
                    } else if constexpr (std::is_same_v<T, AwaitInst>) {
                        replace(i.poll_value);
                    } else if constexpr (std::is_same_v<T, ClosureInitInst>) {
                        for (auto& cap : i.captures) {
                            replace(cap.second);
                        }
                    }
                    // ConstantInst and AllocaInst have no value operands
                },
                inst.inst);
        }

        // Also update terminators
        if (block.terminator.has_value()) {
            std::visit(
                [&replace](auto& t) {
                    using T = std::decay_t<decltype(t)>;

                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (t.value.has_value()) {
                            replace(*t.value);
                        }
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        replace(t.condition);
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        replace(t.discriminant);
                    }
                },
                *block.terminator);
        }
    }

    return changed;
}

} // namespace tml::mir
