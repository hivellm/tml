//! # Bounds Check Elimination Pass - Implementation
//!
//! This pass eliminates redundant bounds checks through value range analysis
//! and loop bounds inference.

#include "mir/passes/bounds_check_elimination.hpp"

#include <algorithm>
#include <queue>

namespace tml::mir {

// ============================================================================
// Main Pass Entry Point
// ============================================================================

auto BoundsCheckEliminationPass::run_on_function(Function& func) -> bool {
    // Reset state for this function
    stats_.reset();
    value_ranges_.clear();
    array_sizes_.clear();
    loops_.clear();
    accesses_.clear();

    // Phase 1: Analysis
    analyze_function(func);

    // Phase 2: Mark safe accesses
    mark_safe_accesses();

    // Phase 3: Apply eliminations
    return apply_eliminations(func);
}

// ============================================================================
// Analysis Phase
// ============================================================================

void BoundsCheckEliminationPass::analyze_function(const Function& func) {
    // Detect loops first (we need loop info for induction variables)
    detect_loops(func);

    // Compute value ranges
    compute_value_ranges(func);

    // Find all array accesses
    find_array_accesses(func);
}

void BoundsCheckEliminationPass::compute_value_ranges(const Function& func) {
    // First pass: collect constants and array sizes
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            // Handle constant instructions
            if (auto* constant = std::get_if<ConstantInst>(&inst.inst)) {
                value_ranges_[inst.result] = compute_constant_range(*constant);
            }
            // Handle array literals - track their size
            else if (auto* arr_init = std::get_if<ArrayInitInst>(&inst.inst)) {
                array_sizes_[inst.result] = static_cast<int64_t>(arr_init->elements.size());
                // The array value itself has a bounded range (its address)
                value_ranges_[inst.result] = ValueRange::unbounded();
            }
            // Handle alloca for arrays
            else if (auto* alloca_inst = std::get_if<AllocaInst>(&inst.inst)) {
                if (auto* arr_type = std::get_if<MirArrayType>(&alloca_inst->alloc_type->kind)) {
                    array_sizes_[inst.result] = static_cast<int64_t>(arr_type->size);
                }
            }
        }
    }

    // Second pass: propagate ranges through binary operations
    // Use a worklist algorithm for dataflow analysis
    bool changed = true;
    int iterations = 0;
    const int max_iterations = 100; // Prevent infinite loops

    while (changed && iterations < max_iterations) {
        changed = false;
        iterations++;

        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                ValueRange new_range;
                bool has_range = false;

                // Binary operations
                if (auto* bin = std::get_if<BinaryInst>(&inst.inst)) {
                    auto left_range = get_range(bin->left.id);
                    auto right_range = get_range(bin->right.id);
                    new_range = compute_binary_range(bin->op, left_range, right_range);
                    has_range = true;
                }
                // PHI nodes - join of all incoming ranges
                else if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                    new_range = compute_phi_range(*phi);
                    has_range = true;
                }
                // Cast instructions - preserve range for integer casts
                else if (auto* cast = std::get_if<CastInst>(&inst.inst)) {
                    // For now, just propagate the source range
                    new_range = get_range(cast->operand.id);
                    has_range = true;
                }

                if (has_range) {
                    auto it = value_ranges_.find(inst.result);
                    if (it == value_ranges_.end()) {
                        value_ranges_[inst.result] = new_range;
                        changed = true;
                    } else if (it->second.min != new_range.min || it->second.max != new_range.max) {
                        // Widen the range (for convergence)
                        it->second = it->second.union_with(new_range);
                        changed = true;
                    }
                }
            }
        }
    }
}

