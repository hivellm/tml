//! # SIMD Vectorization Pass - Implementation
//!
//! Implements loop vectorization, SLP vectorization, and reduction handling.

#include "mir/passes/vectorization.hpp"

#include <algorithm>
#include <cmath>
#include <queue>

namespace tml::mir {

// ============================================================================
// Helper Functions Implementation
// ============================================================================

auto get_llvm_vector_type(VectorElementType elem, size_t width) -> std::string {
    std::string elem_str;
    switch (elem) {
    case VectorElementType::I8:
        elem_str = "i8";
        break;
    case VectorElementType::I16:
        elem_str = "i16";
        break;
    case VectorElementType::I32:
        elem_str = "i32";
        break;
    case VectorElementType::I64:
        elem_str = "i64";
        break;
    case VectorElementType::F32:
        elem_str = "float";
        break;
    case VectorElementType::F64:
        elem_str = "double";
        break;
    }
    return "<" + std::to_string(width) + " x " + elem_str + ">";
}

auto mir_type_to_vector_element(MirTypePtr type) -> std::optional<VectorElementType> {
    if (!type || !type->is_primitive()) {
        return std::nullopt;
    }

    auto* prim = std::get_if<MirPrimitiveType>(&type->kind);
    if (!prim)
        return std::nullopt;

    switch (prim->kind) {
    case PrimitiveType::I8:
        return VectorElementType::I8;
    case PrimitiveType::I16:
        return VectorElementType::I16;
    case PrimitiveType::I32:
        return VectorElementType::I32;
    case PrimitiveType::I64:
        return VectorElementType::I64;
    case PrimitiveType::F32:
        return VectorElementType::F32;
    case PrimitiveType::F64:
        return VectorElementType::F64;
    default:
        return std::nullopt;
    }
}

auto vector_element_size(VectorElementType elem) -> size_t {
    switch (elem) {
    case VectorElementType::I8:
        return 1;
    case VectorElementType::I16:
        return 2;
    case VectorElementType::I32:
        return 4;
    case VectorElementType::I64:
        return 8;
    case VectorElementType::F32:
        return 4;
    case VectorElementType::F64:
        return 8;
    }
    return 0;
}

auto is_vectorizable_binop(BinOp op) -> bool {
    switch (op) {
    case BinOp::Add:
    case BinOp::Sub:
    case BinOp::Mul:
    case BinOp::Div:
    case BinOp::Mod:
    case BinOp::BitAnd:
    case BinOp::BitOr:
    case BinOp::BitXor:
    case BinOp::Shl:
    case BinOp::Shr:
        return true;
    default:
        return false;
    }
}

auto binop_to_reduction(BinOp op) -> std::optional<ReductionOp> {
    switch (op) {
    case BinOp::Add:
        return ReductionOp::Add;
    case BinOp::Mul:
        return ReductionOp::Mul;
    case BinOp::BitAnd:
        return ReductionOp::And;
    case BinOp::BitOr:
        return ReductionOp::Or;
    case BinOp::BitXor:
        return ReductionOp::Xor;
    default:
        return std::nullopt;
    }
}

// ============================================================================
// Memory Dependence Analysis Implementation
// ============================================================================

void MemoryDependenceAnalysis::analyze_loop(const Function& func, const LoopInfo& loop) {
    clear();

    // Collect all memory accesses in the loop
    for (uint32_t block_id : loop.body_blocks) {
        // Find block by ID
        for (size_t bi = 0; bi < func.blocks.size(); ++bi) {
            if (func.blocks[bi].id != block_id)
                continue;

            const auto& block = func.blocks[bi];
            for (size_t i = 0; i < block.instructions.size(); ++i) {
                const auto& inst = block.instructions[i];

                if (auto* load = std::get_if<LoadInst>(&inst.inst)) {
                    MemoryAccess access;
                    access.ptr = load->ptr.id;
                    access.index = INVALID_VALUE; // Will be analyzed separately
                    access.is_read = true;
                    access.inst_index = i;
                    access.block_id = block_id;
                    accesses_.push_back(access);
                } else if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
                    MemoryAccess access;
                    access.ptr = store->ptr.id;
                    access.index = INVALID_VALUE;
                    access.is_read = false;
                    access.inst_index = i;
                    access.block_id = block_id;
                    accesses_.push_back(access);
                }
            }
            break;
        }
    }

    // Analyze dependences between all pairs of accesses
    for (size_t i = 0; i < accesses_.size(); ++i) {
        for (size_t j = i + 1; j < accesses_.size(); ++j) {
            auto dep = get_dependence(accesses_[i], accesses_[j]);
            if (dep.type != DependenceType::None) {
                dependences_.emplace_back(j, dep);
            }
        }
    }
}

