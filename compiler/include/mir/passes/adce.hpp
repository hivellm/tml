//! # Aggressive Dead Code Elimination (ADCE) Pass
//!
//! More aggressive version of DCE that removes code not contributing
//! to program output, even if the code has no apparent side effects.
//!
//! ## Strategy
//!
//! Uses reverse dataflow analysis:
//! 1. Mark all instructions that have observable side effects as "live"
//! 2. Mark instructions whose results are used by live instructions as "live"
//! 3. Remove all non-live instructions
//!
//! ## Observable Side Effects
//!
//! - Stores to memory
//! - Function calls (conservatively)
//! - Returns
//! - I/O operations
//!
//! ## Example
//!
//! Before:
//! ```
//! %1 = load %ptr
//! %2 = add %1, 5      // Dead - result not used
//! %3 = mul %1, 2
//! store %3, %out
//! ```
//!
//! After:
//! ```
//! %1 = load %ptr
//! %3 = mul %1, 2
//! store %3, %out
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_set>

namespace tml::mir {

class ADCEPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "ADCE";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Mark an instruction as live and propagate to its operands
    auto mark_live(const Function& func, ValueId value,
                   std::unordered_set<ValueId>& live_values) -> void;

    // Check if instruction has observable side effects
    auto has_side_effects(const InstructionData& inst) -> bool;

    // Get operand values from instruction
    auto get_operands(const InstructionData& inst) -> std::vector<ValueId>;

    // Find instruction that defines a value
    auto find_def(const Function& func, ValueId value) -> const InstructionData*;
};

} // namespace tml::mir
