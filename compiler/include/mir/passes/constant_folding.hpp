//! # Constant Folding Optimization Pass
//!
//! This pass evaluates constant expressions at compile time, replacing
//! computations with their results when all operands are known constants.
//!
//! ## Optimizations Performed
//!
//! - Binary operations: `add`, `sub`, `mul`, `div`, `mod`, etc.
//! - Unary operations: `neg`, `not`
//! - Comparison operations: `eq`, `ne`, `lt`, `le`, `gt`, `ge`
//! - Boolean operations: `and`, `or`, `xor`
//!
//! ## Example
//!
//! ```mir
//! %1 = const 2
//! %2 = const 3
//! %3 = add %1, %2   ; Folded to: %3 = const 5
//! ```
//!
//! ## When to Run
//!
//! Run after constant propagation for maximum effect. Often run multiple
//! times as other passes may expose new folding opportunities.

#pragma once

#include "mir/mir_pass.hpp"

namespace tml::mir {

/// Constant folding optimization pass.
///
/// Evaluates constant expressions at compile time, replacing operations
/// on constants with their computed results. This is a block-level pass
/// that processes each basic block independently.
class ConstantFoldingPass : public BlockPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "ConstantFolding";
    }

protected:
    /// Runs constant folding on a single basic block.
    auto run_on_block(BasicBlock& block, Function& func) -> bool override;

private:
    /// Attempts to fold a binary operation on two constants.
    /// Returns the result if foldable, nullopt otherwise.
    auto try_fold_binary(BinOp op, const Constant& left, const Constant& right)
        -> std::optional<Constant>;

    /// Attempts to fold a unary operation on a constant.
    /// Returns the result if foldable, nullopt otherwise.
    auto try_fold_unary(UnaryOp op, const Constant& operand) -> std::optional<Constant>;

    /// Retrieves the constant value for a value ID if it's a constant instruction.
    auto get_constant_for_value(const Function& func, ValueId id) -> std::optional<Constant>;
};

} // namespace tml::mir
