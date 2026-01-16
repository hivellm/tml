//! # Dead Virtual Method Elimination Pass Implementation
//!
//! Removes virtual methods that are never called at runtime.

#include "mir/passes/dead_method_elimination.hpp"

#include <algorithm>
#include <queue>

namespace tml::mir {

// ============================================================================
// Helper Functions
// ============================================================================

auto DeadMethodEliminationPass::is_class_method(const std::string& func_name) const -> bool {
    // Class methods have format: ClassName_methodName
    auto underscore = func_name.find('_');
    if (underscore == std::string::npos || underscore == 0) {
        return false;
    }
    // Check if the prefix is a known class
    std::string class_name = func_name.substr(0, underscore);
    return devirt_pass_.get_class_info(class_name) != nullptr;
}

auto DeadMethodEliminationPass::extract_class_name(const std::string& func_name) const
    -> std::string {
    auto underscore = func_name.find('_');
    if (underscore == std::string::npos) {
        return "";
    }
    return func_name.substr(0, underscore);
}

auto DeadMethodEliminationPass::extract_method_name(const std::string& func_name) const
    -> std::string {
    auto underscore = func_name.find('_');
    if (underscore == std::string::npos) {
        return func_name;
    }
    return func_name.substr(underscore + 1);
}

auto DeadMethodEliminationPass::is_entry_point(const Function& func) const -> bool {
    // Main function is always an entry point
    if (func.name == "main" || func.name == "tml_main") {
        return true;
    }

    // Check for @export attribute
    for (const auto& attr : func.attributes) {
        if (attr == "export" || attr == "test" || attr == "bench") {
            return true;
        }
    }

    // Constructors are entry points (they can be called from anywhere)
    auto underscore = func.name.rfind("_new");
    if (underscore != std::string::npos &&
        (underscore + 4 == func.name.length() || func.name[underscore + 4] == '_')) {
        return true;
    }

    // Interface implementations are conservatively kept as entry points
    // since they might be called through dynamic dispatch
    if (is_class_method(func.name)) {
        std::string class_name = extract_class_name(func.name);
        auto class_info = devirt_pass_.get_class_info(class_name);
        if (class_info && !class_info->interfaces.empty()) {
            // This class implements interfaces, keep its methods
            return true;
        }
    }

    return false;
}

// ============================================================================
// Entry Point Discovery
// ============================================================================

void DeadMethodEliminationPass::discover_entry_points(const Module& module) {
    entry_points_.clear();

    for (const auto& func : module.functions) {
        if (is_entry_point(func)) {
            entry_points_.insert(func.name);
            stats_.entry_points++;

            // Mark in method info
            auto it = method_info_.find(func.name);
            if (it != method_info_.end()) {
                it->second.is_entry_point = true;
            }
        }
    }
}

// ============================================================================
// Call Graph Construction
// ============================================================================

void DeadMethodEliminationPass::build_call_graph(const Module& module) {
    method_info_.clear();

    // First pass: create method info for all functions
    for (const auto& func : module.functions) {
        MethodInfo info;
        info.full_name = func.name;

        if (is_class_method(func.name)) {
            info.class_name = extract_class_name(func.name);
            info.method_name = extract_method_name(func.name);

            // Check if this is a virtual method
            auto class_info = devirt_pass_.get_class_info(info.class_name);
            if (class_info && !class_info->is_sealed) {
                // Method could be virtual if class is not sealed
                info.is_virtual = true;
                stats_.virtual_methods++;
            }
        } else {
            info.class_name = "";
            info.method_name = func.name;
        }

        method_info_[func.name] = std::move(info);
        stats_.total_methods++;
    }

    // Second pass: analyze calls in each function
    for (const auto& func : module.functions) {
        analyze_function_calls(func);
    }
}

void DeadMethodEliminationPass::analyze_function_calls(const Function& func) {
    auto it = method_info_.find(func.name);
    if (it == method_info_.end()) {
        return;
    }

    auto& info = it->second;

    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            // Check for direct calls
            if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                info.calls.insert(call->func_name);

                // If this was a devirtualized call, also track the original class method
                if (call->devirt_info.has_value()) {
                    const auto& devirt = *call->devirt_info;
                    std::string original_name = devirt.original_class + "_" + devirt.method_name;
                    info.calls.insert(original_name);
                }
            }

