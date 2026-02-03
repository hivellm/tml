//! # Memory Leak Detection Pass
//!
//! Static analysis pass to detect potential memory leaks:
//! - Heap allocations without corresponding free/destroy calls
//! - Allocations that escape function scope without being returned
//! - Stored references that are overwritten without being freed first
//!
//! This pass runs before optimization to catch issues early.

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"
#include "mir/passes/escape_analysis.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::mir {

/// Information about a detected memory leak
struct MemoryLeakWarning {
    std::string function_name;
    std::string allocation_site; // Block or instruction description
    uint32_t block_id;
    ValueId alloc_value;
    std::string reason;
    bool is_error; // true = error (must fix), false = warning (potential issue)
};

/// Pass to detect potential memory leaks in MIR
class MemoryLeakCheckPass : public MirPass {
public:
    MemoryLeakCheckPass() = default;

    [[nodiscard]] auto name() const -> std::string override {
        return "memory-leak-check";
    }

    auto run(Module& module) -> bool override;

    /// Get all warnings/errors from the last run
    auto get_warnings() const -> const std::vector<MemoryLeakWarning>& {
        return warnings_;
    }

    /// Check if any memory leaks were detected
    auto has_warnings() const -> bool {
        return !warnings_.empty();
    }

    /// Check if any errors (not just warnings) were detected
    auto has_errors() const -> bool;

    /// Print all warnings to stderr
    void print_warnings() const;

private:
    std::vector<MemoryLeakWarning> warnings_;
    const Module* module_ = nullptr;

    /// Analyze a single function for memory leaks
    void analyze_function(const Function& func);

    /// Track all allocations in a function
    auto find_allocations(const Function& func) -> std::unordered_map<ValueId, uint32_t>;

    /// Track all free/destroy calls in a function
    auto find_frees(const Function& func) -> std::unordered_set<ValueId>;

    /// Check if a value is returned from the function
    auto is_returned(const Function& func, ValueId value) -> bool;

    /// Check if a value is stored to a field (ownership transfer)
    auto is_stored_to_field(const Function& func, ValueId value) -> bool;

    /// Check if a value escapes via function argument (ownership transfer)
    auto escapes_via_arg(const Function& func, ValueId value) -> bool;

    /// Get block name for error reporting
    auto get_block_name(const Function& func, uint32_t block_id) -> std::string;

    /// Check if this is a heap allocation call
    auto is_heap_allocation(const Instruction& inst) const -> bool;

    /// Check if this is a free/destroy call
    auto is_free_call(const Instruction& inst, ValueId& freed_value) const -> bool;

    /// Check if this is an arena allocation (managed by arena, no manual free needed)
    auto is_arena_allocation(const Instruction& inst) const -> bool;
};

} // namespace tml::mir
