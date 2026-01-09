//! # Code Sinking Pass
//!
//! Moves computations closer to their uses to reduce register pressure
//! and enable better instruction scheduling.
//!
//! ## Strategy
//!
//! If an instruction is only used in one successor block, move it there.
//! This is the inverse of LICM (hoisting).
//!
//! ## Example
//!
//! Before:
//! ```
//! bb0:
//!     %x = add %a, %b
//!     br_cond %c, bb1, bb2
//! bb1:
//!     use %x
//!     ...
//! bb2:
//!     ... (doesn't use %x)
//! ```
//!
//! After:
//! ```
//! bb0:
//!     br_cond %c, bb1, bb2
//! bb1:
//!     %x = add %a, %b
//!     use %x
//!     ...
//! bb2:
//!     ... (doesn't use %x)
//! ```
//!
//! ## Benefits
//!
//! - Reduces register pressure (value live for shorter time)
//! - Can enable further optimizations in the sunk-to block
//! - Improves instruction scheduling opportunities

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

class SinkingPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "Sinking";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Check if instruction can be sunk (no side effects, moveable)
    auto can_sink(const InstructionData& inst) -> bool;

    // Find the unique block where value is used, if any
    auto find_single_use_block(const Function& func, ValueId value,
                               uint32_t def_block) -> std::optional<uint32_t>;

    // Check if all operands are available in the target block
    auto operands_available_in(const Function& func, const InstructionData& inst,
                               uint32_t target_block, uint32_t source_block) -> bool;

    // Check if instruction's operands dominate the target block
    auto value_dominates_block(const Function& func, ValueId value,
                               uint32_t target_block) -> bool;

    // Get block index
    auto get_block_index(const Function& func, uint32_t id) -> int;
};

} // namespace tml::mir