void BoundsCheckEliminationPass::detect_loops(const Function& func) {
    // Simple loop detection: look for PHI nodes at block headers
    // that have back-edges from later blocks

    for (size_t block_idx = 0; block_idx < func.blocks.size(); block_idx++) {
        const auto& block = func.blocks[block_idx];

        for (const auto& inst : block.instructions) {
            if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                // Check if this looks like a loop induction variable:
                // - One incoming value is a constant (start value)
                // - One incoming value is a binary add/sub of the PHI result (increment)

                std::optional<int64_t> start_value;
                std::optional<int64_t> step;
                uint32_t latch_block = 0;

                for (const auto& [value, pred_block] : phi->incoming) {
                    // Check for constant start value
                    auto range = get_range(value.id);
                    if (range.is_constant()) {
                        // This could be the initial value
                        if (!start_value) {
                            start_value = range.min;
                        }
                    }

                    // Check for increment pattern: we'd need to trace back
                    // For now, use a simple heuristic: if the predecessor
                    // block is after the current block, it's likely a back-edge
                    if (pred_block > static_cast<uint32_t>(block_idx)) {
                        latch_block = pred_block;
                    }
                }

                // For now, if we found what looks like an induction variable
                // with a known start, record it with a default step of 1
                if (start_value && latch_block > 0) {
                    LoopBoundsInfo loop_info;
                    loop_info.induction_var = inst.result;
                    loop_info.start_value = *start_value;
                    loop_info.step = 1; // Default assumption
                    loop_info.is_inclusive = false;
                    loop_info.end_value = std::numeric_limits<int64_t>::max();

                    // Mark blocks in the loop (simple: all blocks between header and latch)
                    for (uint32_t i = static_cast<uint32_t>(block_idx); i <= latch_block; i++) {
                        loop_info.loop_blocks.insert(i);
                    }

                    loops_.push_back(std::move(loop_info));

                    // Update the range for the induction variable
                    // If we know start and step, the range is [start, infinity) for now
                    if (loop_info.start_value >= 0) {
                        value_ranges_[inst.result] =
                            ValueRange::non_negative(loop_info.end_value - 1);
                    }
                }
            }
        }
    }
}

void BoundsCheckEliminationPass::find_array_accesses(const Function& func) {
    for (size_t block_idx = 0; block_idx < func.blocks.size(); block_idx++) {
        const auto& block = func.blocks[block_idx];

        for (size_t inst_idx = 0; inst_idx < block.instructions.size(); inst_idx++) {
            const auto& inst = block.instructions[inst_idx];

            // Look for GetElementPtr instructions - these represent array indexing
            if (auto* gep = std::get_if<GetElementPtrInst>(&inst.inst)) {
                if (!gep->indices.empty()) {
                    ArrayAccess access;
                    access.array_value = gep->base.id;
                    access.index_value = gep->indices[0].id;
                    access.block_id = static_cast<uint32_t>(block_idx);
                    access.inst_index = inst_idx;
                    access.can_eliminate = false;

                    // Try to get the array size
                    auto size_opt = get_array_size(gep->base.id);
                    access.array_size = size_opt.value_or(-1);

                    accesses_.push_back(access);
                    stats_.total_accesses++;
                }
            }
        }
    }
}

// ============================================================================
// Range Computation Helpers
// ============================================================================

auto BoundsCheckEliminationPass::get_range(ValueId id) const -> ValueRange {
    auto it = value_ranges_.find(id);
    if (it != value_ranges_.end()) {
        return it->second;
    }
    return ValueRange::unbounded();
}

