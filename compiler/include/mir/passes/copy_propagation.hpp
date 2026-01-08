//! # Copy Propagation Optimization Pass
//!
//! Replaces uses of copied values with the original value. A copy occurs
//! when a value is assigned to another without modification.
//!
//! ## Copy Types in SSA Form
//!
//! - **Single-entry phi**: `%2 = phi [%1, bb0]` - %2 copies %1
//! - **Constant select**: `%2 = select true, %1, %x` - %2 copies %1
//! - **Identity bitcast**: `%2 = bitcast %1 to T` where types match
//!
//! ## Example
//!
//! Before:
//! ```mir
//! %2 = phi [%1, bb0]      ; Single incoming - just a copy
//! %3 = add %2, 1
//! ```
//!
//! After:
//! ```mir
//! %3 = add %1, 1          ; %2 eliminated, uses %1 directly
//! ```
//!
//! ## Relationship with DCE
//!
//! Copy propagation marks copies as unused but doesn't remove them.
//! Run DCE afterward to clean up the dead copy instructions.
//!
//! ## When to Run
//!
//! Run after CFG simplification (which may create single-entry phis)
//! and before other optimizations that benefit from reduced indirection.

#pragma once

#include "mir/mir_pass.hpp"

#include <unordered_map>

namespace tml::mir {

/// Copy propagation optimization pass.
///
/// Replaces uses of copied values with the original, eliminating
/// unnecessary indirection in the SSA graph.
class CopyPropagationPass : public FunctionPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "CopyPropagation";
    }

protected:
    /// Runs copy propagation on a single function.
    auto run_on_function(Function& func) -> bool override;

private:
    /// Finds all copy instructions in the function.
    /// Returns a map from copied value ID to original value ID.
    auto find_copies(const Function& func) -> std::unordered_map<ValueId, ValueId>;

    /// Replaces all uses of copied values with their originals.
    auto propagate_copies(Function& func, const std::unordered_map<ValueId, ValueId>& copies)
        -> bool;

    /// Checks if an instruction is a simple copy.
    /// Returns the source value ID if it's a copy, nullopt otherwise.
    auto is_copy(const Instruction& inst) -> std::optional<ValueId>;
};

} // namespace tml::mir
