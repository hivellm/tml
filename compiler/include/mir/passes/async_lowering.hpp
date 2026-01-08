//! # Async Lowering Pass
//!
//! Transforms async functions into state machines. Each async function with
//! suspension points (await expressions) is converted to:
//!
//! 1. A **state struct** containing saved locals and current state
//! 2. A **poll function** that resumes from any suspension point
//! 3. Proper `Poll::Ready` / `Poll::Pending` handling
//!
//! ## Example Transformation
//!
//! Original async function:
//! ```tml
//! async func fetch() -> I64 {
//!     let a = await service1()  // suspension point 1
//!     let b = await service2(a) // suspension point 2
//!     return a + b
//! }
//! ```
//!
//! Generated state machine:
//! ```tml
//! struct fetch_state { state: I32, a: I64, b: I64, ... }
//!
//! func fetch_poll(state: ptr fetch_state) -> Poll[I64] {
//!     when state.state:
//!         0 => { /* call service1, return Pending or continue */ }
//!         1 => { /* call service2, return Pending or continue */ }
//!         2 => { return Ready(a + b) }
//! }
//! ```
//!
//! ## Analysis Phase
//!
//! Before transformation, `AsyncAnalysis` identifies:
//! - All suspension points (await expressions)
//! - Locals that are live across suspension points (must be saved)
//!
//! ## When to Run
//!
//! Run late in the pipeline, after most optimizations. The generated
//! state machine code can still benefit from subsequent passes.

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <set>
#include <unordered_map>

namespace tml::mir {

// ============================================================================
// Async Analysis
// ============================================================================

/// Analyzes an async function to find suspension points and saved locals.
///
/// This analysis determines which values must be saved in the state struct
/// (those live across suspension points) and identifies all await expressions.
class AsyncAnalysis {
public:
    /// Creates an analysis for the given async function.
    explicit AsyncAnalysis(const Function& func);

    /// Runs the analysis, populating suspension points and saved locals.
    void analyze();

    /// Returns all suspension points (await instructions).
    [[nodiscard]] auto suspension_points() const -> const std::vector<SuspensionPoint>&;

    /// Returns locals that must be saved across suspensions.
    [[nodiscard]] auto saved_locals() const -> const std::vector<SavedLocal>&;

    /// Returns true if the function has any suspension points.
    [[nodiscard]] auto has_suspensions() const -> bool;

private:
    const Function& func_;
    std::vector<SuspensionPoint> suspensions_;
    std::vector<SavedLocal> saved_locals_;

    void find_suspensions();
    void analyze_saved_locals();
    [[nodiscard]] auto is_live_at(ValueId value, uint32_t block_id) const -> bool;
    [[nodiscard]] auto get_defs_before(uint32_t block_id) const -> std::set<ValueId>;
    [[nodiscard]] auto get_uses_after(uint32_t block_id) const -> std::set<ValueId>;
};

// ============================================================================
// Async Lowering Pass
// ============================================================================

/// Async lowering pass.
///
/// Transforms async functions into state machine implementations that
/// can be polled for completion.
class AsyncLoweringPass : public FunctionPass {
public:
    /// Returns the pass name for logging.
    [[nodiscard]] auto name() const -> std::string override {
        return "async-lowering";
    }

protected:
    /// Runs async lowering on a single function.
    auto run_on_function(Function& func) -> bool override;

private:
    /// Transforms an async function into a state machine.
    void transform_to_state_machine(Function& func, const AsyncAnalysis& analysis);

    /// Generates the state struct type for saved locals.
    auto generate_state_struct(const Function& func, const AsyncAnalysis& analysis) -> StructDef;

    /// Generates the state transition basic blocks.
    void generate_state_blocks(Function& func, const AsyncAnalysis& analysis);

    /// Creates the poll check and branch logic for an await.
    void generate_await_handler(Function& func, const SuspensionPoint& sp, uint32_t state_ptr);
};

// ============================================================================
// Helper Functions
// ============================================================================

/// Checks if a function needs async lowering (has await instructions).
auto needs_async_lowering(const Function& func) -> bool;

/// Extracts the inner type T from a Poll[T] type.
auto get_poll_inner_type(const MirTypePtr& poll_type) -> MirTypePtr;

/// Creates a Poll[T] type wrapping the given inner type.
auto make_poll_type(const MirTypePtr& inner_type) -> MirTypePtr;

} // namespace tml::mir
