//! # Basic Block Merging Pass
//!
//! Merges basic blocks that can be combined into a single block.
//! A block B can be merged into its predecessor A if:
//! - A has exactly one successor (B)
//! - B has exactly one predecessor (A)
//! - A's terminator is an unconditional branch to B
//!
//! ## Example
//!
//! Before:
//! ```
//! bb0:
//!     %1 = add %x, 1
//!     br bb1
//! bb1:
//!     %2 = mul %1, 2
//!     ret %2
//! ```
//!
//! After:
//! ```
//! bb0:
//!     %1 = add %x, 1
//!     %2 = mul %1, 2
//!     ret %2
//! ```
//!
//! ## Benefits
//!
//! - Reduces control flow overhead
//! - Enables further optimizations across merged blocks
//! - Simplifies the CFG for analysis

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

namespace tml::mir {

class BlockMergePass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "BlockMerge";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Check if two blocks can be merged
    auto can_merge(const Function& func, const BasicBlock& pred, const BasicBlock& succ) -> bool;

    // Merge successor into predecessor
    auto merge_blocks(Function& func, size_t pred_idx, size_t succ_idx) -> void;

    // Update phi nodes when blocks are merged
    auto update_phi_nodes(Function& func, uint32_t old_block, uint32_t new_block) -> void;

    // Get block index by ID
    auto get_block_index(const Function& func, uint32_t id) -> int;
};

} // namespace tml::mir
