//! # Dead Code Elimination (DCE) Optimization Pass
//!
//! Removes instructions whose results are never used. An instruction is
//! considered "dead" if it produces a result that is never referenced and
//! has no observable side effects.
//!
//! ## Algorithm
//!
//! 1. Mark all instructions as potentially dead
//! 2. Walk uses backwards, marking used instructions as live
//! 3. Remove instructions that remain marked dead
//! 4. Repeat until no changes (removing one may make others dead)
//!
//! ## Side Effect Handling
//!
//! Instructions with side effects are never removed, including:
//! - Stores to memory
//! - Function calls (may have external effects)
//! - I/O operations
//!
//! ## Example
//!
//! ```mir
//! %1 = add %a, %b    ; Dead - result never used
//! %2 = mul %c, %d    ; Live - used in return
//! return %2
//! ```
//!
//! ## When to Run
//!
//! Run after other optimizations that may create dead code (inlining,
//! constant propagation, etc.).

#pragma once

#include "mir/mir_pass.hpp"

namespace tml::mir {

/// Dead code elimination optimization pass.
///
/// Removes instructions whose results are never used and have no side
/// effects. Applied iteratively since removing one instruction may
/// make others dead.
class DeadCodeEliminationPass : public FunctionPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "DeadCodeElimination";
    }

protected:
    /// Runs DCE on a single function, returning true if any changes were made.
    auto run_on_function(Function& func) -> bool override;

private:
    /// Checks if an instruction can be safely removed (has no side effects).
    auto can_remove(const Instruction& inst) -> bool;

    /// Checks if a value is used anywhere in the function.
    auto is_used(const Function& func, ValueId id) -> bool;
};

} // namespace tml::mir
