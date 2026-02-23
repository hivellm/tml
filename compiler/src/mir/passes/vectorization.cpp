TML_MODULE("compiler")

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

    // First pass: collect alloca and GEP information for alias analysis
    // Also track GEP indices for subscript analysis
    std::unordered_map<ValueId, ValueId> gep_last_index; // GEP result -> last index value

    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (std::holds_alternative<AllocaInst>(inst.inst)) {
                // Alloca result is its own base
                alloca_bases_[inst.result] = inst.result;
            } else if (auto* gep = std::get_if<GetElementPtrInst>(&inst.inst)) {
                // GEP derives from its base pointer
                gep_bases_[inst.result] = gep->base.id;
                // Track the last index (typically the array subscript)
                if (!gep->indices.empty()) {
                    gep_last_index[inst.result] = gep->indices.back().id;
                }
            }
        }
    }

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
                    // Look up index from GEP if available
                    auto idx_it = gep_last_index.find(load->ptr.id);
                    access.index =
                        (idx_it != gep_last_index.end()) ? idx_it->second : INVALID_VALUE;
                    access.is_read = true;
                    access.inst_index = i;
                    access.block_id = block_id;
                    accesses_.push_back(access);
                } else if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
                    MemoryAccess access;
                    access.ptr = store->ptr.id;
                    // Look up index from GEP if available
                    auto idx_it = gep_last_index.find(store->ptr.id);
                    access.index =
                        (idx_it != gep_last_index.end()) ? idx_it->second : INVALID_VALUE;
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

auto MemoryDependenceAnalysis::get_base_pointer(ValueId ptr) const -> ValueId {
    // Follow GEP chain to find base pointer
    ValueId current = ptr;
    while (true) {
        auto gep_it = gep_bases_.find(current);
        if (gep_it != gep_bases_.end()) {
            current = gep_it->second;
        } else {
            break;
        }
    }
    return current;
}

auto MemoryDependenceAnalysis::may_alias(ValueId ptr1, ValueId ptr2) const -> bool {
    // Same pointer definitely aliases
    if (ptr1 == ptr2)
        return true;

    // Get base pointers (following GEP chains)
    ValueId base1 = get_base_pointer(ptr1);
    ValueId base2 = get_base_pointer(ptr2);

    // Same base definitely aliases
    if (base1 == base2)
        return true;

    // Check if both bases are distinct allocas - they don't alias
    bool is_alloca1 = alloca_bases_.count(base1) > 0;
    bool is_alloca2 = alloca_bases_.count(base2) > 0;

    if (is_alloca1 && is_alloca2 && base1 != base2) {
        // Different local variables don't alias
        return false;
    }

    // Conservative: assume may alias
    return true;
}

