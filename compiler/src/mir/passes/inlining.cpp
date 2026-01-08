//! # Function Inlining Pass
//!
//! This pass replaces function calls with the callee's body.
//!
//! ## Inlining Decisions
//!
//! | Attribute       | Behavior                           |
//! |-----------------|------------------------------------|
//! | @inline         | Always inline (up to recursion)    |
//! | @always_inline  | Always inline                      |
//! | @noinline       | Never inline                       |
//! | @never_inline   | Never inline                       |
//! | (none)          | Heuristic-based decision           |
//!
//! ## Optimization Levels
//!
//! | Level | Threshold Multiplier |
//! |-------|---------------------|
//! | -O0   | No inlining         |
//! | -O1   | 1x (conservative)   |
//! | -O2   | 2x (moderate)       |
//! | -O3   | 4x (aggressive)     |
//!
//! ## Inlining Process
//!
//! 1. Build call graph and function map
//! 2. Analyze each call site for cost/benefit
//! 3. Clone callee body with value remapping
//! 4. Replace call with inlined code
//! 5. Iterate until no more inlining possible

#include "mir/passes/inlining.hpp"

#include <algorithm>

namespace tml::mir {

// ============================================================================
// Helper Functions
// ============================================================================

// Check if function has @inline or @always_inline attribute
static auto has_inline_attr(const Function& func) -> bool {
    for (const auto& attr : func.attributes) {
        if (attr == "inline" || attr == "always_inline") {
            return true;
        }
    }
    return false;
}

// Check if function has @noinline or @never_inline attribute
static auto has_noinline_attr(const Function& func) -> bool {
    for (const auto& attr : func.attributes) {
        if (attr == "noinline" || attr == "never_inline") {
            return true;
        }
    }
    return false;
}

// ============================================================================
// InliningPass Implementation
// ============================================================================

auto InliningPass::run(Module& module) -> bool {
    bool changed = false;
    stats_ = InliningStats{};

    // Build call graph and function map
    build_call_graph(module);

    // Adjust thresholds based on optimization level
    int threshold_multiplier = 1;
    switch (options_.optimization_level) {
    case 0:
        threshold_multiplier = 0; // No inlining at -O0
        break;
    case 1:
        threshold_multiplier = 1;
        break;
    case 2:
        threshold_multiplier = 2;
        break;
    case 3:
        threshold_multiplier = 4;
        break;
    default:
        threshold_multiplier = 2;
    }

    if (threshold_multiplier == 0) {
        return false; // No inlining at -O0
    }

    // Iterate until no more inlining possible
    bool made_progress = true;
    int iteration = 0;
    const int max_iterations = 5;

    while (made_progress && iteration < max_iterations) {
        made_progress = false;
        iteration++;

        for (auto& func : module.functions) {
            // Process each block
            for (auto& block : func.blocks) {
                for (size_t i = 0; i < block.instructions.size(); ++i) {
                    auto& inst = block.instructions[i];

                    // Check if this is a call instruction
                    auto* call = std::get_if<CallInst>(&inst.inst);
                    if (!call)
                        continue;

                    stats_.calls_analyzed++;

                    // Find the callee
                    auto callee_it = function_map_.find(call->func_name);
                    if (callee_it == function_map_.end()) {
                        stats_.no_definition++;
                        continue;
                    }

                    const Function* callee = callee_it->second;

                    // Check for @noinline attribute
                    if (has_noinline_attr(*callee)) {
                        stats_.never_inline++;
                        continue;
                    }

                    // Check for @inline attribute - always inline regardless of cost
                    if (has_inline_attr(*callee)) {
                        // Check recursive limit even for @inline
                        std::string call_key = func.name + "->" + callee->name;
                        if (inline_depth_[call_key] >= options_.recursive_limit) {
                            stats_.recursive_limit_hit++;
                            continue;
                        }

                        inline_depth_[call_key]++;
                        int callee_size = count_instructions(*callee);

                        if (inline_call(func, block, i, *callee)) {
                            changed = true;
                            made_progress = true;
                            stats_.calls_inlined++;
                            stats_.always_inline++;
                            stats_.total_instructions_inlined += static_cast<size_t>(callee_size);
                            i = static_cast<size_t>(-1);
                        }
                        continue;
                    }

                    // Check recursive limit
                    std::string call_key = func.name + "->" + callee->name;
                    if (inline_depth_[call_key] >= options_.recursive_limit) {
                        stats_.recursive_limit_hit++;
                        continue;
                    }

                    // Check callee size
                    int callee_size = count_instructions(*callee);
                    if (callee_size > options_.max_callee_size) {
                        stats_.too_large++;
                        continue;
                    }

                    // Analyze cost
                    InlineCost cost = analyze_cost(*call, *callee);
                    cost.threshold = calculate_threshold(func, *call) * threshold_multiplier;

                    if (cost.should_inline()) {
                        inline_depth_[call_key]++;

                        if (inline_call(func, block, i, *callee)) {
                            changed = true;
                            made_progress = true;
                            stats_.calls_inlined++;
                            stats_.total_instructions_inlined += static_cast<size_t>(callee_size);
                            // Restart block processing
                            i = static_cast<size_t>(-1);
                        }
                    } else {
                        stats_.calls_not_inlined++;
                    }
                }
            }
        }
    }

    return changed;
}

void InliningPass::build_call_graph(Module& module) {
    function_map_.clear();
    call_graph_.clear();
    inline_depth_.clear();

    // Build function map
    for (const auto& func : module.functions) {
        function_map_[func.name] = &func;
    }

    // Build call graph
    for (const auto& func : module.functions) {
        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                    call_graph_[func.name].insert(call->func_name);
                }
            }
        }
    }
}

