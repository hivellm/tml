//! # Advanced Loop Optimizations
//!
//! This module provides advanced loop transformations for better performance.
//!
//! ## Loop Interchange
//!
//! Swaps the order of nested loops to improve cache locality:
//!
//! ```tml
//! // Before: Poor cache locality (column-major access)
//! loop i in 0 to N {
//!     loop j in 0 to M {
//!         arr[j][i] = value  // Strided access
//!     }
//! }
//!
//! // After: Good cache locality (row-major access)
//! loop j in 0 to M {
//!     loop i in 0 to N {
//!         arr[j][i] = value  // Sequential access
//!     }
//! }
//! ```
//!
//! ## Loop Tiling (Blocking)
//!
//! Divides loop iterations into smaller tiles for cache reuse:
//!
//! ```tml
//! // Before: Large working set
//! loop i in 0 to N {
//!     loop j in 0 to M {
//!         process(arr[i][j])
//!     }
//! }
//!
//! // After: Tiled for cache locality
//! loop ii in 0 to N by TILE_SIZE {
//!     loop jj in 0 to M by TILE_SIZE {
//!         loop i in ii to min(ii + TILE_SIZE, N) {
//!             loop j in jj to min(jj + TILE_SIZE, M) {
//!                 process(arr[i][j])
//!             }
//!         }
//!     }
//! }
//! ```
//!
//! ## Loop Fusion
//!
//! Combines adjacent loops with the same bounds:
//!
//! ```tml
//! // Before: Two separate loops
//! loop i in 0 to N { arr[i] = i * 2 }
//! loop i in 0 to N { brr[i] = arr[i] + 1 }
//!
//! // After: Single fused loop
//! loop i in 0 to N {
//!     arr[i] = i * 2
//!     brr[i] = arr[i] + 1
//! }
//! ```
//!
//! ## Loop Distribution
//!
//! Splits a loop with independent parts:
//!
//! ```tml
//! // Before: Mixed operations
//! loop i in 0 to N {
//!     arr[i] = compute_a(i)  // Independent
//!     brr[i] = compute_b(i)  // Independent
//! }
//!
//! // After: Separate loops for better vectorization
//! loop i in 0 to N { arr[i] = compute_a(i) }
//! loop i in 0 to N { brr[i] = compute_b(i) }
//! ```

#pragma once

#include "mir/mir.hpp"
#include "mir/mir_pass.hpp"

#include <unordered_set>
#include <vector>

namespace tml::mir {

/// Information about a loop's bounds and structure
struct LoopInfo {
    uint32_t header_block;                    ///< Loop header block
    uint32_t latch_block;                     ///< Back-edge source block
    std::unordered_set<uint32_t> body_blocks; ///< All blocks in loop body
    ValueId induction_var;                    ///< Loop induction variable
    std::optional<int64_t> start;             ///< Start value (if constant)
    std::optional<int64_t> end;               ///< End value (if constant)
    std::optional<int64_t> step;              ///< Step value (if constant)
    uint32_t depth = 0;                       ///< Nesting depth (0 = outermost)
    LoopInfo* parent = nullptr;               ///< Parent loop (if nested)
    std::vector<LoopInfo*> children;          ///< Child loops (nested inside this)
};

/// Statistics for loop optimizations
struct LoopOptStats {
    size_t loops_analyzed = 0;
    size_t interchanges_applied = 0;
    size_t tiles_applied = 0;
    size_t fusions_applied = 0;
    size_t distributions_applied = 0;

    void reset() {
        *this = LoopOptStats{};
    }
};

/// Loop Interchange Pass
///
/// Swaps the order of nested loops to improve cache locality.
class LoopInterchangePass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "LoopInterchange";
    }

    [[nodiscard]] auto stats() const -> const LoopOptStats& {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    LoopOptStats stats_;
    std::vector<LoopInfo> loops_;

    /// Analyze function for loop nests
    void analyze_loops(const Function& func);

    /// Check if two loops can be legally interchanged
    auto can_interchange(const LoopInfo& outer, const LoopInfo& inner) const -> bool;

    /// Perform the interchange transformation
    auto do_interchange(Function& func, LoopInfo& outer, LoopInfo& inner) -> bool;

    /// Check for dependencies that prevent interchange
    auto has_interchange_preventing_deps(const Function& func, const LoopInfo& outer,
                                         const LoopInfo& inner) const -> bool;
};

/// Loop Tiling (Blocking) Pass
///
/// Divides loop iterations into smaller tiles for cache reuse.
class LoopTilingPass : public FunctionPass {
public:
    explicit LoopTilingPass(size_t tile_size = 32) : tile_size_(tile_size) {}

    [[nodiscard]] auto name() const -> std::string override {
        return "LoopTiling";
    }

    [[nodiscard]] auto stats() const -> const LoopOptStats& {
        return stats_;
    }

    void set_tile_size(size_t size) {
        tile_size_ = size;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    LoopOptStats stats_;
    size_t tile_size_;

    /// Check if a loop is a good candidate for tiling
    auto should_tile(const LoopInfo& loop) const -> bool;

    /// Apply tiling transformation to a loop
    auto apply_tiling(Function& func, LoopInfo& loop) -> bool;
};

/// Loop Fusion Pass
///
/// Combines adjacent loops with the same bounds.
class LoopFusionPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "LoopFusion";
    }

    [[nodiscard]] auto stats() const -> const LoopOptStats& {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    LoopOptStats stats_;

    /// Check if two adjacent loops can be fused
    auto can_fuse(const LoopInfo& loop1, const LoopInfo& loop2) const -> bool;

    /// Check if loops have compatible bounds
    auto have_same_bounds(const LoopInfo& loop1, const LoopInfo& loop2) const -> bool;

    /// Check for fusion-preventing dependencies
    auto has_fusion_preventing_deps(const Function& func, const LoopInfo& loop1,
                                    const LoopInfo& loop2) const -> bool;

    /// Fuse two loops together
    auto do_fusion(Function& func, LoopInfo& loop1, LoopInfo& loop2) -> bool;
};

/// Loop Distribution Pass
///
/// Splits a loop with independent parts for better optimization.
class LoopDistributionPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "LoopDistribution";
    }

    [[nodiscard]] auto stats() const -> const LoopOptStats& {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    LoopOptStats stats_;

    /// Find groups of independent statements in a loop
    auto find_independent_groups(const Function& func, const LoopInfo& loop)
        -> std::vector<std::vector<size_t>>;

    /// Check if distributing would be beneficial
    auto should_distribute(const LoopInfo& loop,
                           const std::vector<std::vector<size_t>>& groups) const -> bool;

    /// Distribute a loop into multiple loops
    auto do_distribution(Function& func, LoopInfo& loop,
                         const std::vector<std::vector<size_t>>& groups) -> bool;
};

/// Combined advanced loop optimization pass
class AdvancedLoopOptPass : public FunctionPass {
public:
    [[nodiscard]] auto name() const -> std::string override {
        return "AdvancedLoopOpt";
    }

    [[nodiscard]] auto stats() const -> const LoopOptStats& {
        return stats_;
    }

protected:
    auto run_on_function(Function& func) -> bool override;

private:
    LoopOptStats stats_;
    std::vector<LoopInfo> loops_;

    /// Analyze all loops in function
    void analyze_loops(const Function& func);

    /// Build loop nesting tree
    void build_loop_tree();

    /// Apply all applicable loop optimizations
    auto optimize_loop(Function& func, LoopInfo& loop) -> bool;
};

} // namespace tml::mir
