//! # Common Subexpression Elimination (CSE) Optimization Pass
//!
//! Identifies and eliminates redundant computations. When the same
//! expression is computed multiple times with identical operands,
//! subsequent occurrences are replaced with references to the first result.
//!
//! ## Example
//!
//! Before:
//! ```mir
//! %1 = add %a, %b
//! %2 = add %a, %b    ; Redundant - same as %1
//! %3 = mul %1, %2
//! ```
//!
//! After:
//! ```mir
//! %1 = add %a, %b
//! %3 = mul %1, %1    ; %2 replaced with %1
//! ```
//!
//! ## Limitations
//!
//! - **Local CSE only**: Eliminates within basic blocks, not across them
//! - **No side effects**: Instructions with side effects are not candidates
//! - **Exact matching**: Operands must match exactly (no commutativity)
//!
//! ## When to Run
//!
//! Run after inlining and loop unrolling, which often create duplicate
//! expressions. Follow with DCE to remove the now-unused instructions.

#pragma once

#include "mir/mir_pass.hpp"

#include <functional>
#include <unordered_map>

namespace tml::mir {

/// Key for identifying equivalent expressions.
///
/// Encodes the operation type and operand IDs into a hashable string
/// for duplicate detection.
struct ExprKey {
    /// Encoded key string (op + operand IDs).
    std::string key;

    /// Equality comparison for hash map.
    bool operator==(const ExprKey& other) const {
        return key == other.key;
    }
};

/// Hash function for ExprKey.
struct ExprKeyHash {
    std::size_t operator()(const ExprKey& k) const {
        return std::hash<std::string>{}(k.key);
    }
};

/// Common subexpression elimination optimization pass.
///
/// Identifies redundant computations within basic blocks and replaces
/// them with references to previously computed results.
class CommonSubexpressionEliminationPass : public FunctionPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "CommonSubexpressionElimination";
    }

protected:
    /// Runs CSE on a single function.
    auto run_on_function(Function& func) -> bool override;

private:
    /// Creates a hashable key for an expression.
    /// Returns nullopt if the instruction cannot be CSE'd.
    auto make_expr_key(const Instruction& inst) -> std::optional<ExprKey>;

    /// Checks if an instruction is a candidate for CSE.
    auto can_cse(const Instruction& inst) -> bool;

    /// Replaces all uses of old_value with new_value throughout the function.
    auto replace_uses(Function& func, ValueId old_value, ValueId new_value) -> void;
};

} // namespace tml::mir
