//! # Escape Analysis Optimization Pass
//!
//! Determines whether allocated objects escape the current function scope.
//! Objects that don't escape can be stack-allocated instead of heap-allocated,
//! avoiding allocation overhead and enabling further optimizations.
//!
//! ## Escape Categories
//!
//! - **NoEscape**: Never leaves the function - candidate for stack promotion
//! - **ArgEscape**: Passed to a called function
//! - **ReturnEscape**: Returned from the function
//! - **GlobalEscape**: Stored in a global variable
//!
//! ## Analysis Tracks
//!
//! - Heap allocations (`alloc` calls)
//! - Reference/pointer creation and propagation
//! - Function arguments and return values
//! - Stores to global variables and escaped locations
//!
//! ## Stack Promotion
//!
//! After analysis, the `StackPromotionPass` converts non-escaping heap
//! allocations to stack allocations (`alloca`), eliminating heap overhead.
//!
//! ## Example
//!
//! ```mir
//! %1 = call alloc(16)    ; Heap allocation
//! store 42, %1           ; Only local use
//! %2 = load %1
//! return %2              ; Value returned, not pointer - NoEscape!
//! ```

#pragma once

#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Escape state categories for a value.
enum class EscapeState {
    NoEscape,     ///< Value never escapes the current function.
    ArgEscape,    ///< Value escapes via function argument.
    ReturnEscape, ///< Value escapes via return statement.
    GlobalEscape, ///< Value escapes to global state.
    Unknown       ///< Cannot determine escape state.
};

/// Escape information for a single value.
struct EscapeInfo {
    EscapeState state = EscapeState::Unknown; ///< Escape state.
    bool may_alias_heap = false;              ///< May alias heap-allocated memory.
    bool may_alias_global = false;            ///< May alias global variables.
    bool is_stack_promotable = false;         ///< Can be promoted to stack allocation.
    bool is_class_instance = false;           ///< Is this a class instance allocation?
    std::string class_name;                   ///< Class name if is_class_instance is true.

    /// Returns true if the value escapes the function.
    [[nodiscard]] auto escapes() const -> bool {
        return state != EscapeState::NoEscape;
    }
};

/// Escape analysis pass.
///
/// Analyzes heap allocations to determine which can be safely
/// converted to stack allocations. Results are queried by the
/// `StackPromotionPass`.
class EscapeAnalysisPass : public FunctionPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "EscapeAnalysis";
    }

    /// Queries escape information for a specific value.
    [[nodiscard]] auto get_escape_info(ValueId value) const -> EscapeInfo;

    /// Checks if a value can be safely promoted to stack allocation.
    [[nodiscard]] auto can_stack_promote(ValueId value) const -> bool;

    /// Returns all value IDs that are candidates for stack promotion.
    [[nodiscard]] auto get_stack_promotable() const -> std::vector<ValueId>;

    /// Statistics from the escape analysis.
    struct Stats {
        size_t total_allocations = 0; ///< Total allocations analyzed.
        size_t no_escape = 0;         ///< Allocations that don't escape.
        size_t arg_escape = 0;        ///< Allocations escaping via arguments.
        size_t return_escape = 0;     ///< Allocations escaping via return.
        size_t global_escape = 0;     ///< Allocations escaping to globals.
        size_t stack_promotable = 0;  ///< Allocations eligible for stack promotion.

        // Class instance statistics
        size_t class_instances = 0;          ///< Total class instance allocations.
        size_t class_instances_no_escape = 0; ///< Class instances that don't escape.
        size_t class_instances_promotable = 0; ///< Class instances eligible for stack.
        size_t method_call_escapes = 0;       ///< Escapes via method calls.
        size_t field_store_escapes = 0;       ///< Escapes via field stores.
    };

    /// Returns analysis statistics.
    [[nodiscard]] auto get_stats() const -> Stats {
        return stats_;
    }

protected:
    /// Runs escape analysis on a single function.
    auto run_on_function(Function& func) -> bool override;

private:
    std::unordered_map<ValueId, EscapeInfo> escape_info_;
    Stats stats_;

    void analyze_function(Function& func);
    void analyze_instruction(const InstructionData& inst, Function& func);
    void analyze_call(const CallInst& call, ValueId result_id);
    void analyze_method_call(const MethodCallInst& call, ValueId result_id);
    void analyze_store(const StoreInst& store);
    void analyze_gep(const GetElementPtrInst& gep, ValueId result_id);
    void analyze_return(const ReturnTerm& ret);
    void mark_escape(ValueId value, EscapeState state);
    void propagate_escapes(Function& func);
    [[nodiscard]] auto is_allocation(const Instruction& inst) const -> bool;

    // Class instance tracking helpers
    [[nodiscard]] auto is_constructor_call(const std::string& func_name) const -> bool;
    [[nodiscard]] auto extract_class_name(const std::string& func_name) const -> std::string;
    void track_this_parameter(const Function& func);
};

/// Stack promotion pass.
///
/// Converts heap allocations that don't escape to stack allocations.
/// Runs after `EscapeAnalysisPass` and uses its results.
class StackPromotionPass : public FunctionPass {
public:
    /// Creates a stack promotion pass using results from escape analysis.
    explicit StackPromotionPass(const EscapeAnalysisPass& escape_analysis)
        : escape_analysis_(escape_analysis) {}

    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "StackPromotion";
    }

    /// Statistics from stack promotion.
    struct Stats {
        size_t allocations_promoted = 0; ///< Number of allocations converted.
        size_t bytes_saved = 0;          ///< Estimated bytes saved from heap.
    };

    /// Returns promotion statistics.
    [[nodiscard]] auto get_stats() const -> Stats {
        return stats_;
    }

protected:
    /// Runs stack promotion on a single function.
    auto run_on_function(Function& func) -> bool override;

private:
    const EscapeAnalysisPass& escape_analysis_;
    Stats stats_;

    /// Converts a heap allocation to a stack allocation.
    auto promote_allocation(BasicBlock& block, size_t inst_index, Function& func) -> bool;

    /// Estimates the byte size of an allocation.
    [[nodiscard]] auto estimate_allocation_size(const Instruction& inst) const -> size_t;
};

} // namespace tml::mir
