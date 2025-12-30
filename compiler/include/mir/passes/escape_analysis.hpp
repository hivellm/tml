#pragma once

// Escape Analysis Optimization Pass
//
// Determines whether allocated objects escape the current function.
// Objects that don't escape can be:
// - Stack allocated instead of heap allocated
// - Eligible for more aggressive optimizations
//
// The analysis tracks:
// - Heap allocations (alloc calls)
// - References and pointers
// - Function arguments and returns
// - Stores to global variables

#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/**
 * Escape state for a value
 */
enum class EscapeState {
    NoEscape,     // Value never escapes the current function
    ArgEscape,    // Value escapes via function argument
    ReturnEscape, // Value escapes via return
    GlobalEscape, // Value escapes to global state
    Unknown       // Cannot determine escape state
};

/**
 * Escape information for a value
 */
struct EscapeInfo {
    EscapeState state = EscapeState::Unknown;
    bool may_alias_heap = false;      // May alias heap-allocated memory
    bool may_alias_global = false;    // May alias global variables
    bool is_stack_promotable = false; // Can be promoted to stack allocation

    [[nodiscard]] auto escapes() const -> bool {
        return state != EscapeState::NoEscape;
    }
};

/**
 * Escape analysis pass
 *
 * Determines whether allocated objects escape the current function.
 * Objects that don't escape can be:
 * - Stack allocated instead of heap allocated
 * - Eligible for more aggressive optimizations
 */
class EscapeAnalysisPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "EscapeAnalysis";
    }

    // Query escape state for a value ID
    [[nodiscard]] auto get_escape_info(ValueId value) const -> EscapeInfo;

    // Check if a value can be stack promoted
    [[nodiscard]] auto can_stack_promote(ValueId value) const -> bool;

    // Get all stack-promotable allocation value IDs
    [[nodiscard]] auto get_stack_promotable() const -> std::vector<ValueId>;

    // Statistics
    struct Stats {
        size_t total_allocations = 0;
        size_t no_escape = 0;
        size_t arg_escape = 0;
        size_t return_escape = 0;
        size_t global_escape = 0;
        size_t stack_promotable = 0;
    };

    [[nodiscard]] auto get_stats() const -> Stats {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    std::unordered_map<ValueId, EscapeInfo> escape_info_;
    Stats stats_;

    // Analysis helpers
    void analyze_function(Function& func);
    void analyze_instruction(const InstructionData& inst, Function& func);
    void analyze_call(const CallInst& call, ValueId result_id);
    void analyze_store(const StoreInst& store);
    void analyze_return(const ReturnTerm& ret);

    // Mark a value as escaping
    void mark_escape(ValueId value, EscapeState state);

    // Propagate escape information through the function
    void propagate_escapes(Function& func);

    // Check if an instruction is an allocation
    [[nodiscard]] auto is_allocation(const Instruction& inst) const -> bool;
};

/**
 * Stack promotion pass
 *
 * Converts heap allocations that don't escape to stack allocations.
 * This runs after escape analysis and uses its results.
 */
class StackPromotionPass : public FunctionPass {
public:
    explicit StackPromotionPass(const EscapeAnalysisPass& escape_analysis)
        : escape_analysis_(escape_analysis) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "StackPromotion";
    }

    // Statistics
    struct Stats {
        size_t allocations_promoted = 0;
        size_t bytes_saved = 0;
    };

    [[nodiscard]] auto get_stats() const -> Stats {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    const EscapeAnalysisPass& escape_analysis_;
    Stats stats_;

    // Convert a heap allocation to stack allocation
    auto promote_allocation(BasicBlock& block, size_t inst_index, Function& func) -> bool;

    // Estimate the size of an allocation
    [[nodiscard]] auto estimate_allocation_size(const Instruction& inst) const -> size_t;
};

} // namespace tml::mir
