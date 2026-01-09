//! # Match Branch Simplification Pass
//!
//! Simplifies switch/match statements by:
//! - Collapsing identical branches
//! - Converting single-case switches to conditional branches
//! - Removing unreachable cases
//!
//! ## Example
//!
//! Before:
//! ```
//! switch %x, default=bb4 [
//!   0 -> bb1,
//!   1 -> bb2,
//!   2 -> bb2,  // same as case 1
//!   3 -> bb3
//! ]
//! ```
//!
//! After (cases 1 and 2 merged):
//! ```
//! switch %x, default=bb4 [
//!   0 -> bb1,
//!   1 -> bb2,
//!   2 -> bb2,  // kept but bb2 now handles both
//!   3 -> bb3
//! ]
//! ```
//!
//! Or if only one case differs from default:
//! ```
//! br.cond %x == 0, bb1, bb_default
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

class MatchSimplifyPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "MatchSimplify";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Simplify a switch terminator
    auto simplify_switch(Function& func, BasicBlock& block) -> bool;

    // Check if two blocks are effectively identical
    auto blocks_equivalent(const Function& func, uint32_t block1_id, uint32_t block2_id) -> bool;

    // Convert switch with single non-default case to conditional branch
    auto convert_to_conditional(Function& func, BasicBlock& block, SwitchTerm& switch_term) -> bool;

    // Remove cases that go to the default block
    auto remove_redundant_cases(SwitchTerm& switch_term) -> bool;

    // Fold constant discriminant
    auto fold_constant_switch(Function& func, BasicBlock& block, SwitchTerm& switch_term) -> bool;

    // Get constant value if the discriminant is a constant
    auto get_constant_discriminant(const Function& func, ValueId id) -> std::optional<int64_t>;
};

} // namespace tml::mir