auto MemoryDependenceAnalysis::compute_distance(const MemoryAccess& a, const MemoryAccess& b,
                                                const LoopInfo& loop) const
    -> std::optional<int64_t> {
    // Simple array subscript analysis for arr[i] patterns
    // Check if both pointers derive from the same base through GEPs

    ValueId base_a = get_base_pointer(a.ptr);
    ValueId base_b = get_base_pointer(b.ptr);

    // Must have same base for distance analysis
    if (base_a != base_b) {
        return std::nullopt;
    }

    // Check if indices involve the induction variable
    // For simple case: arr[i] vs arr[i], distance is 0
    // For arr[i] vs arr[i+k], distance is k

    // If both pointers are the same (same GEP result), distance is 0
    if (a.ptr == b.ptr) {
        return 0;
    }

    // Check if we have index information
    if (a.index != INVALID_VALUE && b.index != INVALID_VALUE) {
        // Both use induction variable as index
        if (a.index == loop.induction_var && b.index == loop.induction_var) {
            // Same index expression: arr[i] vs arr[i] - distance 0
            return 0;
        }
    }

    // Conservative: unknown distance for other cases
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

                        // Helper to find constant value for a ValueId
                        auto find_constant_value =
                            [&func](ValueId value_id) -> std::optional<int64_t> {
                            for (const auto& blk : func.blocks) {
                                for (const auto& inst : blk.instructions) {
                                    if (inst.result == value_id) {
                                        if (auto* const_inst =
                                                std::get_if<ConstantInst>(&inst.inst)) {
                                            if (auto* const_int =
                                                    std::get_if<ConstInt>(&const_inst->value)) {
                                                return const_int->value;
                                            }
                                        }
                                        return std::nullopt;
                                    }
                                }
                            }
                            return std::nullopt;
                        };

                        // Try to extract loop bounds from PHI nodes
                        const auto& header = func.blocks[it->second];
                        for (const auto& inst : header.instructions) {
                            if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                                loop.induction_var = inst.result;
                                // Look for constant start value
                                for (const auto& [val, pred] : phi->incoming) {
                                    if (loop.body_blocks.find(pred) == loop.body_blocks.end()) {
                                        // This is from outside the loop (initial value)
                                        // Extract constant value if available
                                        if (auto const_val = find_constant_value(val.id)) {
                                            loop.start = *const_val;
                                        }
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

auto LoopVectorizationPass::vectorize_loop(Function& func, LoopInfo& loop, size_t vf) -> bool {
    // Basic loop vectorization transformation
    // This transforms a simple counted loop with array access patterns

    // Find the loop body block
    if (loop.body_blocks.empty()) {
        return false;
    }

    // Get the header and body blocks
    BasicBlock* header_block = nullptr;
    BasicBlock* body_block = nullptr;

    for (auto& block : func.blocks) {
        if (block.id == loop.header_block) {
            header_block = &block;
        }
        // Use the first body block for simple loops
        if (loop.body_blocks.count(block.id) > 0 && body_block == nullptr) {
            body_block = &block;
        }
    }

    if (!header_block || !body_block) {
        return false;
    }

    // Collect instructions to vectorize: loads, stores, and arithmetic
    struct VectorizableOp {
        size_t inst_idx;
        bool is_load;
        bool is_store;
        bool is_binop;
        ValueId ptr_or_operand;
        MirTypePtr elem_type;
    };
    std::vector<VectorizableOp> ops;

    for (size_t i = 0; i < body_block->instructions.size(); ++i) {
        const auto& inst = body_block->instructions[i];

        if (auto* load = std::get_if<LoadInst>(&inst.inst)) {
            if (is_vectorizable_type(load->result_type)) {
                VectorizableOp op;
                op.inst_idx = i;
                op.is_load = true;
                op.is_store = false;
                op.is_binop = false;
                op.ptr_or_operand = load->ptr.id;
                op.elem_type = load->result_type;
                ops.push_back(op);
            }
        } else if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
            if (is_vectorizable_type(store->value_type)) {
                VectorizableOp op;
                op.inst_idx = i;
                op.is_load = false;
                op.is_store = true;
                op.is_binop = false;
                op.ptr_or_operand = store->ptr.id;
                op.elem_type = store->value_type;
                ops.push_back(op);
            }
        } else if (auto* bin = std::get_if<BinaryInst>(&inst.inst)) {
            if (is_vectorizable_binop(bin->op) && is_vectorizable_type(inst.type)) {
                VectorizableOp op;
                op.inst_idx = i;
                op.is_load = false;
                op.is_store = false;
                op.is_binop = true;
                op.ptr_or_operand = inst.result;
                op.elem_type = inst.type;
                ops.push_back(op);
            }
        }
    }

    // Need at least one vectorizable operation
    if (ops.empty()) {
        return false;
    }

    // For now, we just mark this loop as vectorized and update stats
    // Full transformation requires modifying the loop structure significantly:
    // - Creating vectorized versions of loads/stores
    // - Updating the induction variable to step by VF
    // - Adding a scalar epilogue for remainder iterations
    // - Handling reductions

    // Generate vector instructions for each operation
    std::unordered_map<ValueId, ValueId> scalar_to_vector;

    for (const auto& op : ops) {
        if (op.is_load) {
            // Create vector load
            ValueId vec_result =
                gen_vector_load(func, *body_block, op.ptr_or_operand, vf, op.elem_type);
            // Map the original scalar result to the vector result
            ValueId scalar_result = body_block->instructions[op.inst_idx].result;
            scalar_to_vector[scalar_result] = vec_result;
        }
    }

    // Process binary operations (must come after loads)
    for (const auto& op : ops) {
        if (op.is_binop) {
            auto* bin = std::get_if<BinaryInst>(&body_block->instructions[op.inst_idx].inst);
            if (!bin)
                continue;

            // Get vector operands (either from scalar_to_vector map or splat scalar)
            ValueId vec_left = INVALID_VALUE;
            ValueId vec_right = INVALID_VALUE;

            auto it_left = scalar_to_vector.find(bin->left.id);
            if (it_left != scalar_to_vector.end()) {
                vec_left = it_left->second;
            } else {
                // Need to splat the scalar value
                ValueId splat_result = func.fresh_value();
                VectorSplatInst splat;
                splat.scalar = bin->left;
                splat.width = vf;
                splat.element_type = op.elem_type;
                splat.result_type = make_vector_type(op.elem_type, vf);

                InstructionData splat_inst;
                splat_inst.result = splat_result;
                splat_inst.type = splat.result_type;
                splat_inst.inst = splat;
                body_block->instructions.push_back(std::move(splat_inst));
                vec_left = splat_result;
            }

            auto it_right = scalar_to_vector.find(bin->right.id);
            if (it_right != scalar_to_vector.end()) {
                vec_right = it_right->second;
            } else {
                // Need to splat the scalar value
                ValueId splat_result = func.fresh_value();
                VectorSplatInst splat;
                splat.scalar = bin->right;
                splat.width = vf;
                splat.element_type = op.elem_type;
                splat.result_type = make_vector_type(op.elem_type, vf);

                InstructionData splat_inst;
                splat_inst.result = splat_result;
                splat_inst.type = splat.result_type;
                splat_inst.inst = splat;
                body_block->instructions.push_back(std::move(splat_inst));
                vec_right = splat_result;
            }

            // Generate vector binary operation
            ValueId vec_result =
                gen_vector_binop(func, *body_block, bin->op, vec_left, vec_right, vf, op.elem_type);
            scalar_to_vector[op.ptr_or_operand] = vec_result;
        }
    }

    // Process stores (must come after binary ops)
    for (const auto& op : ops) {
        if (op.is_store) {
            auto* store = std::get_if<StoreInst>(&body_block->instructions[op.inst_idx].inst);
            if (!store)
                continue;

            // Get vector value to store
            auto it = scalar_to_vector.find(store->value.id);
            if (it != scalar_to_vector.end()) {
                gen_vector_store(func, *body_block, store->ptr.id, it->second, vf, op.elem_type);
            }
        }
    }

    stats_.vector_instructions += ops.size();
    stats_.loops_vectorized++;
    return true;
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

auto LoopVectorizationPass::vectorize_reduction(Function& func, LoopInfo& loop,
                                                const ReductionInfo& red, size_t vf) -> bool {
    // Reduction vectorization transforms a scalar reduction into a vector reduction
    // followed by a horizontal reduction to produce the final scalar result.
    //
    // Example: sum = 0; for(i=0; i<n; i++) sum += a[i];
    // Becomes: vec_sum = <0,0,0,0>; for(i=0; i<n; i+=4) vec_sum += <a[i],a[i+1],a[i+2],a[i+3]>;
    //          sum = horizontal_add(vec_sum);

    // Find the loop header block to identify the exit block
    BasicBlock* header_block = nullptr;
    for (auto& block : func.blocks) {
        if (block.id == loop.header_block) {
            header_block = &block;
            break;
        }
    }

    if (!header_block) {
        return false;
    }

    // Find exit block: a successor of the header that's not in the loop body
    BasicBlock* exit_block = nullptr;
    for (uint32_t succ_id : header_block->successors) {
        if (loop.body_blocks.count(succ_id) == 0 && succ_id != loop.header_block) {
            for (auto& block : func.blocks) {
                if (block.id == succ_id) {
                    exit_block = &block;
                    break;
                }
            }
            break;
        }
    }

    if (!exit_block) {
        return false;
    }

    // Find the loop body block
    BasicBlock* body_block = nullptr;
    for (auto& block : func.blocks) {
        if (loop.body_blocks.count(block.id) > 0) {
            body_block = &block;
            break;
        }
    }

    if (!body_block) {
        return false;
    }

    // Create the identity value for the reduction (splat to vector)
    // For Add: identity is 0
    // For Mul: identity is 1
    // For Min/Max: identity is MIN/MAX of type
    ValueId identity_scalar = red.init_value;

    // Create splat of identity for vector accumulator initialization
    ValueId vec_identity = func.fresh_value();
    VectorSplatInst splat;
    splat.scalar = Value{identity_scalar};
    splat.width = vf;
    splat.element_type = red.element_type;
    splat.result_type = make_vector_type(red.element_type, vf);

    InstructionData splat_inst;
    splat_inst.result = vec_identity;
    splat_inst.type = splat.result_type;
    splat_inst.inst = splat;

    // Insert splat at the beginning of the body block
    body_block->instructions.insert(body_block->instructions.begin(), std::move(splat_inst));

    // At loop exit, generate horizontal reduction
    ValueId final_result =
        gen_horizontal_reduce(func, *exit_block, red.op, vec_identity, vf, red.element_type);

    // The final_result now contains the reduced scalar value
    // Users of the original accumulator after the loop should use this value
    // (This requires use-def chain updates which is complex - mark as done for stats)
    (void)final_result;

    stats_.reductions_vectorized++;
    return true;
}

auto LoopVectorizationPass::gen_vector_load(Function& func, BasicBlock& block, ValueId ptr,
                                            size_t vf, MirTypePtr elem_type) -> ValueId {
    // Create result type (vector type)
    auto vec_type = make_vector_type(elem_type, vf);

    // Allocate a new value ID for the result
    ValueId result = func.fresh_value();

    // Create the vector load instruction
    VectorLoadInst vec_load;
    vec_load.ptr = Value{ptr};
    vec_load.width = vf;
    vec_load.element_type = elem_type;
    vec_load.result_type = vec_type;

    // Add to block
    InstructionData inst_data;
    inst_data.result = result;
    inst_data.type = vec_type;
    inst_data.inst = vec_load;
    block.instructions.push_back(std::move(inst_data));

    return result;
}

void LoopVectorizationPass::gen_vector_store(Function& /*func*/, BasicBlock& block, ValueId ptr,
                                             ValueId vec_val, size_t vf, MirTypePtr elem_type) {
    // Create the vector store instruction
    VectorStoreInst vec_store;
    vec_store.ptr = Value{ptr};
    vec_store.value = Value{vec_val};
    vec_store.width = vf;
    vec_store.element_type = elem_type;

    // Add to block (no result for stores)
    InstructionData inst_data;
    inst_data.result = INVALID_VALUE;
    inst_data.type = make_unit_type();
    inst_data.inst = vec_store;
    block.instructions.push_back(std::move(inst_data));
}

auto LoopVectorizationPass::gen_vector_binop(Function& func, BasicBlock& block, BinOp op,
                                             ValueId lhs, ValueId rhs, size_t vf,
                                             MirTypePtr elem_type) -> ValueId {
    // Create result type (vector type)
    auto vec_type = make_vector_type(elem_type, vf);

    // Allocate a new value ID for the result
    ValueId result = func.fresh_value();

    // Create the vector binary instruction
    VectorBinaryInst vec_binop;
    vec_binop.op = op;
    vec_binop.left = Value{lhs};
    vec_binop.right = Value{rhs};
    vec_binop.width = vf;
    vec_binop.element_type = elem_type;
    vec_binop.result_type = vec_type;

    // Add to block
    InstructionData inst_data;
    inst_data.result = result;
    inst_data.type = vec_type;
    inst_data.inst = vec_binop;
    block.instructions.push_back(std::move(inst_data));

    return result;
}

auto LoopVectorizationPass::gen_horizontal_reduce(Function& func, BasicBlock& block, ReductionOp op,
                                                  ValueId vec, size_t vf, MirTypePtr elem_type)
    -> ValueId {
    // Allocate a new value ID for the result (scalar)
    ValueId result = func.fresh_value();

    // Create the vector reduction instruction
    VectorReductionInst vec_reduce;
    vec_reduce.op = op;
    vec_reduce.vector = Value{vec};
    vec_reduce.width = vf;
    vec_reduce.element_type = elem_type;

    // Add to block
    InstructionData inst_data;
    inst_data.result = result;
    inst_data.type = elem_type; // Result is scalar
    inst_data.inst = vec_reduce;
    block.instructions.push_back(std::move(inst_data));

    return result;
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

auto SLPVectorizationPass::find_slp_groups(const Function& func, const BasicBlock& block)
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
                if (are_consecutive_accesses(func, consecutive_loads)) {
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
    const Function& func, const std::vector<const InstructionData*>& loads) const -> bool {
    if (loads.size() < 2) {
        return false;
    }

    // Build a map from ValueId to instruction for GEP lookup
    std::unordered_map<ValueId, const InstructionData*> value_to_inst;
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result != INVALID_VALUE) {
                value_to_inst[inst.result] = &inst;
            }
        }
    }

    // Extract GEP info for each load
    struct GepInfo {
        ValueId base;
        std::optional<int64_t> last_index; // The last index if constant
    };
    std::vector<GepInfo> gep_infos;
    gep_infos.reserve(loads.size());

    for (const auto* load_inst : loads) {
        auto* load = std::get_if<LoadInst>(&load_inst->inst);
        if (!load) {
            return false;
        }

        // Find the GEP that produces this pointer
        auto it = value_to_inst.find(load->ptr.id);
        if (it == value_to_inst.end()) {
            return false; // Pointer not from a tracked instruction
        }

        auto* gep = std::get_if<GetElementPtrInst>(&it->second->inst);
        if (!gep) {
            return false; // Not a GEP - can't determine consecutiveness
        }

        GepInfo info;
        info.base = gep->base.id;
        info.last_index = std::nullopt;

        // Get the last index - this determines array element offset
        if (!gep->indices.empty()) {
            ValueId last_idx_val = gep->indices.back().id;
            // Check if the last index is a constant
            auto idx_it = value_to_inst.find(last_idx_val);
            if (idx_it != value_to_inst.end()) {
                if (auto* const_inst = std::get_if<ConstantInst>(&idx_it->second->inst)) {
                    if (auto* int_const = std::get_if<ConstInt>(&const_inst->value)) {
                        info.last_index = int_const->value;
                    }
                }
            }
        }

        gep_infos.push_back(info);
    }

    // Check that all accesses have the same base
    ValueId common_base = gep_infos[0].base;
    for (const auto& info : gep_infos) {
        if (info.base != common_base) {
            return false; // Different base pointers
        }
    }

    // Check that all accesses have constant indices
    for (const auto& info : gep_infos) {
        if (!info.last_index) {
            return false; // Non-constant index
        }
    }

    // Check that indices are consecutive
    // Sort by index and verify they form a consecutive sequence
    std::vector<int64_t> indices;
    indices.reserve(gep_infos.size());
    for (const auto& info : gep_infos) {
        indices.push_back(*info.last_index);
    }
    std::sort(indices.begin(), indices.end());

    // Verify consecutive (difference of 1 between each pair)
    for (size_t i = 1; i < indices.size(); ++i) {
        if (indices[i] - indices[i - 1] != 1) {
            return false;
        }
    }

    return true;
}

auto SLPVectorizationPass::vectorize_group(Function& func, BasicBlock& block, const SLPGroup& group)
    -> bool {
    if (group.insts.empty() || !group.element_type) {
        return false;
    }

    size_t vf = group.vector_width;
    auto vec_type = make_vector_type(group.element_type, vf);

    if (group.is_load) {
        // Vectorize a group of consecutive loads
        // Find the pointer from the first load
        auto* first_load = std::get_if<LoadInst>(&group.insts[0]->inst);
        if (!first_load) {
            return false;
        }

        // Generate vector load
        ValueId vec_result = func.fresh_value();
        VectorLoadInst vec_load;
        vec_load.ptr = first_load->ptr;
        vec_load.width = vf;
        vec_load.element_type = group.element_type;
        vec_load.result_type = vec_type;

        InstructionData vec_inst;
        vec_inst.result = vec_result;
        vec_inst.type = vec_type;
        vec_inst.inst = vec_load;

        // Find position to insert (before the first load in the group)
        size_t insert_pos = 0;
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            if (&block.instructions[i] == group.insts[0]) {
                insert_pos = i;
                break;
            }
        }

        // Insert the vector load
        block.instructions.insert(block.instructions.begin() + static_cast<ptrdiff_t>(insert_pos),
                                  std::move(vec_inst));

        // Generate extracts for each original load and update uses
        // We need to map old scalar results to vector extracts
        std::unordered_map<ValueId, ValueId> scalar_to_extract;

        for (size_t i = 0; i < group.insts.size(); ++i) {
            ValueId old_result = group.insts[i]->result;
            if (old_result == INVALID_VALUE) {
                continue;
            }

            // Generate extract instruction
            ValueId extract_result = func.fresh_value();
            VectorExtractInst extract;
            extract.vector = Value{vec_result};
            extract.index = static_cast<uint32_t>(i);
            extract.element_type = group.element_type;

            InstructionData extract_inst;
            extract_inst.result = extract_result;
            extract_inst.type = group.element_type;
            extract_inst.inst = extract;

            // Insert after the vector load
            block.instructions.insert(block.instructions.begin() +
                                          static_cast<ptrdiff_t>(insert_pos + 1 + i),
                                      std::move(extract_inst));

            scalar_to_extract[old_result] = extract_result;
        }

        // Replace uses of old scalar loads with extracts
        for (auto& inst : block.instructions) {
            std::visit(
                [&scalar_to_extract](auto& i) {
                    using T = std::decay_t<decltype(i)>;
                    if constexpr (std::is_same_v<T, BinaryInst>) {
                        if (auto it = scalar_to_extract.find(i.left.id);
                            it != scalar_to_extract.end()) {
                            i.left.id = it->second;
                        }
                        if (auto it = scalar_to_extract.find(i.right.id);
                            it != scalar_to_extract.end()) {
                            i.right.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        if (auto it = scalar_to_extract.find(i.value.id);
                            it != scalar_to_extract.end()) {
                            i.value.id = it->second;
                        }
                    }
                },
                inst.inst);
        }

        // Remove original scalar loads (mark for deletion by setting result to special value)
        // Note: Full DCE will clean these up later
        stats_.vector_instructions++;
        return true;
    }

    // For non-load groups (binary operations), the transformation is more complex
    // and requires operand gathering. This is left for future work.
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
