#pragma once

// Function Inlining Optimization Pass
//
// Inlines function calls based on cost analysis and heuristics.
//
// The pass considers:
// - Instruction count in callee
// - Call site context
// - @inline and @noinline attributes
// - Recursive inlining limits
// - Optimization level

#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/**
 * Inlining decision
 */
enum class InlineDecision {
    Inline,         // Should inline
    NoInline,       // Should not inline (cost too high)
    AlwaysInline,   // Must inline (@inline attribute)
    NeverInline,    // Must not inline (@noinline attribute)
    RecursiveLimit, // Hit recursive inlining limit
    TooLarge,       // Callee is too large
    NoDefinition    // No function definition available
};

/**
 * Inlining cost analysis result
 */
struct InlineCost {
    int instruction_cost = 0;    // Cost of instructions
    int call_overhead_saved = 0; // Overhead saved by inlining
    int size_increase = 0;       // Code size increase
    int threshold = 0;           // Threshold for this call site

    [[nodiscard]] auto should_inline() const -> bool {
        return instruction_cost - call_overhead_saved <= threshold;
    }

    [[nodiscard]] auto net_cost() const -> int {
        return instruction_cost - call_overhead_saved;
    }
};

/**
 * Inlining statistics
 */
struct InliningStats {
    size_t calls_analyzed = 0;
    size_t calls_inlined = 0;
    size_t calls_not_inlined = 0;
    size_t always_inline = 0;
    size_t never_inline = 0;
    size_t recursive_limit_hit = 0;
    size_t too_large = 0;
    size_t no_definition = 0;
    size_t total_instructions_inlined = 0;
};

/**
 * Inlining options
 */
struct InliningOptions {
    int base_threshold = 250;   // Base cost threshold
    int recursive_limit = 3;    // Max recursive inlines
    int max_callee_size = 500;  // Max instructions in callee
    int call_penalty = 20;      // Cost of a call instruction
    int alloca_bonus = 10;      // Bonus for eliminating alloca
    bool inline_cold = false;   // Inline cold functions
    bool inline_hot = true;     // Prioritize hot functions
    int optimization_level = 2; // -O level affects thresholds
};

/**
 * Function inlining pass
 *
 * Inlines function calls based on cost analysis and heuristics.
 * Works at the module level to access all function definitions.
 */
class InliningPass : public MirPass {
public:
    explicit InliningPass(InliningOptions opts = {}) : options_(opts) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "Inlining";
    }

    auto run(Module& module) -> bool override;

    // Query inlining decision for a call site
    [[nodiscard]] auto get_decision(const std::string& caller, const std::string& callee) const
        -> InlineDecision;

    // Get cost analysis for a call site
    [[nodiscard]] auto analyze_cost(const CallInst& call, const Function& callee) const
        -> InlineCost;

    // Get statistics
    [[nodiscard]] auto get_stats() const -> InliningStats {
        return stats_;
    }

    // Set options
    void set_options(const InliningOptions& opts) {
        options_ = opts;
    }

private:
    InliningOptions options_;
    InliningStats stats_;
    std::unordered_map<std::string, const Function*> function_map_;
    std::unordered_map<std::string, std::unordered_set<std::string>> call_graph_;
    std::unordered_map<std::string, int> inline_depth_; // Track recursive depth

    // Build function map and call graph
    void build_call_graph(Module& module);

    // Calculate cost threshold based on context
    [[nodiscard]] auto calculate_threshold(const Function& caller, const CallInst& call) const
        -> int;

    // Count instructions in a function
    [[nodiscard]] auto count_instructions(const Function& func) const -> int;

    // Perform the actual inlining
    auto inline_call(Function& caller, BasicBlock& block, size_t call_index, const Function& callee)
        -> bool;

    // Clone a function's body for inlining
    auto clone_function_body(const Function& callee, ValueId first_new_id,
                             const std::vector<Value>& args) -> std::vector<BasicBlock>;

    // Remap value IDs in cloned blocks
    void remap_values(std::vector<BasicBlock>& blocks,
                      const std::unordered_map<ValueId, ValueId>& value_map);
};

/**
 * Always-inline pass
 *
 * Handles functions marked with @inline attribute.
 * These are inlined regardless of cost analysis.
 */
class AlwaysInlinePass : public MirPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "AlwaysInline";
    }

    auto run(Module& module) -> bool override;

    [[nodiscard]] auto get_stats() const -> InliningStats {
        return stats_;
    }

private:
    InliningStats stats_;
};

} // namespace tml::mir
