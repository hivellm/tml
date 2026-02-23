TML_MODULE("compiler")

//! # Batch Destruction Optimization Pass Implementation
//!
//! Optimizes arrays of destructor calls into efficient loops.

#include "mir/passes/batch_destruction.hpp"

#include <algorithm>

namespace tml::mir {

auto BatchDestructionPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    for (auto& block : func.blocks) {
        // Find batches of consecutive destructor calls
        auto batches = find_destructor_batches(func, block);
        stats_.batches_found += batches.size();

        // Process batches in reverse order to maintain valid indices
        for (auto it = batches.rbegin(); it != batches.rend(); ++it) {
            const auto& batch = *it;
            stats_.calls_batched += batch.inst_indices.size();

            if (batch.is_trivial && batch.element_count >= 4) {
                // For trivial destructors with 4+ elements, vectorize
                if (vectorize_trivial(func, block, batch)) {
                    stats_.trivial_vectorized++;
                    changed = true;
                }
            } else if (batch.element_count >= 3) {
                // For 3+ non-trivial elements, replace with loop
                if (replace_with_loop(func, block, batch)) {
                    changed = true;
                }
            }
        }
    }

    return changed;
}

auto BatchDestructionPass::find_destructor_batches(const Function& func, BasicBlock& block)
    -> std::vector<DestructorBatch> {
    std::vector<DestructorBatch> batches;

    // Track consecutive drop calls to array elements
    std::optional<DestructorBatch> current_batch;

    for (size_t i = 0; i < block.instructions.size(); ++i) {
        const auto& inst = block.instructions[i];

        if (auto* call = std::get_if<CallInst>(&inst.inst)) {
            // Check if this is a drop call
            if (call->func_name.find("_drop") != std::string::npos && !call->args.empty()) {

                // Try to extract array access info
                auto access = extract_array_access(call->args[0], func);
                if (access) {
                    auto [array_ptr, index] = *access;

                    if (current_batch) {
                        // Check if this continues the current batch
                        if (current_batch->array_ptr == array_ptr &&
                            static_cast<size_t>(index) == current_batch->element_count) {
                            // Continue current batch
                            current_batch->end_idx = i + 1;
                            current_batch->element_count++;
                            current_batch->inst_indices.push_back(i);
                            continue;
                        } else {
                            // Different array or non-consecutive - save current batch
                            if (current_batch->element_count >= 2) {
                                batches.push_back(*current_batch);
                            }
                            current_batch.reset();
                        }
                    }

                    // Start new batch
                    if (index == 0) {
                        DestructorBatch batch;
                        batch.array_ptr = array_ptr;
                        // Extract element type from drop function name
                        size_t pos = call->func_name.find("_drop");
                        if (pos > 0) {
                            batch.element_type = call->func_name.substr(0, pos);
                        }
                        batch.start_idx = i;
                        batch.end_idx = i + 1;
                        batch.element_count = 1;
                        batch.is_trivial = is_trivial_destructor(batch.element_type);
                        batch.inst_indices.push_back(i);
                        current_batch = batch;
                    }
                } else if (current_batch) {
                    // Non-array drop - save current batch if valid
                    if (current_batch->element_count >= 2) {
                        batches.push_back(*current_batch);
                    }
                    current_batch.reset();
                }
            } else if (current_batch) {
                // Non-drop call interrupts batch
                if (current_batch->element_count >= 2) {
                    batches.push_back(*current_batch);
                }
                current_batch.reset();
            }
        } else if (current_batch) {
            // Non-call instruction may or may not interrupt batch
            // For safety, we end the batch on any non-drop instruction
            if (current_batch->element_count >= 2) {
                batches.push_back(*current_batch);
            }
            current_batch.reset();
        }
    }

    // Don't forget the last batch
    if (current_batch && current_batch->element_count >= 2) {
        batches.push_back(*current_batch);
    }

    return batches;
}

auto BatchDestructionPass::is_trivial_destructor(const std::string& type_name) -> bool {
    // A destructor is trivial if it only frees memory (no custom cleanup)
    auto class_def = env_.lookup_class(type_name);
    if (!class_def) {
        // If we can't find the class, assume non-trivial for safety
        return false;
    }

    // Check if the class has a custom drop method
    for (const auto& method : class_def->methods) {
        if (method.sig.name == "drop") {
            // Has explicit drop method - not trivial
            return false;
        }
    }

    // No explicit drop - considered trivial
    return true;
}