            // Check for virtual method calls
            if (auto* method_call = std::get_if<MethodCallInst>(&inst.inst)) {
                std::string target_name = method_call->receiver_type + "_" + method_call->method_name;
                info.calls.insert(target_name);

                // For virtual calls, we need to consider all possible targets
                // in the class hierarchy. This is done during reachability propagation.
            }
        }
    }
}

// ============================================================================
// Reachability Analysis
// ============================================================================

void DeadMethodEliminationPass::propagate_reachability() {
    reachable_methods_.clear();

    // Start with entry points
    std::queue<std::string> worklist;
    for (const auto& entry : entry_points_) {
        worklist.push(entry);
        reachable_methods_.insert(entry);
    }

    // BFS to find all reachable methods
    while (!worklist.empty()) {
        std::string current = worklist.front();
        worklist.pop();

        auto it = method_info_.find(current);
        if (it == method_info_.end()) {
            continue;
        }

        it->second.is_reachable = true;

        // Process all calls from this method
        for (const auto& callee : it->second.calls) {
            // Add directly called method
            if (reachable_methods_.insert(callee).second) {
                worklist.push(callee);
            }

            // For method calls, also add all virtual targets in hierarchy
            auto callee_it = method_info_.find(callee);
            if (callee_it != method_info_.end() && callee_it->second.is_virtual) {
                add_virtual_targets(callee_it->second.class_name, callee_it->second.method_name);
            }
        }
    }

    // Update statistics
    stats_.reachable_methods = reachable_methods_.size();
    stats_.unreachable_methods = stats_.total_methods - stats_.reachable_methods;
}

void DeadMethodEliminationPass::add_virtual_targets(const std::string& class_name,
                                                     const std::string& method_name) {
    auto class_info = devirt_pass_.get_class_info(class_name);
    if (!class_info) {
        return;
    }

    // Add all subclass implementations to reachable set
    for (const auto& subclass : class_info->all_subclasses) {
        std::string target = subclass + "_" + method_name;
        if (method_info_.find(target) != method_info_.end()) {
            if (reachable_methods_.insert(target).second) {
                auto it = method_info_.find(target);
                if (it != method_info_.end()) {
                    it->second.is_reachable = true;
                }
            }
        }
    }

    // Also add parent implementations (in case of calling super)
    if (class_info->base_class) {
        add_virtual_targets(*class_info->base_class, method_name);
    }
}

// ============================================================================
// Dead Method Elimination
// ============================================================================

auto DeadMethodEliminationPass::eliminate_dead_methods(Module& module) -> bool {
    bool changed = false;

    // Collect dead methods
    std::vector<std::string> dead_methods;
    for (const auto& [name, info] : method_info_) {
        if (!info.is_reachable && !info.is_entry_point) {
            dead_methods.push_back(name);
            if (info.is_virtual) {
                stats_.dead_virtual_methods++;
            }
        }
    }

    // Remove dead methods from module
    // Note: For now, we just mark them as dead rather than removing
    // to preserve vtable indices. Full removal would require vtable rebuilding.
    for (auto& func : module.functions) {
        if (std::find(dead_methods.begin(), dead_methods.end(), func.name) !=
            dead_methods.end()) {
            // Replace function body with unreachable
            // Keep the function signature for vtable compatibility
            func.blocks.clear();

            BasicBlock trap_block;
            trap_block.id = 0;
            trap_block.name = "entry";
            trap_block.terminator = UnreachableTerm{};

            func.blocks.push_back(std::move(trap_block));

            stats_.methods_eliminated++;
            changed = true;
        }
    }

    return changed;
}

// ============================================================================
// Public Interface
// ============================================================================

auto DeadMethodEliminationPass::run(Module& module) -> bool {
    stats_ = DeadMethodStats{};

    // Build call graph
    build_call_graph(module);

    // Discover entry points
    discover_entry_points(module);

    // Propagate reachability
    propagate_reachability();

    // Eliminate dead methods
    return eliminate_dead_methods(module);
}

auto DeadMethodEliminationPass::is_method_reachable(const std::string& method_name) const -> bool {
    return reachable_methods_.find(method_name) != reachable_methods_.end();
}

auto DeadMethodEliminationPass::get_dead_methods() const -> std::vector<std::string> {
    std::vector<std::string> dead;
    for (const auto& [name, info] : method_info_) {
        if (!info.is_reachable && !info.is_entry_point) {
            dead.push_back(name);
        }
    }
    return dead;
}

} // namespace tml::mir
