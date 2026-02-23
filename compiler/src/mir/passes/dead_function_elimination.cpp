TML_MODULE("compiler")

//! # Dead Function Elimination Pass
//!
//! This pass removes functions that are never called from the module.
//!
//! ## Liveness Analysis
//!
//! | Step | Action                                |
//! |------|---------------------------------------|
//! | 1    | Build call graph from all calls       |
//! | 2    | Mark entry points (main, @test, etc.) |
//! | 3    | Propagate liveness through call graph |
//! | 4    | Remove functions not marked as live   |
//!
//! ## Protected Functions
//!
//! Some functions are always kept:
//! - main: Program entry point
//! - @test functions: Test entry points
//! - @bench functions: Benchmark entry points
//! - @export functions: FFI exports
//! - run_all_*: Benchmark/test runners

#include "mir/passes/dead_function_elimination.hpp"

#include <algorithm>

namespace tml::mir {

// ============================================================================
// Helper Functions
// ============================================================================

// Check if a function is an entry point based on attributes
static auto is_entry_point(const Function& func) -> bool {
    // Main function is always an entry point
    if (func.name == "main") {
        return true;
    }

    // Check for entry point attributes
    for (const auto& attr : func.attributes) {
        if (attr == "test" || attr == "bench" || attr == "fuzz" || attr == "export" ||
            attr == "extern" || attr == "public") {
            return true;
        }
    }

    // run_all_* functions are entry points (benchmark/test runners)
    if (func.name.find("run_all_") == 0) {
        return true;
    }

    return false;
}

// ============================================================================
// DeadFunctionEliminationPass Implementation
// ============================================================================

auto DeadFunctionEliminationPass::run(Module& module) -> bool {
    stats_ = DeadFunctionStats{};
    live_functions_.clear();
    call_graph_.clear();

    stats_.functions_analyzed = module.functions.size();

    // Step 1: Build call graph
    build_call_graph(module);

    // Step 2: Mark entry points
    mark_entry_points(module);

    // Step 3: Propagate liveness
    propagate_liveness();

    // Step 4: Remove dead functions
    auto original_size = module.functions.size();

    // Count instructions in functions to be removed
    for (const auto& func : module.functions) {
        if (live_functions_.find(func.name) == live_functions_.end()) {
            stats_.instructions_removed += count_instructions(func);
        }
    }

    // Remove dead functions
    module.functions.erase(std::remove_if(module.functions.begin(), module.functions.end(),
                                          [this](const Function& func) {
                                              return live_functions_.find(func.name) ==
                                                     live_functions_.end();
                                          }),
                           module.functions.end());

    stats_.functions_removed = original_size - module.functions.size();
    stats_.functions_kept = module.functions.size();

    return stats_.functions_removed > 0;
}

void DeadFunctionEliminationPass::build_call_graph(const Module& module) {
    for (const auto& func : module.functions) {
        // Ensure function is in the graph even if it calls nothing
        call_graph_[func.name];

        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                // Check for direct calls
                if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                    call_graph_[func.name].insert(call->func_name);
                }
                // Check for method calls
                else if (auto* mcall = std::get_if<MethodCallInst>(&inst.inst)) {
                    // Method calls use mangled names like "TypeName_method"
                    // Construct the full mangled name from receiver_type and method_name
                    std::string mangled_name = mcall->receiver_type + "__" + mcall->method_name;
                    call_graph_[func.name].insert(mangled_name);

                    // If devirtualized, also add the devirtualized target
                    if (mcall->devirt_info.has_value()) {
                        std::string devirt_target = mcall->devirt_info->original_class + "__" +
                                                    mcall->devirt_info->method_name;
                        call_graph_[func.name].insert(devirt_target);
                    }
                }
            }
        }
    }
}

void DeadFunctionEliminationPass::mark_entry_points(const Module& module) {
    for (const auto& func : module.functions) {
        if (is_entry_point(func)) {
            live_functions_.insert(func.name);
            stats_.entry_points++;
        }
    }

    // If no entry points found, keep all functions (library mode)
    if (live_functions_.empty()) {
        for (const auto& func : module.functions) {
            live_functions_.insert(func.name);
        }
    }
}

void DeadFunctionEliminationPass::propagate_liveness() {
    bool changed = true;

    while (changed) {
        changed = false;

        for (const auto& live_func : live_functions_) {
            auto it = call_graph_.find(live_func);
            if (it == call_graph_.end()) {
                continue;
            }

            for (const auto& callee : it->second) {
                if (live_functions_.find(callee) == live_functions_.end()) {
                    // Check if callee exists in the module
                    if (call_graph_.find(callee) != call_graph_.end()) {
                        live_functions_.insert(callee);
                        changed = true;
                    }
                }
            }
        }
    }
}

auto DeadFunctionEliminationPass::count_instructions(const Function& func) const -> size_t {
    size_t count = 0;
    for (const auto& block : func.blocks) {
        count += block.instructions.size();
        if (block.terminator) {
            count++;
        }
    }
    return count;
}

} // namespace tml::mir
