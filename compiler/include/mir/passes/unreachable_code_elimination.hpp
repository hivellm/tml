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
//! - **Unreachable propagation**: If a branch target is an unreachable block,
//!   redirect or simplify the branch (Rust: `UnreachablePropagation`)
//! - **Switch case pruning**: Remove switch cases targeting unreachable blocks
//!
//! ## Algorithm
//!
//! 1. Simplify branches with constant conditions
//! 2. Propagate unreachable status through branches
//! 3. BFS from entry block to find reachable blocks
//! 4. Remove blocks not marked as reachable
//! 5. Update predecessor lists and phi nodes
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
//! Unreachable propagation example:
//! ```mir
//! bb0:
//!     br_if %cond, bb1, bb2
//! bb1:
//!     unreachable            ; bb1 is trivially unreachable
//! bb2:
//!     return
//! ; Result: br_if simplified to br bb2
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

    /// Propagates unreachable status: if a branch target is an unreachable
    /// block, simplify the branch (redirect to the other target or mark
    /// the block itself as unreachable).
    auto propagate_unreachable(Function& func) -> bool;

    /// Checks if a block is trivially unreachable (empty instructions +
    /// UnreachableTerm, or only side-effect-free instructions + UnreachableTerm).
    auto is_unreachable_block(const BasicBlock& block) const -> bool;
};

} // namespace tml::mir
