#pragma once

// Common Subexpression Elimination (CSE) Optimization Pass
//
// This pass identifies and eliminates redundant computations.
// If the same expression is computed multiple times with the
// same operands, subsequent occurrences are replaced with
// references to the first result.
//
// Example:
//   %1 = add %a, %b
//   %2 = add %a, %b  ; same as %1
//   %3 = mul %1, %2
// Becomes:
//   %1 = add %a, %b
//   %3 = mul %1, %1  ; %2 replaced with %1
//
// Limitations:
// - Only eliminates within the same basic block (local CSE)
// - Does not handle instructions with side effects
// - Requires exact operand matching

#include "mir/mir_pass.hpp"

#include <functional>
#include <unordered_map>

namespace tml::mir {

// Hash for expressions to detect duplicates
struct ExprKey {
    // For binary operations: op + left_id + right_id
    // For unary operations: op + operand_id
    // For casts: kind + operand_id + target_type
    std::string key;

    bool operator==(const ExprKey& other) const {
        return key == other.key;
    }
};

struct ExprKeyHash {
    std::size_t operator()(const ExprKey& k) const {
        return std::hash<std::string>{}(k.key);
    }
};

class CommonSubexpressionEliminationPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "CommonSubexpressionElimination";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Create a key for an expression (for hashing/comparison)
    auto make_expr_key(const Instruction& inst) -> std::optional<ExprKey>;

    // Check if an instruction can be CSE'd
    auto can_cse(const Instruction& inst) -> bool;

    // Replace all uses of old_value with new_value in the function
    auto replace_uses(Function& func, ValueId old_value, ValueId new_value) -> void;
};

} // namespace tml::mir
