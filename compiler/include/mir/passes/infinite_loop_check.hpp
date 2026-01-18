//! # Infinite Loop Detection Pass
//!
//! Static analysis pass to detect potential infinite loops:
//! - Loops without break/return statements
//! - Loops with constant true conditions
//! - Loops where loop variables are never modified
//!
//! This pass runs before optimization to catch issues early.

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace tml::mir {

/// Information about a detected infinite loop
struct InfiniteLoopWarning {
    std::string function_name;
    std::string block_name;
    uint32_t block_id;
    std::string reason;
};

/// Pass to detect potential infinite loops in MIR
class InfiniteLoopCheckPass : public MirPass {
public:
    InfiniteLoopCheckPass() = default;

    [[nodiscard]] auto name() const -> std::string override {
        return "infinite-loop-check";
    }

    auto run(Module& module) -> bool override;

    /// Get all warnings from the last run
    auto get_warnings() const -> const std::vector<InfiniteLoopWarning>& {
        return warnings_;
    }

    /// Check if any infinite loops were detected
    auto has_warnings() const -> bool {
        return !warnings_.empty();
    }

    /// Print all warnings to stderr
    void print_warnings() const;

private:
    std::vector<InfiniteLoopWarning> warnings_;

    /// Analyze a single function for infinite loops
    void analyze_function(const Function& func);

    /// Check if a block is a loop header (has back-edges)
    auto is_loop_header(const Function& func, const BasicBlock& block) -> bool;

    /// Check if there's a path from block to exit without going through loop header
    auto has_exit_path(const Function& func, uint32_t header_id,
                       const std::unordered_set<uint32_t>& loop_blocks) -> bool;

    /// Get all blocks that belong to a loop (dominated by header, can reach header)
    auto get_loop_blocks(const Function& func, uint32_t header_id) -> std::unordered_set<uint32_t>;

    /// Check if any block in the loop has a break (branch to outside loop)
    auto loop_has_exit(const Function& func, uint32_t header_id,
                       const std::unordered_set<uint32_t>& loop_blocks) -> bool;

    /// Check if loop condition is always true (constant true)
    auto is_condition_always_true(const Function& func, const BasicBlock& header) -> bool;

    /// Check if loop variables are modified in the loop body
    auto loop_modifies_condition_vars(const Function& func, const BasicBlock& header,
                                      const std::unordered_set<uint32_t>& loop_blocks) -> bool;

    /// Get successor block IDs from a terminator
    auto get_successors(const Terminator& term) -> std::vector<uint32_t>;

    /// Find block by ID
    auto find_block(const Function& func, uint32_t id) -> const BasicBlock*;
};

} // namespace tml::mir
