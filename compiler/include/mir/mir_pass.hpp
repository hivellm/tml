#pragma once

// MIR Optimization Pass Infrastructure
//
// This provides the framework for MIR optimization passes.
// Passes can transform MIR at different granularities:
// - Module level (whole program)
// - Function level (per function)
// - Block level (per basic block)

#include "mir/mir.hpp"

#include <memory>
#include <string>
#include <vector>

namespace tml::mir {

// ============================================================================
// Pass Base Classes
// ============================================================================

// Base class for all MIR passes
class MirPass {
public:
    virtual ~MirPass() = default;

    // Pass name for debugging/logging
    [[nodiscard]] virtual auto name() const -> std::string = 0;

    // Run the pass on a module, returns true if any changes were made
    virtual auto run(Module& module) -> bool = 0;
};

// Function-level pass - operates on one function at a time
class FunctionPass : public MirPass {
public:
    auto run(Module& module) -> bool override;

protected:
    // Override this to implement function-level transformation
    virtual auto run_on_function(Function& func) -> bool = 0;
};

// Block-level pass - operates on one basic block at a time
class BlockPass : public MirPass {
public:
    auto run(Module& module) -> bool override;

protected:
    // Override this to implement block-level transformation
    virtual auto run_on_block(BasicBlock& block, Function& func) -> bool = 0;
};

// ============================================================================
// Pass Manager
// ============================================================================

// Optimization level
enum class OptLevel {
    O0, // No optimization
    O1, // Basic optimizations
    O2, // Standard optimizations
    O3, // Aggressive optimizations
};

// Pass manager - runs optimization passes in order
class PassManager {
public:
    explicit PassManager(OptLevel level = OptLevel::O2);

    // Add a pass to the pipeline
    void add_pass(std::unique_ptr<MirPass> pass);

    // Run all passes on a module
    // Returns the number of passes that made changes
    auto run(Module& module) -> int;

    // Configure standard optimization pipeline based on level
    void configure_standard_pipeline();

    // Get optimization level
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

// Check if a value is used in the function
auto is_value_used(const Function& func, ValueId value) -> bool;

// Check if an instruction has side effects
auto has_side_effects(const Instruction& inst) -> bool;

// Check if a constant instruction produces a known value
auto is_constant(const Instruction& inst) -> bool;

// Get constant integer value if the instruction is a constant int
auto get_constant_int(const Instruction& inst) -> std::optional<int64_t>;

// Get constant bool value if the instruction is a constant bool
auto get_constant_bool(const Instruction& inst) -> std::optional<bool>;

} // namespace tml::mir
