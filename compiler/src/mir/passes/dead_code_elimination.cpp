// Dead Code Elimination Optimization Pass Implementation

#include "mir/passes/dead_code_elimination.hpp"

#include <algorithm>

namespace tml::mir {

auto DeadCodeEliminationPass::run_on_function(Function& func) -> bool {
    bool changed = false;
    bool made_progress = true;

    // Keep iterating until no more progress is made
    while (made_progress) {
        made_progress = false;

        for (auto& block : func.blocks) {
            // Find instructions to remove
            std::vector<size_t> to_remove;

            for (size_t i = 0; i < block.instructions.size(); ++i) {
                const auto& inst = block.instructions[i];

                // Skip instructions with no result (like stores)
                if (inst.result == INVALID_VALUE) {
                    continue;
                }

                // Check if the result is used
                if (!is_used(func, inst.result)) {
                    // Check if the instruction can be removed
                    if (can_remove(inst.inst)) {
                        to_remove.push_back(i);
                        made_progress = true;
                        changed = true;
                    }
                }
            }

            // Remove dead instructions (in reverse order to maintain indices)
            for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it) {
                block.instructions.erase(block.instructions.begin() +
                                         static_cast<std::ptrdiff_t>(*it));
            }
        }
    }

    return changed;
}

auto DeadCodeEliminationPass::can_remove(const Instruction& inst) -> bool {
    return std::visit(
        [](const auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;

            // Instructions that CAN be removed (no side effects):
            // - Constants
            // - Binary/Unary operations
            // - Load (but only if result unused)
            // - Alloca (but careful with this one)
            // - GEP
            // - ExtractValue
            // - InsertValue
            // - Cast
            // - Phi
            // - Select
            // - Struct/Enum/Tuple/Array init

            // Instructions that CANNOT be removed:
            // - Store (writes to memory)
            // - Call (may have side effects)
            // - MethodCall (may have side effects)

            if constexpr (std::is_same_v<T, StoreInst>) {
                return false;
            } else if constexpr (std::is_same_v<T, CallInst>) {
                // Conservatively assume all calls have side effects
                // TODO: Could be refined with purity analysis
                return false;
            } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                return false;
            } else if constexpr (std::is_same_v<T, AllocaInst>) {
                // Alloca can be removed if the allocated memory is never used
                // For now, be conservative
                return true;
            } else {
                return true;
            }
        },
        inst);
}

auto DeadCodeEliminationPass::is_used(const Function& func, ValueId id) -> bool {
    // Use the utility function from mir_pass.hpp
    return is_value_used(func, id);
}

} // namespace tml::mir
