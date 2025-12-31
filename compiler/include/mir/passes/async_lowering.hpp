#pragma once

// Async Lowering Pass
//
// This pass transforms async functions into state machines.
// For each async function with suspension points (await expressions),
// it generates:
// 1. A state struct containing saved locals and current state
// 2. A state machine that can be resumed from any suspension point
// 3. Proper Poll::Ready / Poll::Pending handling
//
// Example transformation:
//   async func fetch() -> I64 {
//       let a = await service1()  // suspension point 1
//       let b = await service2(a) // suspension point 2
//       return a + b
//   }
//
// Becomes:
//   struct fetch_state { state: I32, a: I64, b: I64, ... }
//   func fetch_poll(state: ptr fetch_state) -> Poll[I64] {
//       match state.state:
//           0 => { ... call service1, if Pending return Pending, else continue }
//           1 => { ... call service2, if Pending return Pending, else continue }
//           2 => { return Ready(a + b) }
//   }

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <set>
#include <unordered_map>

namespace tml::mir {

// ============================================================================
// Async Analysis
// ============================================================================

// Analyze an async function to find suspension points and saved locals
class AsyncAnalysis {
public:
    explicit AsyncAnalysis(const Function& func);

    // Run the analysis
    void analyze();

    // Get suspension points (await instructions)
    [[nodiscard]] auto suspension_points() const -> const std::vector<SuspensionPoint>&;

    // Get locals that must be saved across suspensions
    [[nodiscard]] auto saved_locals() const -> const std::vector<SavedLocal>&;

    // Check if analysis found any suspension points
    [[nodiscard]] auto has_suspensions() const -> bool;

private:
    const Function& func_;
    std::vector<SuspensionPoint> suspensions_;
    std::vector<SavedLocal> saved_locals_;

    // Find all await instructions in the function
    void find_suspensions();

    // Analyze which locals are live across suspension points
    void analyze_saved_locals();

    // Check if a value is live at a given block
    [[nodiscard]] auto is_live_at(ValueId value, uint32_t block_id) const -> bool;

    // Get all values defined before a block
    [[nodiscard]] auto get_defs_before(uint32_t block_id) const -> std::set<ValueId>;

    // Get all values used after a block
    [[nodiscard]] auto get_uses_after(uint32_t block_id) const -> std::set<ValueId>;
};

// ============================================================================
// Async Lowering Pass
// ============================================================================

class AsyncLoweringPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "async-lowering";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Transform an async function into a state machine
    void transform_to_state_machine(Function& func, const AsyncAnalysis& analysis);

    // Generate the state struct type
    auto generate_state_struct(const Function& func, const AsyncAnalysis& analysis) -> StructDef;

    // Generate state transition blocks
    void generate_state_blocks(Function& func, const AsyncAnalysis& analysis);

    // Create the poll check and branch for an await
    void generate_await_handler(Function& func, const SuspensionPoint& sp, uint32_t state_ptr);
};

// ============================================================================
// Helper Functions
// ============================================================================

// Check if a function needs async lowering (has await instructions)
auto needs_async_lowering(const Function& func) -> bool;

// Get the inner type from a Poll[T] type
auto get_poll_inner_type(const MirTypePtr& poll_type) -> MirTypePtr;

// Create a Poll[T] type for a given inner type
auto make_poll_type(const MirTypePtr& inner_type) -> MirTypePtr;

} // namespace tml::mir
