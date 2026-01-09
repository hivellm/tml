//! # Jump Threading Optimization Pass
//!
//! This pass eliminates redundant conditional branches by "threading" jumps
//! through intermediate blocks when the branch condition is known.
//!
//! ## Algorithm
//!
//! When a conditional branch targets a block that also has a conditional
//! branch on a related condition, we can sometimes skip the intermediate block.
//!
//! ## Example
//!
//! ```mir
//! ; Before
//! block_a:
//!     br i1 %cond, label %block_b, label %block_c
//! block_b:
//!     br i1 %cond, label %block_d, label %block_e  ; %cond is known true here
//!
//! ; After
//! block_a:
//!     br i1 %cond, label %block_d, label %block_c  ; Skip block_b's test
//! ```
//!
//! ## Transformations
//!
//! | Pattern                              | Transformation                |
//! |--------------------------------------|-------------------------------|
//! | br(cond) → br(cond) (same cond)      | Thread to final target        |
//! | br(cond) → br(not cond)              | Thread to other branch        |
//!
//! ## When to Run
//!
//! Run after SimplifyCfg and before final DCE.

#pragma once

#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Jump threading optimization pass.
///
/// Threads jumps through blocks when the branch condition can be determined
/// from the incoming edge.
class JumpThreadingPass : public FunctionPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "JumpThreading";
    }

protected:
    /// Runs jump threading on a single function.
    auto run_on_function(Function& func) -> bool override;

private:
    /// Tries to thread a conditional branch through target blocks.
    /// Returns true if any threading was performed.
    auto thread_branch(Function& func, BasicBlock& block) -> bool;

    /// Gets the condition value of a block's terminator, if it's a conditional branch.
    auto get_branch_condition(const BasicBlock& block) -> std::optional<ValueId>;

    /// Gets the block index by ID.
    auto get_block_index(const Function& func, uint32_t block_id) -> int;
};

} // namespace tml::mir