auto BatchDestructionPass::replace_with_loop(Function& func, BasicBlock& block,
                                             const DestructorBatch& batch) -> bool {
    if (batch.inst_indices.empty())
        return false;

    // Create batch drop call - passes array pointer and count
    // The runtime batch_drop function handles looping internally
    CallInst batch_drop;
    batch_drop.func_name = batch.element_type + "_batch_drop";
    batch_drop.args.push_back(Value{batch.array_ptr, nullptr});

    // Create a constant for element count
    ConstantInst count_const;
    count_const.value = ConstInt{static_cast<int64_t>(batch.element_count), false, 64};

    InstructionData count_inst;
    count_inst.result = func.fresh_value();
    count_inst.type = nullptr;
    count_inst.inst = count_const;

    batch_drop.args.push_back(Value{count_inst.result, nullptr});

    InstructionData drop_inst;
    drop_inst.result = 0; // void return
    drop_inst.type = nullptr;
    drop_inst.inst = batch_drop;

    // Remove original drop calls (in reverse order to maintain indices)
    std::vector<size_t> sorted_indices = batch.inst_indices;
    std::sort(sorted_indices.begin(), sorted_indices.end(), std::greater<size_t>());

    for (size_t idx : sorted_indices) {
        if (idx < block.instructions.size()) {
            block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(idx));
        }
    }

    // Insert batch drop at the start position
    size_t insert_pos = batch.start_idx;
    if (insert_pos > block.instructions.size()) {
        insert_pos = block.instructions.size();
    }

    block.instructions.insert(block.instructions.begin() + static_cast<std::ptrdiff_t>(insert_pos),
                              count_inst);
    block.instructions.insert(
        block.instructions.begin() + static_cast<std::ptrdiff_t>(insert_pos + 1), drop_inst);

    return true;
}

auto BatchDestructionPass::vectorize_trivial(Function& func, BasicBlock& block,
                                             const DestructorBatch& batch) -> bool {
    if (batch.inst_indices.empty())
        return false;

    // For trivial destructors (just freeing memory), we can replace
    // N free calls with a single bulk operation

    CallInst bulk_free;
    bulk_free.func_name = "batch_free_array";
    bulk_free.args.push_back(Value{batch.array_ptr, nullptr});

    // Create constant for element count
    ConstantInst count_const;
    count_const.value = ConstInt{static_cast<int64_t>(batch.element_count), false, 64};

    InstructionData count_inst;
    count_inst.result = func.fresh_value();
    count_inst.type = nullptr;
    count_inst.inst = count_const;

    bulk_free.args.push_back(Value{count_inst.result, nullptr});

    // Create constant for element size (simplified - use 8 bytes as default)
    ConstantInst size_const;
    size_const.value = ConstInt{8, false, 64};

    InstructionData size_inst;
    size_inst.result = func.fresh_value();
    size_inst.type = nullptr;
    size_inst.inst = size_const;

    bulk_free.args.push_back(Value{size_inst.result, nullptr});

    InstructionData free_inst;
    free_inst.result = 0; // void return
    free_inst.type = nullptr;
    free_inst.inst = bulk_free;

    // Remove original drop calls
    std::vector<size_t> sorted_indices = batch.inst_indices;
    std::sort(sorted_indices.begin(), sorted_indices.end(), std::greater<size_t>());

    for (size_t idx : sorted_indices) {
        if (idx < block.instructions.size()) {
            block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(idx));
        }
    }

    // Insert bulk free at the start position
    size_t insert_pos = batch.start_idx;
    if (insert_pos > block.instructions.size()) {
        insert_pos = block.instructions.size();
    }

    block.instructions.insert(block.instructions.begin() + static_cast<std::ptrdiff_t>(insert_pos),
                              count_inst);
    block.instructions.insert(
        block.instructions.begin() + static_cast<std::ptrdiff_t>(insert_pos + 1), size_inst);
    block.instructions.insert(
        block.instructions.begin() + static_cast<std::ptrdiff_t>(insert_pos + 2), free_inst);

    return true;
}

auto BatchDestructionPass::are_consecutive_array_drops(const CallInst& call1, const CallInst& call2,
                                                       const Function& func) -> bool {
    if (call1.args.empty() || call2.args.empty())
        return false;

    auto access1 = extract_array_access(call1.args[0], func);
    auto access2 = extract_array_access(call2.args[0], func);

    if (!access1 || !access2)
        return false;

    // Same array and consecutive indices
    return access1->first == access2->first && access2->second == access1->second + 1;
}

auto BatchDestructionPass::extract_array_access(const Value& ptr, const Function& func)
    -> std::optional<std::pair<ValueId, int64_t>> {
    // Look for a GEP instruction that produced this pointer
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result != ptr.id)
                continue;

            if (auto* gep = std::get_if<GetElementPtrInst>(&inst.inst)) {
                // Check if this is an array access pattern
                // GEP base, 0, index -> array element access
                if (gep->indices.size() >= 2) {
                    // Try to get the index as a constant
                    ValueId index_value = gep->indices.back().id;

                    // Look for the constant instruction
                    for (const auto& blk : func.blocks) {
                        for (const auto& ins : blk.instructions) {
                            if (ins.result == index_value) {
                                if (auto* const_inst = std::get_if<ConstantInst>(&ins.inst)) {
                                    if (auto* int_const =
                                            std::get_if<ConstInt>(&const_inst->value)) {
                                        return std::make_pair(gep->base.id, int_const->value);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return std::nullopt;
}

} // namespace tml::mir
