//! # MIR Optimization Pass Infrastructure
//!
//! This module provides the framework for MIR optimization passes.
//! Passes can transform MIR at different granularities:
//!
//! - **Module level**: Whole program transformations
//! - **Function level**: Per-function optimizations
//! - **Block level**: Per-basic-block transformations
//!
//! ## Creating a Pass
//!
//! Inherit from `MirPass`, `FunctionPass`, or `BlockPass` and implement
//! the appropriate `run` method:
//!
//! ```cpp
//! class MyPass : public FunctionPass {
//!     auto name() const -> std::string override { return "my-pass"; }
//!     auto run_on_function(Function& func) -> bool override {
//!         // Transform func, return true if changed
//!     }
//! };
//! ```
//!
//! ## Running Passes
//!
//! Use the `PassManager` to run optimization pipelines:
//!
//! ```cpp
//! PassManager pm(OptLevel::O2);
//! pm.configure_standard_pipeline();
//! pm.run(module);
//! ```

#pragma once

#include "mir/mir.hpp"

#include <memory>
#include <string>
#include <vector>

// Forward declaration for TypeEnv
namespace tml::types {
class TypeEnv;
}

namespace tml::mir {

// ============================================================================
// Pass Base Classes
// ============================================================================

/// Base class for all MIR optimization passes.
///
/// Subclass this for module-level passes. For function or block-level
/// passes, use `FunctionPass` or `BlockPass` instead.
class MirPass {
public:
    virtual ~MirPass() = default;

    /// Returns the pass name for debugging and logging.
    [[nodiscard]] virtual auto name() const -> std::string = 0;

    /// Runs the pass on a module. Returns true if any changes were made.
    virtual auto run(Module& module) -> bool = 0;
};

/// Function-level pass base class.
///
/// Iterates over all functions and calls `run_on_function` for each.
class FunctionPass : public MirPass {
public:
    auto run(Module& module) -> bool override;

protected:
    /// Override to implement function-level transformation.
    virtual auto run_on_function(Function& func) -> bool = 0;
};

/// Block-level pass base class.
///
/// Iterates over all basic blocks and calls `run_on_block` for each.
class BlockPass : public MirPass {
public:
    auto run(Module& module) -> bool override;

protected:
    /// Override to implement block-level transformation.
    virtual auto run_on_block(BasicBlock& block, Function& func) -> bool = 0;
};

// ============================================================================
// Pass Manager
// ============================================================================

/// Optimization level for pass configuration.
enum class OptLevel {
    O0, ///< No optimization (debug).
    O1, ///< Basic optimizations.
    O2, ///< Standard optimizations (default).
    O3, ///< Aggressive optimizations.
};

/// Pass manager - runs optimization passes in order.
///
/// Manages a pipeline of optimization passes and runs them on a module.
/// Configure with `configure_standard_pipeline()` or add passes manually.
class PassManager {
public:
    /// Creates a pass manager with the given optimization level.
    explicit PassManager(OptLevel level = OptLevel::O2);

    /// Adds a pass to the pipeline.
    void add_pass(std::unique_ptr<MirPass> pass);

    /// Runs all passes on a module. Returns the number of passes that changed it.
    auto run(Module& module) -> int;

    /// Configures the standard optimization pipeline based on optimization level.
    void configure_standard_pipeline();

    /// Configures the standard optimization pipeline with OOP optimizations.
    /// Requires TypeEnv for devirtualization and escape analysis passes.
    void configure_standard_pipeline(types::TypeEnv& env);

    /// Returns the optimization level.
    [[nodiscard]] auto opt_level() const -> OptLevel {
        return level_;
    }

private:
    OptLevel level_;
    std::vector<std::unique_ptr<MirPass>> passes_;
};

// ============================================================================
// Analysis Utilities
// ============================================================================

/// Returns true if a value is used anywhere in the function.
auto is_value_used(const Function& func, ValueId value) -> bool;

/// Returns true if an instruction has side effects (cannot be eliminated).
auto has_side_effects(const Instruction& inst) -> bool;

/// Returns true if an instruction produces a compile-time constant.
auto is_constant(const Instruction& inst) -> bool;

/// Returns the constant integer value if the instruction is a constant int.
auto get_constant_int(const Instruction& inst) -> std::optional<int64_t>;

/// Returns the constant bool value if the instruction is a constant bool.
auto get_constant_bool(const Instruction& inst) -> std::optional<bool>;

} // namespace tml::mir
