//! # Mem2Reg Pass (Memory to Register Promotion)
//!
//! Promotes stack allocations (alloca) to SSA registers when the alloca:
//! - Is only used by load/store instructions
//! - Has no address taken (not passed to functions, not used in GEP)
//! - Is in the entry block
//!
//! ## Algorithm
//!
//! For each promotable alloca:
//! 1. Find all loads and stores to the alloca
//! 2. If only one store and it dominates all loads, replace loads with stored value
//! 3. Otherwise, insert phi nodes at dominance frontiers and rename variables
//!
//! ## Example
//!
//! Before:
//! ```
//! entry:
//!   %x = alloca i32
//!   store i32 5, ptr %x
//!   %v1 = load i32, ptr %x
//!   %v2 = add i32 %v1, 10
//! ```
//!
//! After:
//! ```
//! entry:
//!   %v2 = add i32 5, 10
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::mir {

class Mem2RegPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "Mem2Reg";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Information about a promotable alloca
    struct AllocaInfo {
        ValueId alloca_id;
        MirTypePtr alloc_type;
        std::string name;
        size_t block_idx;
        size_t inst_idx;

        // Stores to this alloca: (block_idx, inst_idx, stored_value)
        std::vector<std::tuple<size_t, size_t, ValueId>> stores;

        // Loads from this alloca: (block_idx, inst_idx, load_result)
        std::vector<std::tuple<size_t, size_t, ValueId>> loads;

        // Blocks that define (store to) this alloca
        std::unordered_set<uint32_t> def_blocks;

        // Blocks that use (load from) this alloca
        std::unordered_set<uint32_t> use_blocks;
    };

    // Check if an alloca can be promoted to SSA
    auto is_promotable(const Function& func, ValueId alloca_id, AllocaInfo& info) -> bool;

    // Promote a single-store alloca (simple case)
    auto promote_single_store(Function& func, AllocaInfo& info) -> bool;

    // Promote alloca with multiple stores using phi nodes
    auto promote_with_phi(Function& func, AllocaInfo& info) -> bool;

    // Replace all uses of old_value with new_value
    auto replace_value(Function& func, ValueId old_value, ValueId new_value) -> void;

    // Remove an instruction by index
    auto remove_instruction(BasicBlock& block, size_t idx) -> void;

    // Collect all allocas and their uses
    auto collect_allocas(Function& func) -> std::vector<AllocaInfo>;
};

} // namespace tml::mir
