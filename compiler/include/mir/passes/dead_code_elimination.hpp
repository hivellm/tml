#pragma once

// Dead Code Elimination (DCE) Optimization Pass
//
// This pass removes instructions whose results are never used.
// An instruction is dead if:
// 1. It produces a result that is never used
// 2. It has no side effects
//
// DCE is applied iteratively because removing one instruction
// may make other instructions dead.

#include "mir/mir_pass.hpp"

namespace tml::mir {

class DeadCodeEliminationPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "DeadCodeElimination";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Check if an instruction can be removed (no side effects)
    auto can_remove(const Instruction& inst) -> bool;

    // Check if a value is used anywhere in the function
    auto is_used(const Function& func, ValueId id) -> bool;
};

} // namespace tml::mir
