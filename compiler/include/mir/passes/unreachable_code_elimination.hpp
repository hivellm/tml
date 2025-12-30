#pragma once

// Unreachable Code Elimination (UCE) Optimization Pass
//
// This pass removes basic blocks that cannot be reached from the entry block.
// A block is unreachable if:
// 1. It has no predecessors (except the entry block)
// 2. It's not reachable through any control flow path from entry
//
// This pass also:
// - Removes branches to unreachable blocks
// - Simplifies conditional branches where the condition is constant
// - Removes code after unconditional returns/unreachable terminators

#include "mir/mir_pass.hpp"

#include <unordered_set>

namespace tml::mir {

class UnreachableCodeEliminationPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "UnreachableCodeElimination";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Compute the set of reachable blocks from entry
    auto compute_reachable_blocks(const Function& func) -> std::unordered_set<uint32_t>;

    // Remove unreachable blocks from function
    auto remove_unreachable_blocks(Function& func, const std::unordered_set<uint32_t>& reachable)
        -> bool;

    // Simplify conditional branches with constant conditions
    auto simplify_constant_branches(Function& func) -> bool;

    // Remove instructions after unconditional control flow
    auto remove_dead_instructions_after_terminator(Function& func) -> bool;
};

} // namespace tml::mir