auto MemoryDependenceAnalysis::get_dependence(const MemoryAccess& a, const MemoryAccess& b) const
    -> DependenceDistance {
    DependenceDistance result;

    // If both are reads, no dependence
    if (a.is_read && b.is_read) {
        result.type = DependenceType::None;
        return result;
    }

    // Check if pointers may alias
    if (!may_alias(a.ptr, b.ptr)) {
        result.type = DependenceType::None;
        return result;
    }

    // Determine dependence type
    if (!a.is_read && b.is_read) {
        result.type = DependenceType::True; // RAW
    } else if (a.is_read && !b.is_read) {
        result.type = DependenceType::Anti; // WAR
    } else {
        result.type = DependenceType::Output; // WAW
    }

    // For now, conservatively assume loop-carried with unknown distance
    result.loop_carried = true;
    result.distance = std::nullopt;

    return result;
}

auto MemoryDependenceAnalysis::can_vectorize(size_t vector_width) const -> bool {
    for (const auto& [idx, dep] : dependences_) {
        if (dep.type == DependenceType::None)
            continue;

        // If loop-carried dependence with unknown distance, can't vectorize
        if (dep.loop_carried && !dep.distance) {
            return false;
        }

        // If distance is known and less than vector width, can't vectorize
        if (dep.distance && static_cast<size_t>(std::abs(*dep.distance)) < vector_width) {
            return false;
        }
    }
    return true;
}

auto MemoryDependenceAnalysis::may_alias(ValueId ptr1, ValueId ptr2) const -> bool {
    // Conservative: if same pointer ID, definitely alias
    if (ptr1 == ptr2)
        return true;

    // TODO: Use alias analysis pass for more precise results
    // For now, conservatively assume may alias
    return true;
}

auto MemoryDependenceAnalysis::compute_distance(const MemoryAccess& /*a*/,
                                                const MemoryAccess& /*b*/,
                                                const LoopInfo& /*loop*/) const
    -> std::optional<int64_t> {
    // TODO: Implement array subscript analysis
    // For now, return unknown
    return std::nullopt;
}

// ============================================================================
// Loop Vectorization Pass Implementation
// ============================================================================

auto LoopVectorizationPass::run_on_function(Function& func) -> bool {
    stats_.reset();
    bool changed = false;

    // Analyze loops
    analyze_loops(func);

    // Try to vectorize each loop
    for (auto& loop : loops_) {
        stats_.loops_analyzed++;

        if (can_vectorize_loop(func, loop)) {
            size_t vf = compute_vf(func, loop);
            if (vf > 1 && vectorize_loop(func, loop, vf)) {
                stats_.loops_vectorized++;
                changed = true;
            } else {
                stats_.loops_not_vectorizable++;
            }
        } else {
            stats_.loops_not_vectorizable++;
        }
    }

    return changed;
}

void LoopVectorizationPass::analyze_loops(const Function& func) {
    loops_.clear();

    // Simple loop detection: look for back edges
    // A back edge is an edge from a block to a block that dominates it
    std::unordered_map<uint32_t, size_t> block_index;
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        block_index[func.blocks[i].id] = i;
    }

    for (size_t i = 0; i < func.blocks.size(); ++i) {
        const auto& block = func.blocks[i];
        if (!block.terminator)
            continue;

        std::visit(
            [&](const auto& term) {
                using T = std::decay_t<decltype(term)>;

                auto check_back_edge = [&](uint32_t target) {
                    auto it = block_index.find(target);
                    if (it != block_index.end() && it->second <= i) {
                        // Found a back edge to block at index it->second
                        LoopInfo loop;
                        loop.header_block = target;
                        loop.latch_block = block.id;
                        loop.body_blocks.insert(target);

                        // Collect loop body blocks (simplified: all blocks between header and
                        // latch)
                        for (size_t j = it->second; j <= i; ++j) {
                            loop.body_blocks.insert(func.blocks[j].id);
                        }

                        // Try to extract loop bounds from PHI nodes
                        const auto& header = func.blocks[it->second];
                        for (const auto& inst : header.instructions) {
                            if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                                loop.induction_var = inst.result;
                                // Look for constant start value
                                for (const auto& [val, pred] : phi->incoming) {
                                    if (loop.body_blocks.find(pred) == loop.body_blocks.end()) {
                                        // This is from outside the loop (initial value)
                                        // TODO: Extract constant value if available
                                    }
                                }
                                break;
                            }
                        }

                        loops_.push_back(loop);
                    }
                };

                if constexpr (std::is_same_v<T, BranchTerm>) {
                    check_back_edge(term.target);
                } else if constexpr (std::is_same_v<T, CondBranchTerm>) {
                    check_back_edge(term.true_block);
                    check_back_edge(term.false_block);
                }
            },
            *block.terminator);
    }
}

