//! # Destructor Loop Hoisting Pass
//!
//! This MIR pass optimizes loop-local object allocations by hoisting them
//! outside the loop, reducing allocation overhead.
//!
//! ## Optimization Pattern
//!
//! Before:
//! ```tml
//! loop i in 0 to 1000 {
//!     let obj = MyClass::new()  // 1000 allocations
//!     process(obj)
//!     // destructor called 1000 times
//! }
//! ```
//!
//! After:
//! ```tml
//! let obj = MyClass::new()  // 1 allocation
//! loop i in 0 to 1000 {
//!     obj.reset()           // just reset state
//!     process(obj)
//! }
//! obj.drop()  // 1 destructor call
//! ```
//!
//! ## Analysis Requirements
//!
//! The pass verifies:
//! 1. Object is allocated inside loop
//! 2. Object doesn't escape the loop
//! 3. Class has a reset() method
//! 4. Object is dropped at end of each iteration

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"
#include "types/env.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Information about a loop-local allocation for destructor hoisting
struct DestructorLoopAllocation {
    ValueId alloc_value;    ///< The allocated pointer
    std::string class_name; ///< Class being allocated
    size_t alloc_block;     ///< Block containing allocation
    size_t alloc_inst_idx;  ///< Instruction index of allocation
    size_t drop_block;      ///< Block containing drop (if found)
    size_t drop_inst_idx;   ///< Instruction index of drop
    bool has_reset_method;  ///< Class has reset() method
    bool escapes_loop;      ///< Object escapes loop
    bool can_hoist;         ///< Safe to hoist
};

/// Statistics for destructor hoisting
struct DestructorHoistStats {
    size_t loops_analyzed = 0;
    size_t allocations_found = 0;
    size_t allocations_hoisted = 0;
    size_t drops_moved = 0;
};

/// Destructor loop hoisting optimization pass
class DestructorHoistPass : public FunctionPass {
public:
    explicit DestructorHoistPass(types::TypeEnv& env) : env_(env) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "DestructorHoist";
    }

    /// Get statistics from last run
    [[nodiscard]] auto stats() const -> const DestructorHoistStats& {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    types::TypeEnv& env_;
    DestructorHoistStats stats_;

    /// Detect all loops in the function
    auto detect_loops(const Function& func) -> std::vector<std::pair<size_t, std::vector<size_t>>>;

    /// Find allocations within a loop
    auto find_loop_allocations(const Function& func, const std::vector<size_t>& loop_blocks)
        -> std::vector<DestructorLoopAllocation>;

    /// Check if a value escapes the loop
    auto escapes_loop(const Function& func, ValueId value,
                      const std::unordered_set<size_t>& loop_block_set) -> bool;

    /// Check if class has reset() method
    auto has_reset_method(const std::string& class_name) -> bool;

    /// Find the drop call for an allocation within loop
    auto find_drop_in_loop(const Function& func, ValueId alloc,
                           const std::vector<size_t>& loop_blocks)
        -> std::optional<std::pair<size_t, size_t>>;

    /// Hoist allocation before loop
    auto hoist_allocation(Function& func, const DestructorLoopAllocation& alloc,
                          size_t preheader_block) -> bool;

    /// Replace allocation with reset() call in loop
    auto replace_with_reset(Function& func, const DestructorLoopAllocation& alloc) -> bool;

    /// Move drop after loop
    auto move_drop_after_loop(Function& func, const DestructorLoopAllocation& alloc,
                              size_t exit_block) -> bool;
};

} // namespace tml::mir
