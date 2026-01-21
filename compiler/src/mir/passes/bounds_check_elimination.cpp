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
    // First, collect constants and array sizes (needed for loop bound detection)
    collect_constants(func);

    // Detect loops (uses constant ranges to find loop bounds)
    detect_loops(func);

    // Compute remaining value ranges (uses loop info for PHI nodes)
    compute_value_ranges(func);

    // Find all array accesses
    find_array_accesses(func);
}

void BoundsCheckEliminationPass::collect_constants(const Function& func) {
    // Collect constants and array sizes before loop detection
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            // Handle constant instructions
            if (auto* constant = std::get_if<ConstantInst>(&inst.inst)) {
                value_ranges_[inst.result] = compute_constant_range(*constant);
            }
            // Handle array literals - track their size
            else if (auto* arr_init = std::get_if<ArrayInitInst>(&inst.inst)) {
                array_sizes_[inst.result] = static_cast<int64_t>(arr_init->elements.size());
            }
            // Handle alloca for arrays
            else if (auto* alloca_inst = std::get_if<AllocaInst>(&inst.inst)) {
                if (auto* arr_type = std::get_if<MirArrayType>(&alloca_inst->alloc_type->kind)) {
                    array_sizes_[inst.result] = static_cast<int64_t>(arr_type->size);
                }
            }
            // Handle simple binary ops on constants (e.g., add i32 0, 100 -> 100)
            else if (auto* bin = std::get_if<BinaryInst>(&inst.inst)) {
                auto left_range = get_range(bin->left.id);
                auto right_range = get_range(bin->right.id);
                if (left_range.is_constant() && right_range.is_constant()) {
                    auto new_range = compute_binary_range(bin->op, left_range, right_range);
                    if (new_range.is_constant()) {
                        value_ranges_[inst.result] = new_range;
                    }
                }
            }
        }
    }
}

