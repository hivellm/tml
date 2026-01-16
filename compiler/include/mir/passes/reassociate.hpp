//! # Reassociation Pass
//!
//! Reorders associative and commutative operations to expose optimization
//! opportunities and group constants together.
//!
//! ## Transformations
//!
//! | Original | Reassociated | Benefit |
//! |----------|--------------|---------|
//! | (a + 1) + 2 | a + (1 + 2) = a + 3 | Constant folding |
//! | (a + b) + a | (a + a) + b = 2*a + b | CSE opportunity |
//! | a * (b * c) | (a * b) * c | Left-to-right evaluation |
//!
//! ## Algorithm
//!
//! 1. Linearize chains of associative operations
//! 2. Sort operands (constants last, by rank)
//! 3. Rebuild expression tree
//!
//! ## Associative Operations
//!
//! - Add (integers)
//! - Mul (integers)
//! - BitAnd
//! - BitOr
//! - BitXor

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <vector>

namespace tml::mir {

class ReassociatePass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "Reassociate";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Rank of a value (lower = closer to leaves, higher = closer to root)
    std::unordered_map<ValueId, int> ranks_;

    // Compute ranks for all values in a function
    auto compute_ranks(Function& func) -> void;

    // Get rank of a value
    auto get_rank(ValueId id) -> int;

    // Check if an operation is associative and commutative
    auto is_associative(BinOp op) -> bool;

    // Try to reassociate a binary instruction
    auto try_reassociate(Function& func, BasicBlock& block, size_t inst_idx) -> bool;

    // Linearize a chain of associative operations into operands
    auto linearize(Function& func, ValueId root, BinOp op, std::vector<ValueId>& operands) -> void;

    // Rebuild a balanced tree from operands
    auto rebuild_tree(Function& func, BasicBlock& block, size_t insert_pos,
                      const std::vector<ValueId>& operands, BinOp op, MirTypePtr result_type)
        -> ValueId;

    // Get constant value if any
    auto get_const_int(const Function& func, ValueId id) -> std::optional<int64_t>;

    // Check if a value is a constant
    auto is_constant(const Function& func, ValueId id) -> bool;

    // Find instruction that defines a value
    auto find_def(const Function& func, ValueId id) -> const BinaryInst*;
};

} // namespace tml::mir
