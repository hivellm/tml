//! # Global Value Numbering (GVN) Pass
//!
//! Eliminates redundant computations by assigning the same "value number"
//! to expressions that compute the same value, even across basic blocks.
//!
//! ## Algorithm (Hash-based GVN)
//!
//! 1. Process blocks in dominator tree order
//! 2. For each instruction, compute a hash of its operation and operands
//! 3. If the hash matches a previous instruction with same value number,
//!    replace uses with the previous result
//!
//! ## Benefits over CSE
//!
//! - Works across basic block boundaries
//! - Can discover more redundancies through value numbering
//! - Handles algebraic identities (x + 0 = x)

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::mir {

class GVNPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "GVN";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Value number for an expression
    using ValueNumber = uint32_t;
    static constexpr ValueNumber INVALID_VN = UINT32_MAX;

    // Expression representation for hashing
    struct Expression {
        std::string key;  // String representation for hashing

        bool operator==(const Expression& other) const {
            return key == other.key;
        }
    };

    struct ExpressionHash {
        auto operator()(const Expression& e) const -> size_t {
            return std::hash<std::string>{}(e.key);
        }
    };

    // Compute dominators (simplified - assumes structured control flow)
    auto compute_dominator_order(const Function& func) -> std::vector<size_t>;

    // Get value number for a ValueId
    auto get_value_number(ValueId id) -> ValueNumber;

    // Compute expression key for an instruction
    auto make_expression(const Instruction& inst) -> std::optional<Expression>;

    // Check if instruction is eligible for GVN
    auto can_gvn(const Instruction& inst) -> bool;

    // Replace all uses of old_value with new_value
    auto replace_uses(Function& func, ValueId old_value, ValueId new_value) -> void;

    // Value number table: ValueId -> ValueNumber
    std::unordered_map<ValueId, ValueNumber> value_numbers_;

    // Expression table: Expression -> (ValueNumber, defining ValueId)
    std::unordered_map<Expression, std::pair<ValueNumber, ValueId>, ExpressionHash> expr_table_;

    // Next value number to assign
    ValueNumber next_vn_ = 0;

    // Clear tables for a new function
    auto reset() -> void;
};

} // namespace tml::mir
