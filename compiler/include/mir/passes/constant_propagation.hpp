#pragma once

// Constant Propagation Optimization Pass
//
// This pass replaces uses of variables that are known to be constant
// with the constant value itself. This enables further optimizations
// like constant folding.
//
// For example:
//   let x = 5
//   let y = x + 3   // x is replaced with 5
//
// Becomes:
//   let x = 5
//   let y = 5 + 3   // Now constant folding can compute 8

#include "mir/mir_pass.hpp"

#include <unordered_map>

namespace tml::mir {

class ConstantPropagationPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "ConstantPropagation";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Map from value ID to constant value (if known)
    std::unordered_map<ValueId, Constant> constants_;

    // Build the constant map for a function
    void build_constant_map(const Function& func);

    // Try to replace a value with a constant, returns true if replaced
    auto try_propagate_to_value(Value& val) -> bool;
};

} // namespace tml::mir
