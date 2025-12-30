#pragma once

// Copy Propagation Optimization Pass
//
// This pass replaces uses of copied values with the original value.
// A copy is when a value is simply assigned to another without modification.
//
// In MIR/SSA form, this mainly applies to:
// 1. Phi nodes with single incoming value
// 2. Select instructions with constant condition
// 3. Bitcast to same type
//
// Example:
//   %2 = phi [%1, bb0]     ; single incoming, %2 is just a copy of %1
//   %3 = add %2, 1
// Becomes:
//   %3 = add %1, 1         ; %2 eliminated
//
// This pass works together with DCE to clean up the copies.

#include "mir/mir_pass.hpp"

#include <unordered_map>

namespace tml::mir {

class CopyPropagationPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "CopyPropagation";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Find all copies in the function
    // Returns a map from copied value to original value
    auto find_copies(const Function& func) -> std::unordered_map<ValueId, ValueId>;

    // Replace all uses of copies with originals
    auto propagate_copies(Function& func, const std::unordered_map<ValueId, ValueId>& copies)
        -> bool;

    // Check if an instruction is a simple copy
    auto is_copy(const Instruction& inst) -> std::optional<ValueId>;
};

} // namespace tml::mir
