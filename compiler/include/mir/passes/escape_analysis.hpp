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

/// Conditional escape information for branch-dependent escapes.
struct ConditionalEscape {
    ValueId condition;       ///< Condition value that determines escape.
    EscapeState true_state;  ///< Escape state when condition is true.
    EscapeState false_state; ///< Escape state when condition is false.
};

/// Conditional allocation information for branch-dependent allocations.
/// When allocations occur in different branches, they may share a stack slot.
struct ConditionalAllocation {
    ValueId phi_result;                ///< Result of the phi node merging allocations.
    std::vector<ValueId> alloc_ids;    ///< Allocation value IDs in each branch.
    std::vector<uint32_t> from_blocks; ///< Block IDs containing each allocation.
    size_t max_size = 0;               ///< Maximum size of all allocations.
    bool can_share_slot = true;        ///< True if allocations can share stack slot.
    std::string class_name;            ///< Class name if all are same class.
};

/// Loop allocation information for allocations inside loops.
/// Allocations that don't escape the loop iteration can be stack-promoted
/// with the stack slot reused on each iteration.
struct LoopAllocation {
    ValueId alloc_id;               ///< The allocation value ID.
    uint32_t loop_header;           ///< Block ID of the loop header.
    uint32_t alloc_block;           ///< Block ID containing the allocation.
    bool escapes_iteration = false; ///< True if value escapes current iteration.
    bool is_loop_invariant = false; ///< True if allocation can be hoisted.
    size_t estimated_size = 0;      ///< Estimated allocation size.
    std::string class_name;         ///< Class name if this is a class instance.
};

/// Escape information for a single value.
struct EscapeInfo {
    EscapeState state = EscapeState::Unknown; ///< Escape state.
    bool may_alias_heap = false;              ///< May alias heap-allocated memory.
    bool may_alias_global = false;            ///< May alias global variables.
    bool is_stack_promotable = false;         ///< Can be promoted to stack allocation.
    bool is_class_instance = false;           ///< Is this a class instance allocation?
    std::string class_name;                   ///< Class name if is_class_instance is true.
    bool is_arena_allocated = false;          ///< Allocated via arena (skip destructor).
    bool free_can_be_removed = false;         ///< Corresponding free can be removed.
    std::vector<ConditionalEscape> conditional_escapes; ///< Branch-dependent escapes.

    /// Returns true if the value escapes the function.
    [[nodiscard]] auto escapes() const -> bool {
        return state != EscapeState::NoEscape;
    }

    /// Returns true if the value only escapes conditionally.
    [[nodiscard]] auto has_conditional_escape() const -> bool {
        return !conditional_escapes.empty();
    }

    /// Returns the most optimistic escape state (considering conditions).
    [[nodiscard]] auto optimistic_state() const -> EscapeState {
        if (conditional_escapes.empty()) {
            return state;
        }
        EscapeState min_state = state;
        for (const auto& ce : conditional_escapes) {
            if (ce.true_state < min_state)
                min_state = ce.true_state;
            if (ce.false_state < min_state)
                min_state = ce.false_state;
        }
        return min_state;
    }
};

// Forward declaration for friend
class EscapeAndPromotePass;

