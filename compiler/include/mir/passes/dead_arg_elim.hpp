//! # Dead Argument Elimination Pass
//!
//! Removes unused function parameters from internal functions.
//! If a parameter is never used within a function, it can be eliminated
//! from the function signature and all call sites updated.
//!
//! ## Conditions
//!
//! - Only applies to internal (non-exported) functions
//! - Parameter must have no uses within the function
//! - All call sites must be known and modifiable
//!
//! ## Example
//!
//! Before:
//! ```
//! func foo(x: I32, unused: I32, y: I32) -> I32 {
//!     return x + y
//! }
//!
//! call foo(1, 2, 3)  // unused=2 is never used
//! ```
//!
//! After:
//! ```
//! func foo(x: I32, y: I32) -> I32 {
//!     return x + y
//! }
//!
//! call foo(1, 3)
//! ```
//!
//! ## Benefits
//!
//! - Reduces argument passing overhead
//! - Enables further optimizations at call sites
//! - Reduces register pressure

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_set>

namespace tml::mir {

class DeadArgEliminationPass : public MirPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "DeadArgElim";
    }

    auto run(Module& module) -> bool override;

private:
    // Check if a parameter is used within a function
    auto is_param_used(const Function& func, size_t param_idx) -> bool;

    // Check if function is internal (can be modified)
    auto is_internal_function(const Module& module, const std::string& name) -> bool;

    // Find all call sites for a function
    auto find_call_sites(Module& module, const std::string& func_name)
        -> std::vector<std::pair<Function*, size_t>>;

    // Remove parameter from function and update call sites
    auto eliminate_param(Module& module, Function& func, size_t param_idx) -> void;
};

} // namespace tml::mir
