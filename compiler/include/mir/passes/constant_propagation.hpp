//! # Constant Propagation Optimization Pass
//!
//! Replaces uses of variables known to be constant with the constant value
//! itself. This enables further optimizations like constant folding.
//!
//! ## Algorithm
//!
//! 1. Build a map of all values that are constants
//! 2. For each instruction, replace operands with known constants
//! 3. Run constant folding to simplify the new constant expressions
//!
//! ## Example
//!
//! ```mir
//! %x = const 5
//! %y = add %x, 3    ; %x replaced with const 5
//! ```
//!
//! After constant folding:
//!
//! ```mir
//! %x = const 5
//! %y = const 8      ; 5 + 3 folded
//! ```
//!
//! ## When to Run
//!
//! Run before constant folding. These passes work together in a loop
//! until reaching a fixed point.

#pragma once

#include "mir/mir_pass.hpp"

#include <unordered_map>

namespace tml::mir {

/// Constant propagation optimization pass.
///
/// Replaces uses of variables known to be constant with the constant
/// value itself, enabling further constant folding optimizations.
class ConstantPropagationPass : public FunctionPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "ConstantPropagation";
    }

protected:
    /// Runs constant propagation on a single function.
    auto run_on_function(Function& func) -> bool override;

private:
    /// Map from value ID to constant value (if known).
    std::unordered_map<ValueId, Constant> constants_;

    /// Builds the constant map by scanning all instructions.
    void build_constant_map(const Function& func);

    /// Attempts to replace a value reference with a constant.
    /// Returns true if the replacement was made.
    auto try_propagate_to_value(Value& val) -> bool;
};

} // namespace tml::mir
