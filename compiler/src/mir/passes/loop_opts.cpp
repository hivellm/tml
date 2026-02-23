TML_MODULE("compiler")

//! # Advanced Loop Optimizations - Implementation

#include "mir/passes/loop_opts.hpp"

#include <algorithm>

namespace tml::mir {

// ============================================================================
// Loop Analysis Helpers
// ============================================================================

/// Detect loops in function using back-edge detection
static std::vector<LoopInfo> detect_loops(const Function& func) {
    std::vector<LoopInfo> loops;

    // Simple loop detection: look for back edges (branch to earlier block)
    for (size_t i = 0; i < func.blocks.size(); i++) {
        const auto& block = func.blocks[i];

        if (block.terminator) {
            auto check_backedge = [&](uint32_t target) {
                if (target < static_cast<uint32_t>(i)) {
                    // This is a back edge - indicates a loop
                    LoopInfo loop;
                    loop.header_block = target;
                    loop.latch_block = static_cast<uint32_t>(i);

                    // Add all blocks between header and latch to body
                    for (uint32_t b = target; b <= static_cast<uint32_t>(i); b++) {
                        loop.body_blocks.insert(b);
                    }

                    loops.push_back(loop);
                }
            };

            if (auto* branch = std::get_if<BranchTerm>(&*block.terminator)) {
                check_backedge(branch->target);
            } else if (auto* cond = std::get_if<CondBranchTerm>(&*block.terminator)) {
                check_backedge(cond->true_block);
                check_backedge(cond->false_block);
            }
        }
    }

    return loops;
}

/// Check if loop has constant bounds
static bool has_constant_bounds(const LoopInfo& loop) {
    return loop.start.has_value() && loop.end.has_value() && loop.step.has_value();
}

// ============================================================================
// LoopInterchangePass
// ============================================================================

auto LoopInterchangePass::run_on_function(Function& func) -> bool {
    stats_.reset();
    loops_.clear();

    // Analyze loops
    analyze_loops(func);
    stats_.loops_analyzed = loops_.size();

    bool changed = false;

    // Look for nested loop pairs that can be interchanged
    for (auto& outer : loops_) {
        for (auto* inner : outer.children) {
            if (inner && can_interchange(outer, *inner)) {
                if (do_interchange(func, outer, *inner)) {
                    stats_.interchanges_applied++;
                    changed = true;
                }
            }
        }
    }

    return changed;
}

void LoopInterchangePass::analyze_loops(const Function& func) {
    loops_ = detect_loops(func);

    // Build nesting relationship
    for (size_t i = 0; i < loops_.size(); i++) {
        for (size_t j = 0; j < loops_.size(); j++) {
            if (i != j) {
                // Check if loop j is nested inside loop i
                if (loops_[i].body_blocks.count(loops_[j].header_block) > 0) {
                    loops_[j].parent = &loops_[i];
                    loops_[j].depth = loops_[i].depth + 1;
                    loops_[i].children.push_back(&loops_[j]);
                }
            }
        }
    }
}

auto LoopInterchangePass::can_interchange(const LoopInfo& outer, const LoopInfo& inner) const
    -> bool {
    // Basic requirements for interchange:
    // 1. Both loops must be perfectly nested
    // 2. Loop bounds must be loop-invariant relative to each other
    // 3. No dependencies that prevent interchange

    // Check that inner loop is the only thing in outer loop body
    // (simplified check - real implementation would be more thorough)
    if (outer.body_blocks.size() != inner.body_blocks.size() + 1) {
        return false; // Outer loop has other content besides inner loop
    }

    return true;
}

auto LoopInterchangePass::do_interchange(Function& func, LoopInfo& outer, LoopInfo& inner) -> bool {
    // Placeholder for actual interchange transformation
    // Would need to:
    // 1. Swap the induction variables
    // 2. Swap the bounds
    // 3. Update array index expressions
    (void)func;
    (void)outer;
    (void)inner;
    return false;
}

auto LoopInterchangePass::has_interchange_preventing_deps(const Function& func,
                                                          const LoopInfo& outer,
                                                          const LoopInfo& inner) const -> bool {
    // Check for dependencies that would be violated by interchange
    // e.g., anti-dependencies, output dependencies
    (void)func;
    (void)outer;
    (void)inner;
    return false;
}

// ============================================================================
// LoopTilingPass
// ============================================================================

auto LoopTilingPass::run_on_function(Function& func) -> bool {
    stats_.reset();

    auto loops = detect_loops(func);
    stats_.loops_analyzed = loops.size();

    bool changed = false;

    for (auto& loop : loops) {
        if (should_tile(loop)) {
            if (apply_tiling(func, loop)) {
                stats_.tiles_applied++;
                changed = true;
            }
        }
    }

    return changed;
}

auto LoopTilingPass::should_tile(const LoopInfo& loop) const -> bool {
    // Tile if:
    // 1. Loop has constant bounds
    // 2. Trip count is large enough to benefit from tiling
    // 3. Loop accesses arrays in a pattern that benefits from tiling

    if (!has_constant_bounds(loop)) {
        return false;
    }

    // Check if trip count is worth tiling (at least 2x tile size)
    if (loop.end.has_value() && loop.start.has_value()) {
        int64_t trip_count = *loop.end - *loop.start;
        if (trip_count < static_cast<int64_t>(tile_size_ * 2)) {
            return false;
        }
    }

    return true;
}

auto LoopTilingPass::apply_tiling(Function& func, LoopInfo& loop) -> bool {
    // Placeholder for tiling transformation
    // Would need to:
    // 1. Create outer tile loop
    // 2. Modify inner loop bounds to use tile bounds
    // 3. Update array accesses
    (void)func;
    (void)loop;
    return false;
}

// ============================================================================
// LoopFusionPass
// ============================================================================

auto LoopFusionPass::run_on_function(Function& func) -> bool {
    stats_.reset();

    auto loops = detect_loops(func);
    stats_.loops_analyzed = loops.size();

    bool changed = false;

    // Look for adjacent loops that can be fused
    for (size_t i = 0; i + 1 < loops.size(); i++) {
        auto& loop1 = loops[i];
        auto& loop2 = loops[i + 1];

        // Check if loops are adjacent (loop2 immediately follows loop1)
        if (loop2.header_block == loop1.latch_block + 1) {
            if (can_fuse(loop1, loop2)) {
                if (do_fusion(func, loop1, loop2)) {
                    stats_.fusions_applied++;
                    changed = true;
                }
            }
        }
    }

    return changed;
}

auto LoopFusionPass::can_fuse(const LoopInfo& loop1, const LoopInfo& loop2) const -> bool {
    // Requirements for fusion:
    // 1. Same bounds (start, end, step)
    // 2. No fusion-preventing dependencies
    // 3. Both at same nesting level

    return have_same_bounds(loop1, loop2);
}

auto LoopFusionPass::have_same_bounds(const LoopInfo& loop1, const LoopInfo& loop2) const -> bool {
    if (!has_constant_bounds(loop1) || !has_constant_bounds(loop2)) {
        return false;
    }

    return loop1.start == loop2.start && loop1.end == loop2.end && loop1.step == loop2.step;
}

auto LoopFusionPass::has_fusion_preventing_deps(const Function& func, const LoopInfo& loop1,
                                                const LoopInfo& loop2) const -> bool {
    // Check if loop2 depends on results computed in all iterations of loop1
    // (true dependence across loop bodies)
    (void)func;
    (void)loop1;
    (void)loop2;
    return false;
}

auto LoopFusionPass::do_fusion(Function& func, LoopInfo& loop1, LoopInfo& loop2) -> bool {
    // Placeholder for fusion transformation
    // Would need to:
    // 1. Merge loop bodies into single loop
    // 2. Remove second loop header/latch
    // 3. Update CFG
    (void)func;
    (void)loop1;
    (void)loop2;
    return false;
}

// ============================================================================
// LoopDistributionPass
// ============================================================================

auto LoopDistributionPass::run_on_function(Function& func) -> bool {
    stats_.reset();

    auto loops = detect_loops(func);
    stats_.loops_analyzed = loops.size();

    bool changed = false;

    for (auto& loop : loops) {
        auto groups = find_independent_groups(func, loop);

        if (groups.size() > 1 && should_distribute(loop, groups)) {
            if (do_distribution(func, loop, groups)) {
                stats_.distributions_applied++;
                changed = true;
            }
        }
    }

    return changed;
}

auto LoopDistributionPass::find_independent_groups(const Function& func, const LoopInfo& loop)
    -> std::vector<std::vector<size_t>> {
    // Find groups of statements that don't depend on each other
    // For now, return empty (no distribution)
    (void)func;
    (void)loop;
    return {};
}

auto LoopDistributionPass::should_distribute(const LoopInfo& loop,
                                             const std::vector<std::vector<size_t>>& groups) const
    -> bool {
    // Distribution is beneficial if:
    // 1. One group can be vectorized while others can't
    // 2. Groups have different memory access patterns
    // 3. Groups have different parallelization opportunities
    (void)loop;
    return groups.size() > 1;
}

auto LoopDistributionPass::do_distribution(Function& func, LoopInfo& loop,
                                           const std::vector<std::vector<size_t>>& groups) -> bool {
    // Placeholder for distribution transformation
    (void)func;
    (void)loop;
    (void)groups;
    return false;
}

// ============================================================================
// AdvancedLoopOptPass - Combined Pass
// ============================================================================

auto AdvancedLoopOptPass::run_on_function(Function& func) -> bool {
    stats_.reset();
    loops_.clear();

    // Analyze all loops
    analyze_loops(func);
    stats_.loops_analyzed = loops_.size();

    bool changed = false;

    // Apply optimizations to each loop, starting from innermost
    // Sort by depth (deepest first)
    std::vector<LoopInfo*> sorted_loops;
    for (auto& loop : loops_) {
        sorted_loops.push_back(&loop);
    }
    std::sort(sorted_loops.begin(), sorted_loops.end(),
              [](const LoopInfo* a, const LoopInfo* b) { return a->depth > b->depth; });

    for (auto* loop : sorted_loops) {
        if (optimize_loop(func, *loop)) {
            changed = true;
        }
    }

    return changed;
}

void AdvancedLoopOptPass::analyze_loops(const Function& func) {
    loops_ = detect_loops(func);
    build_loop_tree();
}

void AdvancedLoopOptPass::build_loop_tree() {
    // Build nesting relationship
    for (size_t i = 0; i < loops_.size(); i++) {
        for (size_t j = 0; j < loops_.size(); j++) {
            if (i != j) {
                if (loops_[i].body_blocks.count(loops_[j].header_block) > 0) {
                    loops_[j].parent = &loops_[i];
                    loops_[j].depth = loops_[i].depth + 1;
                    loops_[i].children.push_back(&loops_[j]);
                }
            }
        }
    }
}

auto AdvancedLoopOptPass::optimize_loop(Function& func, LoopInfo& loop) -> bool {
    bool changed = false;

    // Try various optimizations in order of profitability

    // 1. Check for tiling opportunity
    if (loop.start.has_value() && loop.end.has_value()) {
        // Tiling is beneficial for loops with large trip counts
        int64_t trip_count = *loop.end - *loop.start;
        if (trip_count >= 64) {
            // Would apply tiling transformation here
            // For now, just mark as potential candidate
            // stats_.tiles_applied++; // Placeholder
            (void)trip_count;
        }
    }

    // 2. Check for interchange opportunity with parent
    if (loop.parent) {
        // Interchange is beneficial when it improves cache locality
        // Would check array access patterns and apply interchange
        // For now, placeholder
        // stats_.interchanges_applied++; // Placeholder
    }

    (void)func; // Will be used in full transformation
    return changed;
}

} // namespace tml::mir
