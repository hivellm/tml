//! # Loop Unrolling Pass
//!
//! Unrolls small loops with known trip counts to reduce loop overhead
//! and enable further optimizations.
//!
//! ## Strategy
//!
//! - Only unroll loops with compile-time known bounds
//! - Limit unrolling to small loops (configurable threshold)
//! - Full unroll for very small trip counts (< 8)
//! - Partial unroll (2x-4x) for larger loops
//!
//! ## Example
//!
//! Before:
//! ```
//! loop i in 0 to 4 {
//!     sum = sum + arr[i]
//! }
//! ```
//!
//! After (fully unrolled):
//! ```
//! sum = sum + arr[0]
//! sum = sum + arr[1]
//! sum = sum + arr[2]
//! sum = sum + arr[3]
//! ```
//!
//! ## Benefits
//!
//! - Eliminates loop overhead (branch, increment, compare)
//! - Enables constant propagation of loop index
//! - Allows instruction scheduling across iterations

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_set>

namespace tml::mir {

struct LoopUnrollOptions {
    // Maximum trip count for full unrolling
    int max_full_unroll_count = 8;

    // Maximum instructions per loop body for unrolling
    int max_loop_body_size = 20;

    // Unroll factor for partial unrolling
    int partial_unroll_factor = 4;

    // Maximum trip count to consider for any unrolling
    int max_trip_count = 64;
};

class LoopUnrollPass : public FunctionPass {
public:
    explicit LoopUnrollPass(LoopUnrollOptions opts = {}) : options_(opts) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "LoopUnroll";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    struct LoopInfo {
        uint32_t header_id;
        uint32_t latch_id;
        std::unordered_set<uint32_t> body_blocks;
        ValueId induction_var;
        int64_t start_value;
        int64_t end_value;
        int64_t step;
        bool is_increment;  // true if step > 0
    };

    // Find loops suitable for unrolling
    auto find_unrollable_loops(const Function& func) -> std::vector<LoopInfo>;

    // Analyze a loop to extract trip count info
    auto analyze_loop(const Function& func, uint32_t header, uint32_t latch)
        -> std::optional<LoopInfo>;

    // Check if loop body is small enough
    auto is_loop_body_small(const Function& func, const LoopInfo& loop) -> bool;

    // Fully unroll a loop
    auto fully_unroll(Function& func, const LoopInfo& loop) -> bool;

    // Helper to find back edges
    auto find_back_edges(const Function& func)
        -> std::vector<std::pair<uint32_t, uint32_t>>;

    // Get block by ID
    auto get_block(const Function& func, uint32_t id) -> const BasicBlock*;
    auto get_block_mut(Function& func, uint32_t id) -> BasicBlock*;

    // Get block index
    auto get_block_index(const Function& func, uint32_t id) -> int;

    LoopUnrollOptions options_;
};

} // namespace tml::mir
