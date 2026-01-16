//! # Dead Virtual Method Elimination Pass
//!
//! Removes virtual methods that are never called at runtime.
//! This reduces binary size and enables further optimizations.
//!
//! ## Analysis Strategy
//!
//! ### 1. Entry Point Discovery
//!
//! Identify all entry points into the program:
//! - `main` function
//! - Exported functions (`@export`)
//! - Interface method implementations (conservatively kept)
//!
//! ### 2. Virtual Call Graph Construction
//!
//! Build a graph of potential virtual method calls:
//! - Direct calls to methods
//! - Virtual dispatch sites (receiver.method())
//! - Devirtualized calls track their original virtual target
//!
//! ### 3. Reachability Analysis
//!
//! Mark methods reachable from entry points:
//! - Follow static calls
//! - For virtual calls, mark all possible targets in hierarchy
//!
//! ### 4. Elimination
//!
//! Remove method bodies that are unreachable:
//! - Replace with trap/unreachable
//! - Update vtable entries (optional)
//!
//! ## Statistics
//!
//! The pass tracks how many methods were analyzed and eliminated.

#pragma once

#include "mir/mir_pass.hpp"
#include "mir/passes/devirtualization.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Statistics collected during dead method elimination.
struct DeadMethodStats {
    size_t total_methods = 0;        ///< Total methods in module.
    size_t entry_points = 0;         ///< Number of entry points.
    size_t reachable_methods = 0;    ///< Methods reachable from entry points.
    size_t unreachable_methods = 0;  ///< Methods that are dead.
    size_t methods_eliminated = 0;   ///< Methods actually removed.
    size_t virtual_methods = 0;      ///< Virtual methods analyzed.
    size_t dead_virtual_methods = 0; ///< Dead virtual methods.

    /// Elimination rate (0.0 to 1.0).
    [[nodiscard]] auto elimination_rate() const -> double {
        if (total_methods == 0)
            return 0.0;
        return static_cast<double>(methods_eliminated) / static_cast<double>(total_methods);
    }
};

/// Information about a method for reachability analysis.
struct MethodInfo {
    std::string full_name;                 ///< Full method name (Class_method).
    std::string class_name;                ///< Owning class name.
    std::string method_name;               ///< Method name without class prefix.
    bool is_virtual = false;               ///< True if method can be overridden.
    bool is_entry_point = false;           ///< True if this is an entry point.
    bool is_reachable = false;             ///< True if reachable from entry points.
    std::unordered_set<std::string> calls; ///< Methods this method calls.
};

/// Dead Virtual Method Elimination pass.
///
/// Analyzes method reachability and removes dead methods from the module.
class DeadMethodEliminationPass : public MirPass {
public:
    /// Creates a dead method elimination pass.
    /// @param devirt_pass Reference to devirtualization pass for hierarchy info.
    explicit DeadMethodEliminationPass(DevirtualizationPass& devirt_pass)
        : devirt_pass_(devirt_pass) {}

    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "DeadMethodElimination";
    }

    /// Runs dead method elimination on the entire module.
    auto run(Module& module) -> bool override;

    /// Returns elimination statistics.
    [[nodiscard]] auto get_stats() const -> DeadMethodStats {
        return stats_;
    }

    /// Checks if a method is reachable.
    [[nodiscard]] auto is_method_reachable(const std::string& method_name) const -> bool;

    /// Gets the set of dead methods.
    [[nodiscard]] auto get_dead_methods() const -> std::vector<std::string>;

private:
    DevirtualizationPass& devirt_pass_;
    DeadMethodStats stats_;

    // Method reachability data
    std::unordered_map<std::string, MethodInfo> method_info_;
    std::unordered_set<std::string> entry_points_;
    std::unordered_set<std::string> reachable_methods_;

    /// Discovers all entry points in the module.
    void discover_entry_points(const Module& module);

    /// Builds the call graph from all functions.
    void build_call_graph(const Module& module);

    /// Analyzes a function for calls.
    void analyze_function_calls(const Function& func);

    /// Propagates reachability from entry points.
    void propagate_reachability();

    /// Eliminates unreachable methods from the module.
    auto eliminate_dead_methods(Module& module) -> bool;

    /// Checks if a function is a class method.
    [[nodiscard]] auto is_class_method(const std::string& func_name) const -> bool;

    /// Extracts class name from a method name.
    [[nodiscard]] auto extract_class_name(const std::string& func_name) const -> std::string;

    /// Extracts method name (without class prefix) from a full name.
    [[nodiscard]] auto extract_method_name(const std::string& func_name) const -> std::string;

    /// Checks if a function is an entry point.
    [[nodiscard]] auto is_entry_point(const Function& func) const -> bool;

    /// Adds virtual method targets to reachable set.
    void add_virtual_targets(const std::string& class_name, const std::string& method_name);
};

} // namespace tml::mir