auto InliningPass::calculate_threshold([[maybe_unused]] const Function& caller,
                                       const CallInst& call) const -> int {
    int threshold = options_.base_threshold;

    // Bonus for small callees
    auto callee_it = function_map_.find(call.func_name);
    if (callee_it != function_map_.end()) {
        int size = count_instructions(*callee_it->second);
        if (size < 10) {
            threshold += 100; // Small functions get bonus
        } else if (size < 50) {
            threshold += 50;
        }
    }

    return threshold;
}

auto InliningPass::count_instructions(const Function& func) const -> int {
    int count = 0;
    for (const auto& block : func.blocks) {
        count += static_cast<int>(block.instructions.size());
        if (block.terminator) {
            count++; // Count terminator
        }
    }
    return count;
}

auto InliningPass::analyze_cost([[maybe_unused]] const CallInst& call, const Function& callee) const
    -> InlineCost {
    InlineCost cost;

    // Base cost is instruction count
    cost.instruction_cost = count_instructions(callee);

    // Call overhead saved
    cost.call_overhead_saved = options_.call_penalty;

    // Size increase estimate
    cost.size_increase = cost.instruction_cost;

    return cost;
}

auto InliningPass::inline_call(Function& caller, BasicBlock& block, size_t call_index,
                               const Function& callee) -> bool {
    // Get the call instruction
    if (call_index >= block.instructions.size())
        return false;

    auto& inst = block.instructions[call_index];
    auto* call = std::get_if<CallInst>(&inst.inst);
    if (!call)
        return false;

    // Clone callee's body
    auto inlined_blocks = clone_function_body(callee, caller.next_value_id, call->args);

    if (inlined_blocks.empty())
        return false;

    // Update caller's next_value_id
    for (const auto& cloned_block : inlined_blocks) {
        for (const auto& cloned_inst : cloned_block.instructions) {
            if (cloned_inst.result != INVALID_VALUE && cloned_inst.result >= caller.next_value_id) {
                caller.next_value_id = cloned_inst.result + 1;
            }
        }
    }

    // Split the call block and insert inlined code
    // This is a simplified version - full implementation would:
    // 1. Split the block at the call site
    // 2. Connect entry of inlined code to first half
    // 3. Connect return points to second half
    // 4. Handle return value properly

    // For now, just add the blocks and remove the call
    // (Simplified implementation)
    block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(call_index));

    // Add inlined blocks to the function
    for (auto& cloned_block : inlined_blocks) {
        cloned_block.id = caller.next_block_id++;
        caller.blocks.push_back(std::move(cloned_block));
    }

    return true;
}

