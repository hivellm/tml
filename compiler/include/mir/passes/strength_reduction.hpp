//! # Strength Reduction Pass
//!
//! Replaces expensive operations with cheaper equivalents.
//!
//! ## Transformations
//!
//! | Original | Replacement | Condition |
//! |----------|-------------|-----------|
//! | x * 2^n  | x << n      | power of 2 |
//! | x / 2^n  | x >> n      | unsigned, power of 2 |
//! | x % 2^n  | x & (2^n-1) | unsigned, power of 2 |
//! | x * 2    | x + x       | sometimes faster |
//! | x * 3    | x + x + x   | or (x << 1) + x |
//!
//! ## Example
//!
//! Before:
//! ```
//! %r = mul i32 %x, 8
//! ```
//!
//! After:
//! ```
//! %r = shl i32 %x, 3
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

namespace tml::mir {

class StrengthReductionPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "StrengthReduction";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Check if a value is a power of 2
    auto is_power_of_two(int64_t n) -> bool;

    // Get log2 of a power of 2
    auto log2_of(int64_t n) -> int;

    // Get constant value for a ValueId
    auto get_const_int(const Function& func, ValueId id) -> std::optional<int64_t>;

    // Try to reduce a multiply instruction
    auto reduce_multiply(Function& func, BasicBlock& block, size_t inst_idx,
                         const BinaryInst& mul) -> bool;

    // Try to reduce a divide instruction
    auto reduce_divide(Function& func, BasicBlock& block, size_t inst_idx,
                       const BinaryInst& div) -> bool;

    // Try to reduce a modulo instruction
    auto reduce_modulo(Function& func, BasicBlock& block, size_t inst_idx,
                       const BinaryInst& mod) -> bool;
};

} // namespace tml::mir
