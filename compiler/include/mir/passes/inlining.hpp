//! # Function Inlining Optimization Pass
//!
//! Inlines function calls based on cost-benefit analysis and heuristics.
//! Inlining eliminates call overhead and enables further optimizations
//! by exposing the callee's code to the caller's context.
//!
//! ## Decision Factors
//!
//! - **Instruction count**: Larger functions have higher cost
//! - **Call site context**: Hot paths get higher threshold
//! - **Attributes**: `@inline` forces inlining, `@noinline` prevents it
//! - **Recursion**: Limits depth to prevent infinite expansion
//! - **Optimization level**: `-O3` is more aggressive than `-O1`
//!
//! ## Cost Model
//!
//! ```
//! net_cost = instruction_cost - call_overhead_saved
//! should_inline = net_cost <= threshold
//! ```
//!
//! ## Passes
//!
//! - **InliningPass**: Cost-based inlining with configurable thresholds
//! - **AlwaysInlinePass**: Handles `@inline` attributed functions
//!
//! ## When to Run
//!
//! Run early in the optimization pipeline. Inlining exposes opportunities
//! for constant propagation, DCE, and other optimizations.

#pragma once

#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Inlining decision for a call site.
enum class InlineDecision {
    Inline,         ///< Should inline (cost analysis passed).
    NoInline,       ///< Should not inline (cost too high).
    AlwaysInline,   ///< Must inline (`@inline` attribute).
    NeverInline,    ///< Must not inline (`@noinline` attribute).
    RecursiveLimit, ///< Hit recursive inlining depth limit.
    TooLarge,       ///< Callee exceeds maximum size.
    NoDefinition    ///< No function definition available.
};

/// Cost analysis result for an inlining decision.
struct InlineCost {
    int instruction_cost = 0;    ///< Weighted cost of callee instructions.
    int call_overhead_saved = 0; ///< Overhead eliminated by inlining.
    int size_increase = 0;       ///< Code size increase in bytes.
    int threshold = 0;           ///< Threshold for this call site.

    /// Returns true if inlining is beneficial based on cost analysis.
    [[nodiscard]] auto should_inline() const -> bool {
        return instruction_cost - call_overhead_saved <= threshold;
    }

    /// Returns the net cost (positive = expensive, negative = beneficial).
    [[nodiscard]] auto net_cost() const -> int {
        return instruction_cost - call_overhead_saved;
    }
};

/// Statistics collected during inlining.
struct InliningStats {
    size_t calls_analyzed = 0;             ///< Total call sites examined.
    size_t calls_inlined = 0;              ///< Calls that were inlined.
    size_t calls_not_inlined = 0;          ///< Calls rejected by cost analysis.
    size_t always_inline = 0;              ///< Calls inlined due to `@inline`.
    size_t never_inline = 0;               ///< Calls blocked by `@noinline`.
    size_t recursive_limit_hit = 0;        ///< Calls blocked by recursion limit.
    size_t too_large = 0;                  ///< Calls blocked by size limit.
    size_t no_definition = 0;              ///< Calls with no available definition.
    size_t total_instructions_inlined = 0; ///< Total instructions copied.

    // Devirtualized call statistics
    size_t devirt_calls_analyzed = 0; ///< Devirtualized calls examined.
    size_t devirt_calls_inlined = 0;  ///< Devirtualized calls that were inlined.
    size_t devirt_sealed_inlined = 0; ///< Inlined from sealed class devirt.
    size_t devirt_exact_inlined = 0;  ///< Inlined from exact type devirt.
    size_t devirt_single_inlined = 0; ///< Inlined from single impl devirt.

    // Constructor inlining statistics
    size_t constructor_calls_analyzed = 0; ///< Constructor calls examined.
    size_t constructor_calls_inlined = 0;  ///< Constructor calls that were inlined.
    size_t base_constructor_inlined = 0;   ///< Base constructor calls inlined.
};