void BoundsCheckEliminationPass::compute_value_ranges(const Function& func) {
    // Note: constants and array sizes are already collected by collect_constants()
    // This function propagates ranges through operations and uses loop info for PHIs

    // Propagate ranges through binary operations
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

    // Build a map from value ID to instruction for comparison analysis
    std::unordered_map<ValueId, const InstructionData*> value_to_inst;
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result != INVALID_VALUE) {
                value_to_inst[inst.result] = &inst;
            }
        }
    }

    for (size_t block_idx = 0; block_idx < func.blocks.size(); block_idx++) {
        const auto& block = func.blocks[block_idx];

        for (const auto& inst : block.instructions) {
            if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                // Check if this looks like a loop induction variable:
                // - One incoming value is a constant (start value)
                // - One incoming value is a binary add/sub of the PHI result (increment)

                std::optional<int64_t> start_value;
                uint32_t latch_block = 0;

                for (const auto& [value, pred_block] : phi->incoming) {
                    // Check for constant start value
                    auto range = get_range(value.id);
                    if (range.is_constant()) {
                        if (!start_value) {
                            start_value = range.min;
                        }
                    }

                    // If the predecessor block is after the current block, it's likely a back-edge
                    if (pred_block > static_cast<uint32_t>(block_idx)) {
                        latch_block = pred_block;
                    }
                }

                if (start_value && latch_block > 0) {
                    LoopBoundsInfo loop_info;
                    loop_info.induction_var = inst.result;
                    loop_info.start_value = *start_value;
                    loop_info.step = 1;
                    loop_info.is_inclusive = false;
                    loop_info.end_value = std::numeric_limits<int64_t>::max();

                    // Mark blocks in the loop
                    for (uint32_t i = static_cast<uint32_t>(block_idx); i <= latch_block; i++) {
                        loop_info.loop_blocks.insert(i);
                    }

                    // Try to find the loop bound by analyzing conditional branches
                    // Look for pattern: if (iv >= N) break; or if (iv < N) continue;
                    for (uint32_t loop_block : loop_info.loop_blocks) {
                        if (loop_block >= func.blocks.size())
                            continue;
                        const auto& lb = func.blocks[loop_block];
                        if (!lb.terminator)
                            continue;

                        if (auto* cond_br = std::get_if<CondBranchTerm>(&*lb.terminator)) {
                            // Find the comparison that produces the condition
                            auto cond_it = value_to_inst.find(cond_br->condition.id);
                            if (cond_it == value_to_inst.end())
                                continue;

                            if (auto* cmp = std::get_if<BinaryInst>(&cond_it->second->inst)) {
                                // Check for comparison operators
                                bool is_lt = (cmp->op == BinOp::Lt);
                                bool is_ge = (cmp->op == BinOp::Ge);
                                bool is_le = (cmp->op == BinOp::Le);
                                bool is_gt = (cmp->op == BinOp::Gt);

                                if (is_lt || is_ge || is_le || is_gt) {
                                    // Check if comparing the induction variable
                                    bool iv_on_left = (cmp->left.id == inst.result);
                                    bool iv_on_right = (cmp->right.id == inst.result);

                                    if (iv_on_left || iv_on_right) {
                                        ValueId bound_id =
                                            iv_on_left ? cmp->right.id : cmp->left.id;
                                        auto bound_range = get_range(bound_id);

                                        // Also check array_sizes_ for bounds
                                        auto arr_it = array_sizes_.find(bound_id);
                                        if (arr_it != array_sizes_.end()) {
                                            bound_range = ValueRange::constant(arr_it->second);
                                        }

                                        if (bound_range.is_constant()) {
                                            int64_t bound = bound_range.min;
                                            // Adjust for operator type and which side IV is on
                                            // iv < N -> range is [start, N-1]
                                            // iv >= N (break) -> range is [start, N-1]
                                            // iv <= N -> range is [start, N]
                                            if ((iv_on_left && is_lt) || (iv_on_right && is_gt)) {
                                                loop_info.end_value = bound;
                                                loop_info.is_inclusive = false;
                                            } else if ((iv_on_left && is_ge) ||
                                                       (iv_on_right && is_le)) {
                                                // Break condition: iv >= N means loop runs while iv
                                                // < N
                                                loop_info.end_value = bound;
                                                loop_info.is_inclusive = false;
                                            } else if ((iv_on_left && is_le) ||
                                                       (iv_on_right && is_ge)) {
                                                loop_info.end_value = bound + 1;
                                                loop_info.is_inclusive = true;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    loops_.push_back(loop_info);

                    // Update the range for the induction variable
                    if (loop_info.start_value >= 0 &&
                        loop_info.end_value < std::numeric_limits<int64_t>::max()) {
                        int64_t max_val =
                            loop_info.is_inclusive ? loop_info.end_value : loop_info.end_value - 1;
                        value_ranges_[inst.result] = ValueRange{loop_info.start_value, max_val};
                    } else if (loop_info.start_value >= 0) {
                        value_ranges_[inst.result] = ValueRange::non_negative();
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

                    // Use known_array_size from GEP if available, otherwise try lookup
                    if (gep->known_array_size >= 0) {
                        access.array_size = gep->known_array_size;
                    } else {
                        auto size_opt = get_array_size(gep->base.id);
                        access.array_size = size_opt.value_or(-1);
                    }

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
            if constexpr (std::is_same_v<T, ConstInt>) {
                return ValueRange::constant(c.value);
            } else if constexpr (std::is_same_v<T, ConstBool>) {
                return ValueRange::constant(c.value ? 1 : 0);
            } else {
                // ConstFloat, ConstString, ConstUnit - not useful for bounds analysis
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
    bool changed = false;

    // Mark safe accesses by setting needs_bounds_check = false on GEP instructions
    for (const auto& access : accesses_) {
        if (access.can_eliminate) {
            // Get the block and instruction
            if (access.block_id < func.blocks.size()) {
                auto& block = func.blocks[access.block_id];
                if (access.inst_index < block.instructions.size()) {
                    auto& inst = block.instructions[access.inst_index];
                    if (auto* gep = std::get_if<GetElementPtrInst>(&inst.inst)) {
                        // Mark as not needing bounds check
                        gep->needs_bounds_check = false;
                        // Propagate array size for @llvm.assume generation in codegen
                        if (access.array_size >= 0) {
                            gep->known_array_size = access.array_size;
                        }
                        changed = true;
                    }
                }
            }
        }
    }

    return changed;
}

} // namespace tml::mir