auto BoundsCheckEliminationPass::get_array_size(ValueId id) const -> std::optional<int64_t> {
    auto it = array_sizes_.find(id);
    if (it != array_sizes_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto BoundsCheckEliminationPass::get_induction_bounds(ValueId id) const
    -> std::optional<ValueRange> {
    for (const auto& loop : loops_) {
        if (loop.induction_var == id) {
            if (loop.end_value != std::numeric_limits<int64_t>::max()) {
                return ValueRange{loop.start_value, loop.end_value - (loop.is_inclusive ? 0 : 1)};
            }
            // If we only know the start, return a partial range
            if (loop.start_value >= 0) {
                return ValueRange::non_negative();
            }
        }
    }
    return std::nullopt;
}

auto BoundsCheckEliminationPass::compute_constant_range(const ConstantInst& inst) const
    -> ValueRange {
    return std::visit(
        [](const auto& c) -> ValueRange {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, int64_t>) {
                return ValueRange::constant(c);
            } else if constexpr (std::is_same_v<T, int32_t>) {
                return ValueRange::constant(static_cast<int64_t>(c));
            } else if constexpr (std::is_same_v<T, bool>) {
                return ValueRange::constant(c ? 1 : 0);
            } else {
                return ValueRange::unbounded();
            }
        },
        inst.value);
}

auto BoundsCheckEliminationPass::compute_binary_range(BinOp op, const ValueRange& left,
                                                      const ValueRange& right) const -> ValueRange {
    // Handle arithmetic operations on bounded ranges
    switch (op) {
    case BinOp::Add: {
        // [a, b] + [c, d] = [a+c, b+d]
        if (left.is_bounded() && right.is_bounded()) {
            // Check for overflow
            int64_t new_min = left.min + right.min;
            int64_t new_max = left.max + right.max;
            // Simple overflow check
            if (new_min >= left.min && new_max >= left.max) {
                return ValueRange{new_min, new_max};
            }
        }
        // If one side is constant, we can still track relative bounds
        if (right.is_constant() && left.is_non_negative()) {
            if (right.min >= 0) {
                return ValueRange::non_negative();
            }
        }
        return ValueRange::unbounded();
    }

    case BinOp::Sub: {
        // [a, b] - [c, d] = [a-d, b-c]
        if (left.is_bounded() && right.is_bounded()) {
            return ValueRange{left.min - right.max, left.max - right.min};
        }
        return ValueRange::unbounded();
    }

    case BinOp::Mul: {
        // Multiplication of non-negative ranges
        if (left.is_non_negative() && right.is_non_negative()) {
            if (left.is_bounded() && right.is_bounded()) {
                return ValueRange{left.min * right.min, left.max * right.max};
            }
            return ValueRange::non_negative();
        }
        return ValueRange::unbounded();
    }

    case BinOp::Div: {
        // Division - preserve non-negative property
        if (left.is_non_negative() && right.is_constant() && right.min > 0) {
            return ValueRange{left.min / right.min, left.max / right.min};
        }
        return ValueRange::unbounded();
    }

    case BinOp::Mod: {
        // Modulo with positive divisor: result is in [0, divisor-1]
        if (right.is_constant() && right.min > 0) {
            return ValueRange{0, right.min - 1};
        }
        return ValueRange::unbounded();
    }

    case BinOp::BitAnd: {
        // Bitwise AND with positive constant gives bounded result
        if (right.is_constant() && right.min >= 0) {
            return ValueRange{0, right.min};
        }
        return ValueRange::unbounded();
    }

    default:
        return ValueRange::unbounded();
    }
}

auto BoundsCheckEliminationPass::compute_phi_range(const PhiInst& phi) const -> ValueRange {
    if (phi.incoming.empty()) {
        return ValueRange::unbounded();
    }

    // Start with the first incoming value's range
    auto result = get_range(phi.incoming[0].first.id);

    // Union with all other incoming ranges
    for (size_t i = 1; i < phi.incoming.size(); i++) {
        result = result.union_with(get_range(phi.incoming[i].first.id));
    }

    return result;
}

// ============================================================================
// Safe Access Marking
// ============================================================================

void BoundsCheckEliminationPass::mark_safe_accesses() {
    for (auto& access : accesses_) {
        // Skip if we don't know the array size
        if (access.array_size < 0) {
            continue;
        }

        // Get the index range
        auto index_range = get_range(access.index_value);

        // Case 1: Constant index within bounds
        if (index_range.is_constant()) {
            if (index_range.min >= 0 && index_range.min < access.array_size) {
                access.can_eliminate = true;
                stats_.constant_index++;
                stats_.eliminated_checks++;
                continue;
            }
        }

        // Case 2: Index range is fully bounded and within array bounds
        if (index_range.is_valid_index_for(access.array_size)) {
            access.can_eliminate = true;
            stats_.range_analysis++;
            stats_.eliminated_checks++;
            continue;
        }

        // Case 3: Check if index is a loop induction variable with known bounds
        auto induction_bounds = get_induction_bounds(access.index_value);
        if (induction_bounds && induction_bounds->is_valid_index_for(access.array_size)) {
            access.can_eliminate = true;
            stats_.loop_bounded++;
            stats_.eliminated_checks++;
            continue;
        }

        // Case 4: Non-negative index with proven upper bound
        // (e.g., from modulo operation or comparison)
        if (index_range.is_non_negative() && index_range.max < access.array_size) {
            access.can_eliminate = true;
            stats_.range_analysis++;
            stats_.eliminated_checks++;
            continue;
        }
    }
}

// ============================================================================
// Transformation Phase
// ============================================================================

auto BoundsCheckEliminationPass::apply_eliminations(Function& func) -> bool {
    (void)func; // Will be used for actual transformation in full implementation
    bool changed = false;

    // For now, we mark the accesses as safe by adding metadata
    // The actual bounds check removal happens in codegen
    // We'll add a flag to the GetElementPtrInst or emit a marker

    for (const auto& access : accesses_) {
        if (access.can_eliminate) {
            // In a full implementation, we would:
            // 1. Find the bounds check branch in codegen
            // 2. Replace it with an unconditional branch to the success path
            // 3. Or mark the GEP with a "no-bounds-check" flag

            // For now, just track that we can eliminate
            changed = true;
        }
    }

    return changed;
}

} // namespace tml::mir
