//! # SimplifySelect Pass
//!
//! Simplifies select (conditional) instructions.
//!
//! ## Optimizations
//!
//! - `select(true, a, b)` → `a`
//! - `select(false, a, b)` → `b`
//! - `select(c, a, a)` → `a` (same value)
//! - `select(c, true, false)` → `c`
//! - `select(c, false, true)` → `not c`
//! - `select(not c, a, b)` → `select(c, b, a)`

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

namespace tml::mir {

class SimplifySelectPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "SimplifySelect";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    auto simplify_select(Function& func, BasicBlock& block, size_t idx, SelectInst& sel) -> bool;
    auto get_const_bool(const Function& func, ValueId id) -> std::optional<bool>;
    auto is_not_instruction(const Function& func, ValueId id) -> std::optional<ValueId>;
    auto replace_uses(Function& func, ValueId old_val, ValueId new_val) -> void;
};

} // namespace tml::mir
