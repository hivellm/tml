//! # Loop Invariant Code Motion (LICM) Pass
//!
//! Moves loop-invariant computations out of loops to reduce redundant work.
//!
//! ## What is Loop Invariant?
//!
//! An instruction is loop-invariant if its operands are:
//! - Constants
//! - Defined outside the loop
//! - Defined by other loop-invariant instructions
//!
//! ## Algorithm
//!
//! 1. Identify natural loops (back edges in CFG)
//! 2. For each loop, find the preheader block
//! 3. Mark loop-invariant instructions
//! 4. Move safe instructions to preheader
//!
//! ## Alias Analysis Integration
//!
//! With alias analysis, LICM can also hoist:
//! - Loads from addresses that are not modified in the loop
//! - Loads from loop-invariant addresses where no aliasing stores exist
//!
//! ## Example
//!
//! Before:
//! ```
//! loop:
//!   %inv = add i32 %a, %b    // a, b defined outside loop
//!   %i = phi i32 [0, entry], [%i_next, loop]
//!   %x = add i32 %i, %inv
//!   ...
//! ```
//!
//! After:
//! ```
//! preheader:
//!   %inv = add i32 %a, %b    // hoisted
//!   br loop
//! loop:
//!   %i = phi i32 [0, preheader], [%i_next, loop]
//!   %x = add i32 %i, %inv
//!   ...
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::mir {

// Forward declaration
class AliasAnalysisPass;

class LICMPass : public FunctionPass {
public:
    /// Construct without alias analysis (conservative mode)
    LICMPass() = default;

    /// Construct with alias analysis for load hoisting
    explicit LICMPass(AliasAnalysisPass* alias_analysis) : alias_analysis_(alias_analysis) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "LICM";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    AliasAnalysisPass* alias_analysis_ = nullptr;
    // Represents a natural loop
    struct Loop {
        uint32_t header_id;                       // Loop header block
        std::unordered_set<uint32_t> blocks;      // All blocks in the loop
        std::unordered_set<uint32_t> exit_blocks; // Blocks that exit the loop
        uint32_t preheader_id = UINT32_MAX;       // Preheader (if exists/created)
    };

    // Find all natural loops in the function
    auto find_loops(const Function& func) -> std::vector<Loop>;

    // Find back edges (target dominates source)
    auto find_back_edges(const Function& func) -> std::vector<std::pair<uint32_t, uint32_t>>;

    // Build loop body from back edge
    auto build_loop(const Function& func, uint32_t header, uint32_t latch) -> Loop;

    // Check if an instruction is loop-invariant
    auto is_loop_invariant(const Function& func, const InstructionData& inst, const Loop& loop,
                           const std::unordered_set<ValueId>& invariant_values) -> bool;

    // Check if it's safe to hoist an instruction
    auto can_hoist(const InstructionData& inst) -> bool;

    // Check if a load can be hoisted (no aliasing stores in loop)
    auto can_hoist_load(const Function& func, const LoadInst& load, ValueId load_ptr,
                        const Loop& loop) -> bool;

    // Check if any store in the loop may alias with the given pointer
    auto has_aliasing_store_in_loop(const Function& func, ValueId ptr, const Loop& loop) -> bool;

    // Hoist invariant instructions to preheader
    auto hoist_invariants(Function& func, Loop& loop) -> bool;

    // Get or create a preheader for the loop
    auto get_or_create_preheader(Function& func, Loop& loop) -> uint32_t;

    // Check if a value is defined in a loop
    auto is_defined_in_loop(const Function& func, ValueId value, const Loop& loop) -> bool;

    // Get block index by ID
    auto get_block_index(const Function& func, uint32_t block_id) -> int;
};

} // namespace tml::mir