auto LoopVectorizationPass::can_vectorize_loop(const Function& func, const LoopInfo& loop) -> bool {
    // Check trip count
    auto trip_count = get_trip_count(loop);
    if (!trip_count || *trip_count < static_cast<int64_t>(config_.min_trip_count)) {
        stats_.failed_unknown_trip++;
        return false;
    }

    // Analyze memory dependences
    mem_dep_.analyze_loop(func, loop);
    if (!mem_dep_.can_vectorize(config_.vectorization_factor)) {
        stats_.failed_memory_dep++;
        return false;
    }

    // Check that all instructions in loop body are vectorizable
    for (uint32_t block_id : loop.body_blocks) {
        for (const auto& block : func.blocks) {
            if (block.id != block_id)
                continue;

            for (const auto& inst : block.instructions) {
                if (!is_vectorizable_inst(inst)) {
                    // Check if it's a reduction - those are OK
                    auto reductions = detect_reductions(func, loop);
                    bool is_reduction = false;
                    for (const auto& red : reductions) {
                        if (inst.result == red.accumulator) {
                            is_reduction = true;
                            break;
                        }
                    }
                    if (!is_reduction) {
                        return false;
                    }
                }
            }
            break;
        }
    }

    return true;
}

auto LoopVectorizationPass::compute_vf(const Function& func, const LoopInfo& loop) const -> size_t {
    // Determine element type from loop body
    MirTypePtr elem_type = nullptr;

    for (uint32_t block_id : loop.body_blocks) {
        for (const auto& block : func.blocks) {
            if (block.id != block_id)
                continue;

            for (const auto& inst : block.instructions) {
                if (inst.type && is_vectorizable_type(inst.type)) {
                    elem_type = inst.type;
                    break;
                }
            }
            if (elem_type)
                break;
        }
        if (elem_type)
            break;
    }

    if (!elem_type) {
        return 1; // Can't determine type, no vectorization
    }

    // Compute VF based on target width and element size
    auto vec_elem = mir_type_to_vector_element(elem_type);
    if (!vec_elem)
        return 1;

    size_t elem_size = vector_element_size(*vec_elem);
    size_t target_bytes = static_cast<size_t>(config_.target_width) / 8;
    size_t vf = target_bytes / elem_size;

    // Clamp to trip count if known
    auto trip_count = get_trip_count(loop);
    if (trip_count && static_cast<size_t>(*trip_count) < vf) {
        vf = static_cast<size_t>(*trip_count);
    }

    return vf;
}

auto LoopVectorizationPass::vectorize_loop(Function& /*func*/, LoopInfo& /*loop*/, size_t vf)
    -> bool {
    // TODO: Implement actual loop vectorization transformation
    // This is a placeholder that indicates the infrastructure is in place

    // The full implementation would:
    // 1. Create a preheader for setup
    // 2. Transform scalar loads into vector loads
    // 3. Transform scalar operations into vector operations
    // 4. Transform scalar stores into vector stores
    // 5. Handle loop remainder (tail) for non-divisible trip counts
    // 6. Handle reductions with horizontal reduction at end

    stats_.vector_instructions += vf; // Placeholder
    return false;                     // Return false until fully implemented
}

