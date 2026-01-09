//! # Narrowing Pass
//!
//! Replaces operations on wider types with narrower types when safe.
//! This can reduce register pressure and improve performance.
//!
//! ## Transformations
//!
//! | Original | Narrowed | Condition |
//! |----------|----------|-----------|
//! | i64 op | i32 op | Value fits in i32 |
//! | zext then op | op on narrow | If result fits |
//!
//! ## Example
//!
//! Before:
//! ```
//! %wide = zext i32 %x to i64
//! %result = add i64 %wide, 1
//! %narrow = trunc i64 %result to i32
//! ```
//!
//! After:
//! ```
//! %result = add i32 %x, 1
//! ```
//!
//! ## Safety
//!
//! Only narrows when:
//! - The value is known to fit in the narrower type
//! - The operation doesn't overflow in the narrower type
//! - All uses can accept the narrower type

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

class NarrowingPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "Narrowing";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Try to narrow a zext-op-trunc pattern
    auto try_narrow_zext_pattern(Function& func, BasicBlock& block, size_t inst_idx) -> bool;

    // Try to narrow a sext-op-trunc pattern
    auto try_narrow_sext_pattern(Function& func, BasicBlock& block, size_t inst_idx) -> bool;

    // Check if a value is only used by truncation instructions
    auto is_only_used_by_trunc(const Function& func, ValueId value,
                               int target_bits) -> bool;

    // Find the instruction that defines a value
    auto find_def_inst(const Function& func, ValueId id) -> const InstructionData*;

    // Get bit width of a type
    auto get_bit_width(const MirTypePtr& type) -> int;

    // Check if operation is safe to narrow
    auto is_safe_to_narrow(BinOp op) -> bool;

    // Replace uses of old_value with new_value
    auto replace_uses(Function& func, ValueId old_value, ValueId new_value) -> void;
};

} // namespace tml::mir
