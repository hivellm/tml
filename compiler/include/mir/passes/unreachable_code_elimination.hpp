//! # Unreachable Code Elimination (UCE) Optimization Pass
//!
//! Removes basic blocks that cannot be reached from the function entry.
//! A block is unreachable if there is no control flow path from the entry
//! block to it.
//!
//! ## Optimizations Performed
//!
//! - **Block removal**: Delete unreachable basic blocks
//! - **Branch simplification**: Convert conditional branches with constant
//!   conditions to unconditional branches
//! - **Dead code removal**: Remove instructions after `return` or `unreachable`
//!
//! ## Algorithm
//!
//! 1. Start from entry block, traverse CFG via DFS/BFS
//! 2. Mark all visited blocks as reachable
//! 3. Remove blocks not marked as reachable
//! 4. Simplify branches with constant conditions
//!
//! ## Example
//!
//! ```mir
//! bb0:
//!     br_if const_false, bb1, bb2
//! bb1:                      ; Unreachable after branch simplification
//!     ...
//! bb2:
//!     return
//! ```
//!
//! ## When to Run
//!
//! Run after constant propagation (to detect constant branch conditions)
//! and after inlining (which may create unreachable code).

#pragma once

#include "mir/mir_pass.hpp"

#include <unordered_set>

namespace tml::mir {

/// Unreachable code elimination optimization pass.
///
/// Removes basic blocks that cannot be reached from the function entry
/// and simplifies control flow where branch conditions are constant.
class UnreachableCodeEliminationPass : public FunctionPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "UnreachableCodeElimination";
    }

protected:
    /// Runs UCE on a single function.
    auto run_on_function(Function& func) -> bool override;

private:
    /// Computes the set of reachable block IDs via CFG traversal.
    auto compute_reachable_blocks(const Function& func) -> std::unordered_set<uint32_t>;

    /// Removes blocks not in the reachable set.
    auto remove_unreachable_blocks(Function& func, const std::unordered_set<uint32_t>& reachable)
        -> bool;

    /// Converts conditional branches with constant conditions to unconditional.
    auto simplify_constant_branches(Function& func) -> bool;

    /// Removes instructions after terminators (dead code).
    auto remove_dead_instructions_after_terminator(Function& func) -> bool;
};

} // namespace tml::mir
