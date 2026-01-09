//! # Tail Call Optimization Pass
//!
//! Marks tail calls for optimization by the backend. A call is a tail call if:
//! - It is immediately followed by a return
//! - The return value is exactly the call result (or void)
//! - No cleanup is needed after the call
//!
//! ## Benefits
//!
//! - Converts recursive calls to loops (prevents stack overflow)
//! - Reduces call overhead
//!
//! ## Example
//!
//! Before:
//! ```
//! func factorial(n: I32, acc: I32) -> I32 {
//!     if n <= 1 { return acc }
//!     return factorial(n - 1, n * acc)  // tail call
//! }
//! ```
//!
//! After (in IR):
//! ```
//! %result = tail call factorial(%n_minus_1, %new_acc)
//! ret %result
//! ```
//!
//! ## Limitations
//!
//! This pass only marks tail calls. The actual optimization (converting to
//! jumps) is done by the LLVM backend.

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_set>

namespace tml::mir {

// Extended CallInst with tail call flag
// Note: This pass sets a flag that the codegen can use

class TailCallPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "TailCall";
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    // Check if a call can be a tail call
    auto is_tail_call_candidate(const Function& func, const BasicBlock& block,
                                size_t inst_idx) -> bool;

    // Check if the instruction at inst_idx is immediately followed by a return
    // of its result
    auto is_followed_by_return(const BasicBlock& block, size_t inst_idx,
                               ValueId call_result) -> bool;

    // Mark a call as a tail call (for codegen)
    // Since we can't modify CallInst structure, we use function attributes
    // or a separate data structure
    std::unordered_set<ValueId> tail_calls_;

public:
    // Check if a call result is a tail call
    auto is_tail_call(ValueId call_result) const -> bool {
        return tail_calls_.count(call_result) > 0;
    }
};

} // namespace tml::mir
