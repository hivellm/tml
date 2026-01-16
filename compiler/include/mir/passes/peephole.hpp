//! # Peephole Optimization Pass
//!
//! Performs local pattern-based transformations on instruction sequences.
//! Works on small windows of consecutive instructions to apply algebraic
//! simplifications and identity removals.
//!
//! ## Patterns Recognized
//!
//! - `x + 0` → `x`
//! - `x - 0` → `x`
//! - `x * 1` → `x`
//! - `x * 0` → `0`
//! - `x / 1` → `x`
//! - `x & 0` → `0`
//! - `x & -1` → `x`
//! - `x | 0` → `x`
//! - `x | -1` → `-1`
//! - `x ^ 0` → `x`
//! - `x ^ x` → `0`
//! - `x - x` → `0`
//! - `x << 0` → `x`
//! - `x >> 0` → `x`
//! - Double negation: `--x` → `x`
//! - Double not: `!!x` → `x`
//!
//! ## Example
//!
//! Before:
//! ```
//! %1 = mul %x, 1
//! %2 = add %1, 0
//! ```
//!
//! After:
//! ```
//! (both instructions removed, uses of %2 replaced with %x)
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

namespace tml::mir {

class PeepholePass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "Peephole";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Try to simplify a binary instruction
    auto simplify_binary(Function& func, BasicBlock& block, size_t idx, BinaryInst& bin) -> bool;

    // Try to simplify a unary instruction
    auto simplify_unary(Function& func, BasicBlock& block, size_t idx, UnaryInst& unary) -> bool;

    // Get constant value if instruction is a constant
    auto get_const_int(const Function& func, ValueId id) -> std::optional<int64_t>;

    // Replace all uses of old_value with new_value
    auto replace_uses(Function& func, ValueId old_value, ValueId new_value) -> void;

    // Create a constant instruction
    auto create_constant(Function& func, int64_t value, const MirTypePtr& type) -> ValueId;
};

} // namespace tml::mir