auto LoopVectorizationPass::detect_reductions(const Function& func, const LoopInfo& loop)
    -> std::vector<ReductionInfo> {
    std::vector<ReductionInfo> reductions;

    // Look for PHI nodes in loop header that follow reduction pattern
    for (const auto& block : func.blocks) {
        if (block.id != loop.header_block)
            continue;

        for (const auto& inst : block.instructions) {
            if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                // Check if this PHI is a reduction accumulator
                // Pattern: phi has one incoming from outside (init) and one from inside (update)

                ValueId init_val = INVALID_VALUE;
                ValueId update_val = INVALID_VALUE;
                uint32_t init_pred = UINT32_MAX;

                for (const auto& [val, pred] : phi->incoming) {
                    if (loop.body_blocks.find(pred) == loop.body_blocks.end()) {
                        init_val = val.id;
                        init_pred = pred;
                    } else {
                        update_val = val.id;
                    }
                }
                (void)init_pred; // May be used in future for preheader identification

                if (init_val != INVALID_VALUE && update_val != INVALID_VALUE) {
                    // Find the instruction that produces update_val
                    for (uint32_t body_block_id : loop.body_blocks) {
                        for (const auto& body_block : func.blocks) {
                            if (body_block.id != body_block_id)
                                continue;

                            for (const auto& body_inst : body_block.instructions) {
                                if (body_inst.result != update_val)
                                    continue;

                                // Check if it's a binary op using the PHI result
                                if (auto* bin = std::get_if<BinaryInst>(&body_inst.inst)) {
                                    bool uses_phi = (bin->left.id == inst.result ||
                                                     bin->right.id == inst.result);
                                    if (uses_phi) {
                                        auto red_op = binop_to_reduction(bin->op);
                                        if (red_op) {
                                            ReductionInfo info;
                                            info.op = *red_op;
                                            info.accumulator = inst.result;
                                            info.init_value = init_val;
                                            info.update_value = (bin->left.id == inst.result)
                                                                    ? bin->right.id
                                                                    : bin->left.id;
                                            info.element_type = body_inst.type;
                                            reductions.push_back(info);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        break;
    }

    return reductions;
}

auto LoopVectorizationPass::vectorize_reduction(Function& /*func*/, LoopInfo& /*loop*/,
                                                const ReductionInfo& /*red*/, size_t /*vf*/)
    -> bool {
    // TODO: Implement reduction vectorization
    stats_.reductions_vectorized++;
    return false; // Placeholder
}

auto LoopVectorizationPass::gen_vector_load(Function& /*func*/, BasicBlock& /*block*/,
                                            ValueId /*ptr*/, size_t /*vf*/,
                                            MirTypePtr /*elem_type*/) -> ValueId {
    // TODO: Generate vector load instruction
    return INVALID_VALUE;
}

void LoopVectorizationPass::gen_vector_store(Function& /*func*/, BasicBlock& /*block*/,
                                             ValueId /*ptr*/, ValueId /*vec_val*/, size_t /*vf*/,
                                             MirTypePtr /*elem_type*/) {
    // TODO: Generate vector store instruction
}

auto LoopVectorizationPass::gen_vector_binop(Function& /*func*/, BasicBlock& /*block*/,
                                             BinOp /*op*/, ValueId /*lhs*/, ValueId /*rhs*/,
                                             size_t /*vf*/, MirTypePtr /*elem_type*/) -> ValueId {
    // TODO: Generate vector binary operation
    return INVALID_VALUE;
}

auto LoopVectorizationPass::gen_horizontal_reduce(Function& /*func*/, BasicBlock& /*block*/,
                                                  ReductionOp /*op*/, ValueId /*vec*/,
                                                  size_t /*vf*/, MirTypePtr /*elem_type*/)
    -> ValueId {
    // TODO: Generate horizontal reduction
    return INVALID_VALUE;
}

auto LoopVectorizationPass::is_vectorizable_inst(const InstructionData& inst) const -> bool {
    return std::visit(
        [](const auto& i) -> bool {
            using T = std::decay_t<decltype(i)>;

            if constexpr (std::is_same_v<T, BinaryInst>) {
                return is_vectorizable_binop(i.op);
            } else if constexpr (std::is_same_v<T, UnaryInst>) {
                return true; // Most unary ops are vectorizable
            } else if constexpr (std::is_same_v<T, LoadInst>) {
                return true;
            } else if constexpr (std::is_same_v<T, StoreInst>) {
                return true;
            } else if constexpr (std::is_same_v<T, CastInst>) {
                return true;
            } else if constexpr (std::is_same_v<T, ConstantInst>) {
                return true;
            } else if constexpr (std::is_same_v<T, PhiInst>) {
                return true; // PHIs for induction/reduction are OK
            } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                return true; // GEPs are needed for memory access
            } else {
                return false;
            }
        },
        inst.inst);
}

auto LoopVectorizationPass::is_vectorizable_type(MirTypePtr type) const -> bool {
    return mir_type_to_vector_element(type).has_value();
}

auto LoopVectorizationPass::get_trip_count(const LoopInfo& loop) const -> std::optional<int64_t> {
    if (loop.start && loop.end && loop.step && *loop.step != 0) {
        return (*loop.end - *loop.start) / *loop.step;
    }
    return std::nullopt;
}

// ============================================================================
// SLP Vectorization Pass Implementation
// ============================================================================

auto SLPVectorizationPass::run_on_function(Function& func) -> bool {
    stats_.reset();
    bool changed = false;

    for (auto& block : func.blocks) {
        auto groups = find_slp_groups(func, block);
        stats_.slp_groups_found += groups.size();

        for (const auto& group : groups) {
            if (vectorize_group(func, block, group)) {
                stats_.slp_groups_vectorized++;
                changed = true;
            }
        }
    }

    return changed;
}

auto SLPVectorizationPass::find_slp_groups(const Function& /*func*/, const BasicBlock& block)
    -> std::vector<SLPGroup> {
    std::vector<SLPGroup> groups;

    // Look for consecutive loads from consecutive addresses
    std::vector<const InstructionData*> consecutive_loads;

    for (size_t i = 0; i < block.instructions.size(); ++i) {
        const auto& inst = block.instructions[i];

        if (std::holds_alternative<LoadInst>(inst.inst)) {
            consecutive_loads.push_back(&inst);

            // Check if we have enough consecutive loads
            if (consecutive_loads.size() >= 4) {
                if (are_consecutive_accesses(consecutive_loads)) {
                    SLPGroup group;
                    group.insts = consecutive_loads;
                    group.vector_width = consecutive_loads.size();
                    group.element_type = consecutive_loads[0]->type;
                    group.is_load = true;
                    groups.push_back(group);
                    consecutive_loads.clear();
                }
            }
        } else {
            consecutive_loads.clear();
        }
    }

    // Look for isomorphic binary operations
    for (size_t i = 0; i + 3 < block.instructions.size(); ++i) {
        const auto& inst0 = block.instructions[i];
        if (!std::holds_alternative<BinaryInst>(inst0.inst))
            continue;

        std::vector<const InstructionData*> isomorphic;
        isomorphic.push_back(&inst0);

        for (size_t j = i + 1; j < block.instructions.size() && isomorphic.size() < 4; ++j) {
            const auto& inst = block.instructions[j];
            if (are_isomorphic(inst0, inst)) {
                isomorphic.push_back(&inst);
            }
        }

        if (isomorphic.size() >= 4) {
            SLPGroup group;
            group.insts = isomorphic;
            group.vector_width = isomorphic.size();
            group.element_type = isomorphic[0]->type;
            groups.push_back(group);
        }
    }

    return groups;
}

auto SLPVectorizationPass::are_isomorphic(const InstructionData& a, const InstructionData& b) const
    -> bool {
    // Check if same type of instruction
    if (a.inst.index() != b.inst.index())
        return false;

    // Check if same result type
    if (a.type != b.type)
        return false;

    // Check specific instruction type
    if (auto* bin_a = std::get_if<BinaryInst>(&a.inst)) {
        auto* bin_b = std::get_if<BinaryInst>(&b.inst);
        if (!bin_b)
            return false;
        return bin_a->op == bin_b->op;
    }

    if (auto* unary_a = std::get_if<UnaryInst>(&a.inst)) {
        auto* unary_b = std::get_if<UnaryInst>(&b.inst);
        if (!unary_b)
            return false;
        return unary_a->op == unary_b->op;
    }

    return false;
}

auto SLPVectorizationPass::are_consecutive_accesses(
    const std::vector<const InstructionData*>& /*loads*/) const -> bool {
    // TODO: Implement consecutive address detection
    // This requires analyzing GEP indices to determine if addresses are consecutive
    return false;
}

auto SLPVectorizationPass::vectorize_group(Function& /*func*/, BasicBlock& /*block*/,
                                           const SLPGroup& /*group*/) -> bool {
    // TODO: Implement SLP vectorization transformation
    return false;
}

// ============================================================================
// Combined Vectorization Pass Implementation
// ============================================================================

auto VectorizationPass::run(Module& module) -> bool {
    stats_.reset();
    bool changed = false;

    // Run loop vectorization first
    LoopVectorizationPass loop_vec(config_);
    if (loop_vec.run(module)) {
        changed = true;
        const auto& loop_stats = loop_vec.stats();
        stats_.loops_analyzed += loop_stats.loops_analyzed;
        stats_.loops_vectorized += loop_stats.loops_vectorized;
        stats_.loops_not_vectorizable += loop_stats.loops_not_vectorizable;
        stats_.reductions_vectorized += loop_stats.reductions_vectorized;
        stats_.vector_instructions += loop_stats.vector_instructions;
    }

    // Then run SLP vectorization
    SLPVectorizationPass slp_vec(config_);
    if (slp_vec.run(module)) {
        changed = true;
        const auto& slp_stats = slp_vec.stats();
        stats_.slp_groups_found += slp_stats.slp_groups_found;
        stats_.slp_groups_vectorized += slp_stats.slp_groups_vectorized;
    }

    return changed;
}

} // namespace tml::mir