/// Escape analysis pass.
///
/// Analyzes heap allocations to determine which can be safely
/// converted to stack allocations. Results are queried by the
/// `StackPromotionPass`.
///
/// ## Sealed Class Optimization
///
/// For sealed classes, the pass uses fast-path analysis:
/// - Method calls on sealed class instances don't escape `this`
/// - Constructor calls for sealed classes mark result as stack-promotable
/// - Field stores to sealed class instances have bounded escape
class EscapeAnalysisPass : public FunctionPass {
    friend class EscapeAndPromotePass;

public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "EscapeAnalysis";
    }

    /// Sets the module reference for class metadata lookup.
    void set_module(const Module* module) {
        module_ = module;
    }

    /// Queries escape information for a specific value.
    [[nodiscard]] auto get_escape_info(ValueId value) const -> EscapeInfo;

    /// Checks if a value can be safely promoted to stack allocation.
    [[nodiscard]] auto can_stack_promote(ValueId value) const -> bool;

    /// Returns all value IDs that are candidates for stack promotion.
    [[nodiscard]] auto get_stack_promotable() const -> std::vector<ValueId>;

    /// Returns conditional allocations identified in the function.
    [[nodiscard]] auto get_conditional_allocations() const
        -> const std::vector<ConditionalAllocation>&;

    /// Returns loop allocations identified in the function.
    [[nodiscard]] auto get_loop_allocations() const -> const std::vector<LoopAllocation>&;

    /// Statistics from the escape analysis.
    struct Stats {
        size_t total_allocations = 0; ///< Total allocations analyzed.
        size_t no_escape = 0;         ///< Allocations that don't escape.
        size_t arg_escape = 0;        ///< Allocations escaping via arguments.
        size_t return_escape = 0;     ///< Allocations escaping via return.
        size_t global_escape = 0;     ///< Allocations escaping to globals.
        size_t stack_promotable = 0;  ///< Allocations eligible for stack promotion.

        // Class instance statistics
        size_t class_instances = 0;            ///< Total class instance allocations.
        size_t class_instances_no_escape = 0;  ///< Class instances that don't escape.
        size_t class_instances_promotable = 0; ///< Class instances eligible for stack.
        size_t method_call_escapes = 0;        ///< Escapes via method calls.
        size_t field_store_escapes = 0;        ///< Escapes via field stores.

        // Advanced escape analysis statistics
        size_t conditional_escapes = 0; ///< Values with conditional escapes.
        size_t arena_allocations = 0;   ///< Allocations via arena.
        size_t free_removals = 0;       ///< Free calls that can be removed.

        // Sealed class optimization statistics
        size_t sealed_class_instances = 0;  ///< Sealed class instance allocations.
        size_t sealed_class_promotable = 0; ///< Sealed instances eligible for stack.
        size_t sealed_method_noescapes = 0; ///< Method calls that don't escape due to sealed.

        // Conditional allocation statistics
        size_t conditional_allocations_found = 0; ///< Phi nodes merging allocations.
        size_t conditional_allocs_shareable = 0;  ///< Allocations that can share slot.

        // Loop allocation statistics
        size_t loop_allocations_found = 0; ///< Allocations inside loops.
        size_t loop_allocs_promotable = 0; ///< Loop allocations promotable to stack.
        size_t loop_allocs_hoistable = 0;  ///< Allocations that can be hoisted out.
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
    std::vector<ConditionalAllocation> conditional_allocs_;
    std::vector<LoopAllocation> loop_allocs_;
    std::unordered_set<uint32_t> loop_headers_;            ///< Block IDs that are loop headers.
    std::unordered_map<uint32_t, uint32_t> block_to_loop_; ///< Maps block to its loop header.
    Stats stats_;
    const Module* module_ = nullptr; ///< Module reference for class metadata lookup.

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

    // Advanced escape analysis helpers
    [[nodiscard]] auto is_arena_alloc_call(const std::string& func_name) const -> bool;
    void analyze_conditional_branch(const CondBranchTerm& branch, Function& func);
    void track_conditional_escape(ValueId value, ValueId condition, EscapeState true_state,
                                  EscapeState false_state);
    void analyze_free_removal(const CallInst& call, ValueId value);
    [[nodiscard]] auto can_remove_free(ValueId value) const -> bool;

    // Sealed class optimization helpers
    [[nodiscard]] auto is_sealed_class(const std::string& class_name) const -> bool;
    [[nodiscard]] auto is_stack_allocatable_class(const std::string& class_name) const -> bool;
    [[nodiscard]] auto get_class_metadata(const std::string& class_name) const
        -> std::optional<ClassMetadata>;
    void apply_sealed_class_optimization(const std::string& class_name, EscapeInfo& info);

    // Conditional allocation helpers
    void find_conditional_allocations(Function& func);
    void analyze_phi_for_allocations(const PhiInst& phi, ValueId phi_result, Function& func);

    // Loop allocation helpers
    void identify_loops(Function& func);
    void find_loop_allocations(Function& func);
    [[nodiscard]] auto is_in_loop(uint32_t block_id) const -> bool;
    [[nodiscard]] auto get_loop_header(uint32_t block_id) const -> uint32_t;
    void analyze_loop_escape(LoopAllocation& loop_alloc, Function& func);
};

/// Stack promotion pass.
///
/// Converts heap allocations that don't escape to stack allocations.
/// Runs after `EscapeAnalysisPass` and uses its results.
class StackPromotionPass : public FunctionPass {
    friend class EscapeAndPromotePass;

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
        size_t allocations_promoted = 0;     ///< Number of allocations converted.
        size_t bytes_saved = 0;              ///< Estimated bytes saved from heap.
        size_t free_calls_removed = 0;       ///< Number of free/drop calls removed.
        size_t destructors_inserted = 0;     ///< Number of destructor calls inserted at scope end.
        size_t conditional_slots_shared = 0; ///< Number of conditional allocs sharing a slot.
        size_t conditional_allocs_promoted = 0; ///< Number of conditional allocations promoted.
        size_t loop_allocs_promoted = 0;        ///< Number of loop allocations promoted.
        size_t loop_allocs_hoisted = 0;         ///< Number of loop allocations hoisted out.
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

    /// Marks an instruction as stack-eligible by setting its is_stack_eligible flag.
    void mark_stack_eligible(InstructionData& inst);

    /// Removes free/drop calls for a promoted allocation.
    void remove_free_calls(Function& func, ValueId promoted_value);

    /// Inserts destructor call at scope exit for a promoted class instance.
    void insert_destructor(Function& func, ValueId value, const std::string& class_name);

    /// Promotes conditional allocations that share a stack slot.
    void promote_conditional_allocations(Function& func);

    /// Promotes loop allocations that can be reused per-iteration.
    void promote_loop_allocations(Function& func);

    /// Tracks which allocations have been promoted (for free removal).
    std::unordered_set<ValueId> promoted_values_;

    /// Tracks shared stack slots for conditional allocations (phi_result -> slot_id).
    std::unordered_map<ValueId, ValueId> shared_stack_slots_;

    /// Tracks loop allocations that were hoisted to the loop preheader.
    std::unordered_set<ValueId> hoisted_loop_allocs_;
};

/// Combined escape analysis and stack promotion pass.
///
/// This pass runs escape analysis followed by stack promotion in a single pass.
/// This is more efficient than running them separately because:
/// 1. We don't need to store results between passes
/// 2. Stack promotion runs immediately with fresh analysis results
class EscapeAndPromotePass : public FunctionPass {
public:
    EscapeAndPromotePass() = default;

    [[nodiscard]] auto name() const -> std::string override {
        return "EscapeAndPromote";
    }

    auto get_escape_stats() const -> EscapeAnalysisPass::Stats {
        return escape_pass_.get_stats();
    }
    auto get_promotion_stats() const -> StackPromotionPass::Stats {
        return promotion_pass_ ? promotion_pass_->get_stats() : StackPromotionPass::Stats{};
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    EscapeAnalysisPass escape_pass_;
    std::unique_ptr<StackPromotionPass> promotion_pass_;
};

} // namespace tml::mir
