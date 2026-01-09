//! # Early Common Subexpression Elimination Pass
//!
//! Performs local CSE early in the pipeline before other optimizations.
//!
//! ## Key Features
//!
//! - Works within basic blocks only (local CSE)
//! - Hashes expressions for fast lookup
//! - Handles: binary ops, unary ops, casts, GEPs
//! - Does NOT handle: loads, stores, calls (side effects)

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>

namespace tml::mir {

class EarlyCSEPass : public FunctionPass {
  public:
    [[nodiscard]] auto name() const -> std::string override { return "EarlyCSE"; }

  protected:
    auto run_on_function(Function& func) -> bool override;

  private:
    // Expression key for hashing
    struct ExprKey {
        std::string op;
        std::vector<ValueId> operands;

        auto operator==(const ExprKey& other) const -> bool {
            return op == other.op && operands == other.operands;
        }
    };

    struct ExprKeyHash {
        auto operator()(const ExprKey& key) const -> size_t {
            size_t h = std::hash<std::string>{}(key.op);
            for (ValueId v : key.operands) {
                h ^= std::hash<ValueId>{}(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
            return h;
        }
    };

    auto process_block(BasicBlock& block) -> bool;
    auto get_expr_key(const Instruction& inst) -> std::optional<ExprKey>;
    auto replace_uses_in_block(BasicBlock& block, size_t start_idx, ValueId old_val,
                               ValueId new_val) -> void;
};

} // namespace tml::mir
