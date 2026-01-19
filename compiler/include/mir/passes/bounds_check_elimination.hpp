//! # Bounds Check Elimination Pass
//!
//! This pass eliminates redundant array/slice bounds checks when the index
//! can be proven to be within bounds at compile time.
//!
//! ## Analysis Strategy
//!
//! 1. **Value Range Analysis**: Track integer ranges through the function
//! 2. **Loop Bounds Inference**: For `loop i in 0 to n`, derive `0 <= i < n`
//! 3. **Array Size Tracking**: Track known array sizes from literals/constants
//! 4. **Safe Index Identification**: Compare index range against array size
//!
//! ## Optimization Cases
//!
//! | Pattern | Condition | Optimization |
//! |---------|-----------|--------------|
//! | `arr[i]` in loop `i in 0 to arr.len()` | Index bounded by len | Remove check |
//! | `arr[0]` on non-empty array | Constant valid index | Remove check |
//! | `arr[i]` after `if i < arr.len()` | Dominated by check | Remove check |
//! | Sequential access `arr[i], arr[i+1]` | Range proven safe | Remove checks |

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace tml::mir {

/// Represents a range of possible integer values [min, max]
struct ValueRange {
    int64_t min = std::numeric_limits<int64_t>::min();
    int64_t max = std::numeric_limits<int64_t>::max();

    /// Check if this range is a single constant value
    [[nodiscard]] auto is_constant() const -> bool {
        return min == max;
    }

    /// Check if this range is fully bounded (not infinite)
    [[nodiscard]] auto is_bounded() const -> bool {
        return min != std::numeric_limits<int64_t>::min() &&
               max != std::numeric_limits<int64_t>::max();
    }

    /// Check if all values in this range are non-negative
    [[nodiscard]] auto is_non_negative() const -> bool {
        return min >= 0;
    }

    /// Check if all values in this range are less than a given bound
    [[nodiscard]] auto is_less_than(int64_t bound) const -> bool {
        return max < bound;
    }

    /// Check if this range is valid for array indexing with given size
    [[nodiscard]] auto is_valid_index_for(int64_t array_size) const -> bool {
        return min >= 0 && max < array_size;
    }

    /// Intersect two ranges
    [[nodiscard]] auto intersect(const ValueRange& other) const -> ValueRange {
        return ValueRange{std::max(min, other.min), std::min(max, other.max)};
    }

    /// Union two ranges (conservative - takes outer bounds)
    [[nodiscard]] auto union_with(const ValueRange& other) const -> ValueRange {
        return ValueRange{std::min(min, other.min), std::max(max, other.max)};
    }

    /// Create a constant range
    static auto constant(int64_t value) -> ValueRange {
        return ValueRange{value, value};
    }

    /// Create an unbounded range
    static auto unbounded() -> ValueRange {
        return ValueRange{};
    }

    /// Create a non-negative range [0, max]
    static auto non_negative(int64_t max_val = std::numeric_limits<int64_t>::max()) -> ValueRange {
        return ValueRange{0, max_val};
    }
};

/// Information about an array access that may have a bounds check
struct ArrayAccess {
    ValueId array_value; ///< The array being accessed
    ValueId index_value; ///< The index expression
    uint32_t block_id;   ///< Block containing the access
    size_t inst_index;   ///< Instruction index in the block
    int64_t array_size;  ///< Known array size (-1 if unknown)
    bool can_eliminate;  ///< Whether the bounds check can be eliminated
};

/// Statistics for bounds check elimination
struct BoundsCheckEliminationStats {
    size_t total_accesses = 0;    ///< Total array accesses analyzed
    size_t eliminated_checks = 0; ///< Bounds checks eliminated
    size_t loop_bounded = 0;      ///< Eliminated due to loop bounds
    size_t constant_index = 0;    ///< Eliminated due to constant index
    size_t dominated_check = 0;   ///< Eliminated due to dominating check
    size_t range_analysis = 0;    ///< Eliminated due to range analysis

    void reset() {
        *this = BoundsCheckEliminationStats{};
    }
};

/// Loop information for bounds analysis
struct LoopBoundsInfo {
    ValueId induction_var; ///< Loop induction variable
    int64_t start_value;   ///< Initial value
    int64_t end_value;     ///< End bound (exclusive for `to`, inclusive for `through`)
    int64_t step;          ///< Increment per iteration
    bool is_inclusive;     ///< True for `through`, false for `to`
    std::unordered_set<uint32_t> loop_blocks; ///< Blocks inside the loop
};

/// Bounds Check Elimination Pass
///
/// Analyzes array accesses and eliminates redundant bounds checks when
/// the index can be proven to be within bounds at compile time.
class BoundsCheckEliminationPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "BoundsCheckElimination";
    }

    /// Get statistics from the last run
    [[nodiscard]] auto stats() const -> const BoundsCheckEliminationStats& {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    BoundsCheckEliminationStats stats_;

    /// Map from value ID to its computed range
    std::unordered_map<ValueId, ValueRange> value_ranges_;

    /// Map from value ID to known array size
    std::unordered_map<ValueId, int64_t> array_sizes_;

    /// Detected loops with bounds information
    std::vector<LoopBoundsInfo> loops_;

    /// Array accesses that may have bounds checks
    std::vector<ArrayAccess> accesses_;

    // ============ Analysis Methods ============

    /// Analyze the function to gather information
    void analyze_function(const Function& func);

    /// Compute value ranges for all values in the function
    void compute_value_ranges(const Function& func);

    /// Detect loops and their bounds
    void detect_loops(const Function& func);

    /// Find all array accesses in the function
    void find_array_accesses(const Function& func);

    /// Get the range for a value
    [[nodiscard]] auto get_range(ValueId id) const -> ValueRange;

    /// Get the known size of an array value
    [[nodiscard]] auto get_array_size(ValueId id) const -> std::optional<int64_t>;

    /// Check if a value is an induction variable with known bounds
    [[nodiscard]] auto get_induction_bounds(ValueId id) const -> std::optional<ValueRange>;

    // ============ Range Computation ============

    /// Compute the range for a constant instruction
    [[nodiscard]] auto compute_constant_range(const ConstantInst& inst) const -> ValueRange;

    /// Compute the range for a binary operation
    [[nodiscard]] auto compute_binary_range(BinOp op, const ValueRange& left,
                                            const ValueRange& right) const -> ValueRange;

    /// Compute the range for a phi node (join of incoming ranges)
    [[nodiscard]] auto compute_phi_range(const PhiInst& phi) const -> ValueRange;

    // ============ Transformation ============

    /// Mark safe array accesses that don't need bounds checks
    void mark_safe_accesses();

    /// Apply the transformations to eliminate bounds checks
    auto apply_eliminations(Function& func) -> bool;
};

} // namespace tml::mir