auto InliningPass::clone_function_body(const Function& callee, ValueId first_new_id,
                                       const std::vector<Value>& args) -> std::vector<BasicBlock> {
    std::vector<BasicBlock> cloned;

    // Build value remapping from callee parameters to call arguments
    std::unordered_map<ValueId, ValueId> value_map;
    for (size_t i = 0; i < callee.params.size() && i < args.size(); ++i) {
        value_map[callee.params[i].value_id] = args[i].id;
    }

    // Generate new value IDs for all instructions
    ValueId next_id = first_new_id;
    for (const auto& block : callee.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result != INVALID_VALUE) {
                value_map[inst.result] = next_id++;
            }
        }
    }

    // Clone each block
    for (const auto& block : callee.blocks) {
        BasicBlock new_block;
        new_block.id = block.id; // Will be remapped by caller
        new_block.name = "inline_" + block.name;

        // Clone instructions
        for (const auto& inst : block.instructions) {
            InstructionData new_inst = inst;

            // Remap result
            if (inst.result != INVALID_VALUE) {
                auto it = value_map.find(inst.result);
                if (it != value_map.end()) {
                    new_inst.result = it->second;
                }
            }

            // Remap operands
            std::visit(
                [&value_map](auto& i) {
                    using T = std::decay_t<decltype(i)>;

                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        auto it = value_map.find(i.left.id);
                        if (it != value_map.end())
                            i.left.id = it->second;
                        it = value_map.find(i.right.id);
                        if (it != value_map.end())
                            i.right.id = it->second;
                    } else if constexpr (std::is_same_v<T, UnaryInst>) {
                        auto it = value_map.find(i.operand.id);
                        if (it != value_map.end())
                            i.operand.id = it->second;
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        auto it = value_map.find(i.ptr.id);
                        if (it != value_map.end())
                            i.ptr.id = it->second;
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        auto it = value_map.find(i.ptr.id);
                        if (it != value_map.end())
                            i.ptr.id = it->second;
                        it = value_map.find(i.value.id);
                        if (it != value_map.end())
                            i.value.id = it->second;
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        for (auto& arg : i.args) {
                            auto it = value_map.find(arg.id);
                            if (it != value_map.end())
                                arg.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, SelectInst>) {
                        auto it = value_map.find(i.condition.id);
                        if (it != value_map.end())
                            i.condition.id = it->second;
                        it = value_map.find(i.true_val.id);
                        if (it != value_map.end())
                            i.true_val.id = it->second;
                        it = value_map.find(i.false_val.id);
                        if (it != value_map.end())
                            i.false_val.id = it->second;
                    }
                },
                new_inst.inst);

            new_block.instructions.push_back(new_inst);
        }

        // Clone terminator
        if (block.terminator) {
            new_block.terminator = *block.terminator;

            // Remap values in terminator
            std::visit(
                [&value_map](auto& term) {
                    using T = std::decay_t<decltype(term)>;

                    if constexpr (std::is_same_v<T, ReturnTerm>) {
                        if (term.value) {
                            auto it = value_map.find(term.value->id);
                            if (it != value_map.end()) {
                                term.value->id = it->second;
                            }
                        }
                    } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                        auto it = value_map.find(term.condition.id);
                        if (it != value_map.end()) {
                            term.condition.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, SwitchTerm>) {
                        auto it = value_map.find(term.discriminant.id);
                        if (it != value_map.end()) {
                            term.discriminant.id = it->second;
                        }
                    }
                },
                *new_block.terminator);
        }

        cloned.push_back(std::move(new_block));
    }

    return cloned;
}

void InliningPass::remap_values(
    [[maybe_unused]] std::vector<BasicBlock>& blocks,
    [[maybe_unused]] const std::unordered_map<ValueId, ValueId>& value_map) {
    // Already done in clone_function_body
}

auto InliningPass::get_decision(const std::string& caller, const std::string& callee) const
    -> InlineDecision {
    auto it = function_map_.find(callee);
    if (it == function_map_.end()) {
        return InlineDecision::NoDefinition;
    }

    const Function* func = it->second;

    // Check for @noinline attribute
    if (has_noinline_attr(*func)) {
        return InlineDecision::NeverInline;
    }

    // Check for @inline attribute
    if (has_inline_attr(*func)) {
        return InlineDecision::AlwaysInline;
    }

    std::string key = caller + "->" + callee;
    auto depth_it = inline_depth_.find(key);
    if (depth_it != inline_depth_.end() && depth_it->second >= options_.recursive_limit) {
        return InlineDecision::RecursiveLimit;
    }

    if (count_instructions(*func) > options_.max_callee_size) {
        return InlineDecision::TooLarge;
    }

    return InlineDecision::Inline;
}

// ============================================================================
// AlwaysInlinePass Implementation
// ============================================================================

auto AlwaysInlinePass::run([[maybe_unused]] Module& module) -> bool {
    // This pass identifies @inline functions
    // Actual inlining is handled by InliningPass
    // For now, just return false (no changes)
    stats_ = InliningStats{};
    return false;
}

} // namespace tml::mir
