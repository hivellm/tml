//! # Control Flow Graph Simplification Pass
//!
//! This pass simplifies the control flow graph by:
//! - Merging blocks with single predecessor/successor
//! - Removing empty blocks (just unconditional jumps)
//! - Eliminating unreachable blocks
//! - Simplifying branches with constant conditions
//!
//! ## Transformations
//!
//! | Pattern                          | Transformation                    |
//! |----------------------------------|-----------------------------------|
//! | Block A → Block B (only edge)    | Merge A and B                     |
//! | Empty block with br              | Redirect predecessors             |
//! | br i1 true, T, F                 | br T                              |
//! | br i1 false, T, F                | br F                              |
//! | Unreachable block                | Remove                            |
//!
//! ## Example
//!
//! ```mir
//! ; Before
//! entry:
//!     br label %middle
//! middle:
//!     %x = add %a, %b
//!     br label %exit
//! exit:
//!     ret %x
//!
//! ; After (blocks merged)
//! entry:
//!     %x = add %a, %b
//!     ret %x
//! ```
//!
//! ## When to Run
//!
//! Run after inlining and other passes that create redundant blocks.
//! Rust runs SimplifyCfg multiple times throughout the pipeline.

#pragma once

#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Control flow graph simplification pass.
///
/// Simplifies the CFG by merging blocks, removing empty blocks,
/// and eliminating unreachable code. This reduces code size and
/// improves cache locality.
class SimplifyCfgPass : public FunctionPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "SimplifyCfg";
    }

protected:
    /// Runs CFG simplification on a single function.
    auto run_on_function(Function& func) -> bool override;

private:
    /// Merges blocks that have a single predecessor and single successor.
    /// Returns true if any blocks were merged.
    auto merge_blocks(Function& func) -> bool;

    /// Removes empty blocks that only contain an unconditional branch.
    /// Redirects all predecessors to the target.
    /// Returns true if any blocks were removed.
    auto remove_empty_blocks(Function& func) -> bool;

    /// Simplifies conditional branches with constant conditions.
    /// Returns true if any branches were simplified.
    auto simplify_constant_branches(Function& func) -> bool;

    /// Removes unreachable blocks (blocks with no predecessors except entry).
    /// Returns true if any blocks were removed.
    auto remove_unreachable_blocks(Function& func) -> bool;

    /// Builds a map from block ID to its predecessors.
    auto build_predecessor_map(const Function& func)
        -> std::unordered_map<uint32_t, std::vector<uint32_t>>;

    /// Builds a map from block ID to its successors.
    auto build_successor_map(const Function& func)
        -> std::unordered_map<uint32_t, std::vector<uint32_t>>;

    /// Gets the block index by ID, or -1 if not found.
    auto get_block_index(const Function& func, uint32_t block_id) -> int;

    /// Updates all references to old_block_id to point to new_block_id.
    void redirect_block_references(Function& func, uint32_t old_block_id, uint32_t new_block_id);
};

} // namespace tml::mir
