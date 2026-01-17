//! # Builder Pattern Optimization Pass
//!
//! Detects and optimizes method chaining patterns (builder pattern):
//!
//! ```tml
//! let config = ConfigBuilder::new()
//!     .set_name("foo")     // Returns self
//!     .set_value(42)       // Returns self
//!     .set_enabled(true)   // Returns self
//!     .build()             // Returns final object
//! ```
//!
//! ## Optimizations
//!
//! 1. **Intermediate Object Elimination**: When a method returns `self`,
//!    the return value is the same object - no allocation needed.
//!
//! 2. **Chain Fusion**: Consecutive method calls on the same object
//!    can be fused into a single scope for SROA.
//!
//! 3. **Copy Elision**: When the final object is assigned, avoid copies
//!    by building directly into the target location (RVO/NRVO).
//!
//! ## Detection
//!
//! A method is part of a builder pattern if:
//! - It returns `self` type (same type as receiver)
//! - It's chained with other methods
//! - The chain ends with a terminal method (build, finish, create)

#pragma once

#include "mir/mir_pass.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::mir {

/// Information about a self-returning method (builder method).
struct BuilderMethodInfo {
    std::string class_name;       ///< Class the method belongs to.
    std::string method_name;      ///< Name of the method.
    bool returns_self = false;    ///< True if method returns `self`.
    bool is_terminal = false;     ///< True if method is terminal (build, finish).
    bool modifies_state = false;  ///< True if method modifies object state.
};

/// Information about a method chain in the code.
struct MethodChain {
    ValueId receiver;                   ///< Initial receiver object.
    std::vector<ValueId> call_results;  ///< Results of each call in chain.
    std::vector<std::string> methods;   ///< Method names in order.
    bool has_terminal = false;          ///< True if chain ends with terminal.
    ValueId final_result = INVALID_VALUE; ///< Result of terminal method.
};

/// Statistics for builder pattern optimization.
struct BuilderOptStats {
    size_t methods_analyzed = 0;        ///< Total methods examined.
    size_t builder_methods_found = 0;   ///< Methods returning self.
    size_t chains_detected = 0;         ///< Method chains found.
    size_t intermediates_eliminated = 0; ///< Intermediate objects eliminated.
    size_t copies_elided = 0;           ///< Copies avoided (RVO/NRVO).
    size_t chains_fused = 0;            ///< Chains fused for optimization.
};

/// Builder pattern optimization pass.
///
/// Detects and optimizes method chaining patterns common in builder APIs.
/// This enables LLVM to treat the entire chain as a single allocation
/// and optimize away intermediate copies.
class BuilderOptPass : public MirPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "BuilderOpt";
    }

    /// Runs builder pattern optimization on the module.
    auto run(Module& module) -> bool override;

    /// Returns optimization statistics.
    [[nodiscard]] auto get_stats() const -> BuilderOptStats {
        return stats_;
    }

    /// Checks if a method returns self (is a builder method).
    [[nodiscard]] auto is_builder_method(const std::string& class_name,
                                         const std::string& method_name) const -> bool;

    /// Gets information about a builder method.
    [[nodiscard]] auto get_method_info(const std::string& class_name,
                                       const std::string& method_name) const
        -> const BuilderMethodInfo*;

private:
    BuilderOptStats stats_;

    // Builder method cache: class_name -> method_name -> info
    mutable std::unordered_map<std::string,
        std::unordered_map<std::string, BuilderMethodInfo>> builder_methods_;

    // Known terminal method names
    std::unordered_set<std::string> terminal_methods_ = {
        "build", "finish", "create", "done", "complete", "finalize", "make"
    };

    /// Analyzes a module to find builder methods.
    void analyze_builder_methods(const Module& module);

    /// Detects method chains in a function.
    auto detect_chains(const Function& func) -> std::vector<MethodChain>;

    /// Optimizes a method chain for intermediate elimination.
    auto optimize_chain(Function& func, const MethodChain& chain) -> bool;

    /// Checks if a type returns self (by analyzing return type).
    auto returns_self_type(const Function& method) const -> bool;

    /// Checks if a method name is a terminal method.
    [[nodiscard]] auto is_terminal_method(const std::string& name) const -> bool;

    /// Marks intermediate allocations as eliminable.
    void mark_intermediates_eliminable(Function& func, const MethodChain& chain);

    /// Applies copy elision to a chain.
    auto apply_copy_elision(Function& func, const MethodChain& chain) -> bool;
};

} // namespace tml::mir