/// Configuration options for the inlining pass.
struct InliningOptions {
    int base_threshold = 250;   ///< Base cost threshold for inlining.
    int recursive_limit = 3;    ///< Maximum recursive inlining depth.
    int max_callee_size = 500;  ///< Maximum instructions in callee.
    int call_penalty = 20;      ///< Cost assigned to call instructions.
    int alloca_bonus = 10;      ///< Bonus for eliminating stack allocations.
    bool inline_cold = false;   ///< Whether to inline cold (rarely executed) code.
    bool inline_hot = true;     ///< Whether to prioritize hot (frequently executed) code.
    int optimization_level = 2; ///< Optimization level (affects thresholds).

    // Devirtualized call options
    int devirt_bonus = 100;        ///< Threshold bonus for devirtualized calls.
    int devirt_exact_bonus = 150;  ///< Extra bonus for exact type devirtualization.
    int devirt_sealed_bonus = 120; ///< Extra bonus for sealed class devirtualization.
    bool prioritize_devirt = true; ///< Whether to prioritize devirtualized calls.

    // Constructor inlining options
    int constructor_bonus = 200;         ///< Threshold bonus for constructor calls.
    int base_constructor_bonus = 250;    ///< Extra bonus for base constructor chains.
    bool prioritize_constructors = true; ///< Whether to prioritize constructor inlining.

    // Single-expression method options (getters/setters)
    bool always_inline_single_expr = true;  ///< Always inline methods with single expression.
    int single_expr_max_size = 3;           ///< Max instructions to be considered single-expression.
};

/// Function inlining pass.
///
/// Inlines function calls based on cost analysis. Works at module level
/// to access all function definitions for cross-function inlining.
class InliningPass : public MirPass {
public:
    /// Creates an inlining pass with the given options.
    explicit InliningPass(InliningOptions opts = {}) : options_(opts) {}

    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "Inlining";
    }

    /// Runs inlining on the entire module.
    auto run(Module& module) -> bool override;

    /// Queries the inlining decision for a specific call site.
    [[nodiscard]] auto get_decision(const std::string& caller, const std::string& callee) const
        -> InlineDecision;

    /// Analyzes the cost of inlining a specific call.
    [[nodiscard]] auto analyze_cost(const CallInst& call, const Function& callee) const
        -> InlineCost;

    /// Returns inlining statistics.
    [[nodiscard]] auto get_stats() const -> InliningStats {
        return stats_;
    }

    /// Updates inlining options.
    void set_options(const InliningOptions& opts) {
        options_ = opts;
    }

private:
    InliningOptions options_;
    InliningStats stats_;
    std::unordered_map<std::string, const Function*> function_map_;
    std::unordered_map<std::string, std::unordered_set<std::string>> call_graph_;
    std::unordered_map<std::string, int> inline_depth_;
    int inline_counter_ = 0; ///< Counter for generating unique block names.

    void build_call_graph(Module& module);
    [[nodiscard]] auto calculate_threshold(const Function& caller, const CallInst& call) const
        -> int;
    [[nodiscard]] auto count_instructions(const Function& func) const -> int;
    auto inline_call(Function& caller, BasicBlock& block, size_t call_index, const Function& callee)
        -> bool;
    auto clone_function_body(const Function& callee, ValueId first_new_id,
                             const std::vector<Value>& args, int inline_id)
        -> std::vector<BasicBlock>;
    void remap_values(std::vector<BasicBlock>& blocks,
                      const std::unordered_map<ValueId, ValueId>& value_map);
};

/// Always-inline pass.
///
/// Handles functions marked with `@inline` attribute. These are inlined
/// unconditionally, regardless of cost analysis.
class AlwaysInlinePass : public MirPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "AlwaysInline";
    }

    /// Runs always-inline on the entire module.
    auto run(Module& module) -> bool override;

    /// Returns inlining statistics.
    [[nodiscard]] auto get_stats() const -> InliningStats {
        return stats_;
    }

private:
    InliningStats stats_;
};

} // namespace tml::mir
