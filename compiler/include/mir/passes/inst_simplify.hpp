//! # Instruction Simplification Pass (Peephole Optimizations)
//!
//! This pass performs local peephole optimizations on individual instructions.
//! It recognizes common patterns and replaces them with simpler equivalents.
//!
//! ## Algebraic Simplifications
//!
//! | Pattern             | Simplification      |
//! |---------------------|---------------------|
//! | x + 0               | x                   |
//! | x - 0               | x                   |
//! | x * 0               | 0                   |
//! | x * 1               | x                   |
//! | x / 1               | x                   |
//! | x & 0               | 0                   |
//! | x & -1              | x                   |
//! | x | 0               | x                   |
//! | x ^ 0               | x                   |
//! | x ^ x               | 0                   |
//! | x - x               | 0                   |
//! | x & x               | x                   |
//! | x | x               | x                   |
//!
//! ## Comparison Simplifications
//!
//! | Pattern             | Simplification      |
//! |---------------------|---------------------|
//! | x == x              | true                |
//! | x != x              | false               |
//! | x < x               | false               |
//! | x <= x              | true                |
//! | x > x               | false               |
//! | x >= x              | true                |
//!
//! ## Boolean Simplifications
//!
//! | Pattern             | Simplification      |
//! |---------------------|---------------------|
//! | not (not x)         | x                   |
//! | x and true          | x                   |
//! | x and false         | false               |
//! | x or true           | true                |
//! | x or false          | x                   |
//!
//! ## When to Run
//!
//! Run early (before inlining) to clean up generated code, and again
//! after other passes that may expose new simplification opportunities.

#pragma once

#include "mir/mir_pass.hpp"

namespace tml::mir {

/// Instruction simplification (peephole optimization) pass.
///
/// Performs local optimizations on individual instructions by recognizing
/// common patterns and replacing them with simpler equivalents.
class InstSimplifyPass : public FunctionPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "InstSimplify";
    }

protected:
    /// Runs instruction simplification on a single function.
    auto run_on_function(Function& func) -> bool override;

private:
    /// Tries to simplify a binary instruction.
    /// Returns the simplified value ID, or INVALID_VALUE if no simplification.
    auto simplify_binary(const BinaryInst& inst, const Function& func) -> ValueId;

    /// Tries to simplify a unary instruction.
    /// Returns the simplified value ID, or INVALID_VALUE if no simplification.
    auto simplify_unary(const UnaryInst& inst, const Function& func) -> ValueId;

    /// Tries to simplify a select instruction.
    /// Returns the simplified value ID, or INVALID_VALUE if no simplification.
    auto simplify_select(const SelectInst& inst, const Function& func) -> ValueId;

    /// Gets the constant integer value of a value ID, if it's a constant.
    auto get_const_int(const Function& func, ValueId id) -> std::optional<int64_t>;

    /// Gets the constant boolean value of a value ID, if it's a constant.
    auto get_const_bool(const Function& func, ValueId id) -> std::optional<bool>;

    /// Checks if two values are the same (same ID or same constant).
    auto are_same_value(const Function& func, ValueId a, ValueId b) -> bool;

    /// Replaces all uses of old_value with new_value.
    void replace_uses(Function& func, ValueId old_value, ValueId new_value);
};

} // namespace tml::mir
