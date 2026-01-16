//! # Batch Destruction Optimization Pass
//!
//! This MIR pass optimizes destruction of arrays and collections by
//! batching individual destructor calls into efficient loops.
//!
//! ## Optimization Patterns
//!
//! ### 1. Array Destruction
//!
//! Before (individual calls):
//! ```
//! call @drop(array[0])
//! call @drop(array[1])
//! ...
//! call @drop(array[N-1])
//! ```
//!
//! After (batched loop):
//! ```
//! for i in 0..N:
//!     call @drop(array[i])
//! ```
//!
//! ### 2. Trivial Destructor Vectorization
//!
//! For types with trivial destructors (only freeing memory), the
//! destructor loop can be replaced with a single bulk free operation.
//!
//! ### 3. Collection Clearing
//!
//! Detects patterns like clearing a List/Vec and batches the element
//! destruction into an efficient loop.

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"
#include "types/env.hpp"

#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Information about a batch of consecutive destructor calls
struct DestructorBatch {
    ValueId array_ptr;                ///< Array being destroyed
    std::string element_type;         ///< Type of elements
    size_t start_idx;                 ///< First instruction index
    size_t end_idx;                   ///< Last instruction index (exclusive)
    size_t element_count;             ///< Number of elements
    bool is_trivial;                  ///< Destructor is trivial (just free)
    std::vector<size_t> inst_indices; ///< Indices of individual drop calls
};

/// Statistics for batch destruction
struct BatchDestructionStats {
    size_t batches_found = 0;
    size_t calls_batched = 0;
    size_t trivial_vectorized = 0;
};

/// Batch destruction optimization pass
class BatchDestructionPass : public FunctionPass {
public:
    explicit BatchDestructionPass(types::TypeEnv& env) : env_(env) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "BatchDestruction";
    }

    /// Get statistics from last run
    [[nodiscard]] auto stats() const -> const BatchDestructionStats& {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    types::TypeEnv& env_;
    BatchDestructionStats stats_;

    /// Find consecutive destructor calls to array elements
    auto find_destructor_batches(const Function& func, BasicBlock& block)
        -> std::vector<DestructorBatch>;

    /// Check if a destructor is trivial (just frees memory)
    auto is_trivial_destructor(const std::string& type_name) -> bool;

    /// Replace batch of destructor calls with a loop
    auto replace_with_loop(Function& func, BasicBlock& block, const DestructorBatch& batch) -> bool;

    /// Vectorize trivial destructor batch (bulk free)
    auto vectorize_trivial(Function& func, BasicBlock& block, const DestructorBatch& batch) -> bool;

    /// Check if two drop calls are for consecutive array elements
    auto are_consecutive_array_drops(const CallInst& call1, const CallInst& call2,
                                     const Function& func) -> bool;

    /// Extract array pointer and index from a drop call argument
    auto extract_array_access(const Value& ptr, const Function& func)
        -> std::optional<std::pair<ValueId, int64_t>>;
};

} // namespace tml::mir
