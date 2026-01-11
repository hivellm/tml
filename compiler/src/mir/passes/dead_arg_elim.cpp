//! # Dead Argument Elimination Pass
//!
//! Removes unused function parameters from internal functions.

#include "mir/passes/dead_arg_elim.hpp"

namespace tml::mir {

auto DeadArgEliminationPass::run(Module& module) -> bool {
    bool changed = false;

    for (auto& func : module.functions) {
        // Skip external or exported functions
        if (!is_internal_function(module, func.name)) {
            continue;
        }

        // Skip main function
        if (func.name == "main" || func.name == "tml_main") {
            continue;
        }

        // Check each parameter for usage
        // Process from last to first to maintain indices
        for (size_t i = func.params.size(); i > 0; --i) {
            size_t param_idx = i - 1;

            if (!is_param_used(func, param_idx)) {
                // Find all call sites
                auto call_sites = find_call_sites(module, func.name);

                // Only eliminate if we found all call sites
                // (conservative - if we can't find all, don't optimize)
                if (!call_sites.empty()) {
                    eliminate_param(module, func, param_idx);
                    changed = true;
                }
            }
        }
    }

    return changed;
}

auto DeadArgEliminationPass::is_param_used(const Function& func, size_t param_idx) -> bool {
    if (param_idx >= func.params.size()) {
        return false;
    }

    ValueId param_value = func.params[param_idx].value_id;

    // Check all instructions for uses of this parameter
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            bool used = std::visit(
                [param_value](const auto& i) -> bool {
                    using T = std::decay_t<decltype(i)>;
                    (void)i;

                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        return i.left.id == param_value || i.right.id == param_value;
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        return i.operand.id == param_value;
                    } else if constexpr (std::is_same_v<T, CastInst>) {
                        return i.operand.id == param_value;
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        return i.ptr.id == param_value;
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        return i.ptr.id == param_value || i.value.id == param_value;
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (const auto& arg : i.args) {
                            if (arg.id == param_value)
                                return true;
                        }
                        return false;
                    } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                        if (i.receiver.id == param_value)
                            return true;
                        for (const auto& arg : i.args) {
                            if (arg.id == param_value)
                                return true;
                        }
                        return false;
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        return i.condition.id == param_value || i.true_val.id == param_value ||
                               i.false_val.id == param_value;
                    } else if constexpr (std::is_same_v<T, PhiInst>) {
                        for (const auto& [val, _] : i.incoming) {
                            if (val.id == param_value)
                                return true;
                        }
                        return false;
                    } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                        if (i.base.id == param_value)
                            return true;
                        for (const auto& idx : i.indices) {
                            if (idx.id == param_value)
                                return true;
                        }
                        return false;
                    } else {
                        return false;
                    }
                },
                inst.inst);

            if (used) {
                return true;
            }
        }

        // Check terminator
        if (block.terminator) {
            bool used = std::visit(
                [param_value](const auto& t) -> bool {
                    using T = std::decay_t<decltype(t)>;

                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        return t.value && t.value->id == param_value;
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        return t.condition.id == param_value;
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        return t.discriminant.id == param_value;
                    } else {
                        return false;
                    }
                },
                *block.terminator);

            if (used) {
                return true;
            }
        }
    }

    return false;
}

auto DeadArgEliminationPass::is_internal_function(const Module& module, const std::string& name)
    -> bool {
    // For now, consider all non-main functions as internal
    // A more sophisticated check would look at linkage/visibility attributes
    (void)module;
    return name != "main" && name != "tml_main" && name.find("extern") == std::string::npos;
}

auto DeadArgEliminationPass::find_call_sites(Module& module, const std::string& func_name)
    -> std::vector<std::pair<Function*, size_t>> {
    std::vector<std::pair<Function*, size_t>> sites;

    for (auto& func : module.functions) {
        for (auto& block : func.blocks) {
            for (size_t i = 0; i < block.instructions.size(); ++i) {
                if (auto* call = std::get_if<CallInst>(&block.instructions[i].inst)) {
                    if (call->func_name == func_name) {
                        sites.emplace_back(&func, i);
                    }
                }
            }
        }
    }

    return sites;
}

auto DeadArgEliminationPass::eliminate_param(Module& module, Function& func, size_t param_idx)
    -> void {
    // Remove parameter from function
    func.params.erase(func.params.begin() + static_cast<std::ptrdiff_t>(param_idx));

    // Update all call sites
    for (auto& caller : module.functions) {
        for (auto& block : caller.blocks) {
            for (auto& inst : block.instructions) {
                if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                    if (call->func_name == func.name && param_idx < call->args.size()) {
                        call->args.erase(call->args.begin() +
                                         static_cast<std::ptrdiff_t>(param_idx));
                    }
                }
            }
        }
    }
}

} // namespace tml::mir
