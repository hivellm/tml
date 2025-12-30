// Constant Propagation Optimization Pass Implementation

#include "mir/passes/constant_propagation.hpp"

namespace tml::mir {

auto ConstantPropagationPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    // Build the constant map
    build_constant_map(func);

    // Now propagate constants to all uses
    for (auto& block : func.blocks) {
        for (auto& inst : block.instructions) {
            bool inst_changed = std::visit(
                [this](auto& i) -> bool {
                    using T = std::decay_t<decltype(i)>;
                    bool modified = false;

                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        // Note: We don't actually replace the Value here because
                        // the Value struct contains the ID. Instead, constant folding
                        // will look up the constant. But we could mark for optimization.
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        // Same as above
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        // For select, if condition is constant, we could simplify
                        auto it = constants_.find(i.condition.id);
                        if (it != constants_.end()) {
                            if (auto* b = std::get_if<ConstBool>(&it->second)) {
                                // Mark that this select can be simplified
                                // The actual simplification would replace the select
                                // with just the true or false value
                                modified = true;
                            }
                        }
                    }
                    // Add more cases as needed

                    return modified;
                },
                inst.inst);

            changed |= inst_changed;
        }
    }

    return changed;
}

void ConstantPropagationPass::build_constant_map(const Function& func) {
    constants_.clear();

    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result == INVALID_VALUE) {
                continue;
            }

            // Check if this instruction produces a constant
            if (auto* ci = std::get_if<ConstantInst>(&inst.inst)) {
                constants_[inst.result] = ci->value;
            }
        }
    }
}

auto ConstantPropagationPass::try_propagate_to_value(Value& val) -> bool {
    auto it = constants_.find(val.id);
    if (it != constants_.end()) {
        // The value is a constant - in a more complete implementation,
        // we would replace the use with the constant
        return true;
    }
    return false;
}

} // namespace tml::mir
