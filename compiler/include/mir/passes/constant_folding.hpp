#pragma once

// Constant Folding Optimization Pass
//
// This pass evaluates constant expressions at compile time.
// For example, `2 + 3` is replaced with `5`.
//
// Optimizations performed:
// - Binary operations on constants (add, sub, mul, div, etc.)
// - Unary operations on constants (neg, not)
// - Comparison operations on constants
// - Boolean operations on constants

#include "mir/mir_pass.hpp"

namespace tml::mir {

class ConstantFoldingPass : public BlockPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "ConstantFolding";
    }

protected:
    auto run_on_block(BasicBlock& block, Function& func) -> bool override;

private:
    // Try to fold a binary operation, returns nullopt if not foldable
    auto try_fold_binary(BinOp op, const Constant& left, const Constant& right)
        -> std::optional<Constant>;

    // Try to fold a unary operation
    auto try_fold_unary(UnaryOp op, const Constant& operand) -> std::optional<Constant>;

    // Get constant value for a value ID if it's a constant
    auto get_constant_for_value(const Function& func, ValueId id) -> std::optional<Constant>;
};

} // namespace tml::mir
