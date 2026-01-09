//! # Dead Function Elimination Pass
//!
//! Removes functions that are never called from the module.
//!
//! ## Algorithm
//!
//! 1. Build call graph from all functions
//! 2. Mark entry points as live (main, @test, @bench, etc.)
//! 3. Traverse call graph marking all reachable functions
//! 4. Remove unmarked functions
//!
//! ## Entry Points
//!
//! | Attribute    | Reason                        |
//! |--------------|-------------------------------|
//! | main         | Program entry point           |
//! | @test        | Test function                 |
//! | @bench       | Benchmark function            |
//! | @fuzz        | Fuzz target                   |
//! | @export      | Exported for FFI              |
//! | @inline      | May be called from unknown    |
//!
//! ## When to Run
//!
//! Run after inlining to eliminate functions that were fully inlined.

#pragma once

#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Statistics collected during dead function elimination.
struct DeadFunctionStats {
    size_t functions_analyzed = 0;    ///< Total functions examined.
    size_t functions_removed = 0;     ///< Functions eliminated.
    size_t functions_kept = 0;        ///< Functions retained (reachable).
    size_t entry_points = 0;          ///< Number of entry points found.
    size_t instructions_removed = 0;  ///< Total instructions removed.
};

/// Dead function elimination pass.
///
/// Removes unreachable functions from the module. A function is considered
/// reachable if it can be called from an entry point (main, tests, exports).
class DeadFunctionEliminationPass : public MirPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "DeadFunctionElimination";
    }

    /// Runs dead function elimination on the module.
    auto run(Module& module) -> bool override;

    /// Returns elimination statistics.
    [[nodiscard]] auto get_stats() const -> DeadFunctionStats {
        return stats_;
    }

private:
    DeadFunctionStats stats_;
    std::unordered_set<std::string> live_functions_;
    std::unordered_map<std::string, std::unordered_set<std::string>> call_graph_;

    /// Builds the call graph for the module.
    void build_call_graph(const Module& module);

    /// Marks entry point functions as live.
    void mark_entry_points(const Module& module);

    /// Recursively marks all functions reachable from live functions.
    void propagate_liveness();

    /// Counts instructions in a function.
    [[nodiscard]] auto count_instructions(const Function& func) const -> size_t;
};

} // namespace tml::mir
