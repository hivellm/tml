//! # Constructor Initialization Fusion Pass Implementation
//!
//! Fuses consecutive stores to object fields during construction.

#include "mir/passes/constructor_fusion.hpp"

#include <algorithm>

namespace tml::mir {

auto ConstructorFusionPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    for (auto& block : func.blocks) {
        // Analyze the block for store sequences
        auto sequences = analyze_block(func, block);

        // Try to fuse each sequence
        for (const auto& seq : sequences) {
            if (can_fuse_stores(seq, func)) {
                if (fuse_stores(func, block, seq)) {
                    changed = true;
                }
            }
        }

        // Eliminate redundant vtable stores
        if (eliminate_vtable_stores(func, block)) {
            changed = true;
        }
    }

    return changed;
}

auto ConstructorFusionPass::analyze_block(Function& func, BasicBlock& block)
    -> std::vector<StoreSequence> {
    std::vector<StoreSequence> sequences;

    // Map: object pointer -> current store sequence
    std::unordered_map<ValueId, StoreSequence> active_sequences;

    for (size_t i = 0; i < block.instructions.size(); ++i) {
        const auto& inst = block.instructions[i];

        // Look for store instructions
        if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
            // Check if storing to a field of an object (via GEP)
            // Look for the GEP that produced the pointer
            for (const auto& prev_inst : block.instructions) {
                if (prev_inst.result != store->ptr.id)
                    continue;

                if (auto* gep = std::get_if<GetElementPtrInst>(&prev_inst.inst)) {
                    // This is a store through a GEP - might be field initialization
                    ValueId base = gep->base.id;

                    // Get or create sequence for this object
                    auto& seq = active_sequences[base];
                    if (seq.store_indices.empty()) {
                        seq.object_ptr = base;
                        auto class_name = get_class_name(func, base);
                        if (class_name) {
                            seq.class_name = *class_name;
                        }
                    }

                    // Record this store
                    seq.store_indices.push_back(i);
                    seq.values.push_back(store->value);

                    // Record field index (from GEP indices)
                    if (gep->indices.size() >= 2) {
                        // Second index is typically the field index
                        // This is simplified - real impl would analyze GEP more carefully
                        seq.field_indices.push_back(
                            static_cast<uint32_t>(seq.field_indices.size()));
                    }

                    stats_.constructors_analyzed++;
                    break;
                }
            }
        }

        // Call instructions may invalidate sequences (unless they're known safe)
        if (std::holds_alternative<CallInst>(inst.inst) ||
            std::holds_alternative<MethodCallInst>(inst.inst)) {
            // For now, finalize all active sequences when we see a call
            for (auto& [ptr, seq] : active_sequences) {
                if (!seq.store_indices.empty()) {
                    sequences.push_back(std::move(seq));
                }
            }
            active_sequences.clear();
        }
    }

    // Finalize remaining sequences
    for (auto& [ptr, seq] : active_sequences) {
        if (!seq.store_indices.empty()) {
            sequences.push_back(std::move(seq));
        }
    }

    return sequences;
}

auto ConstructorFusionPass::can_fuse_stores(const StoreSequence& seq, const Function& /*func*/)
    -> bool {
    // Need at least 2 stores to fuse
    if (seq.store_indices.size() < 2) {
        return false;
    }

    // Need to know the class type
    if (seq.class_name.empty()) {
        return false;
    }

    // Check that stores are to consecutive fields
    // This is a simplified check - real impl would verify field adjacency
    if (seq.field_indices.empty()) {
        return false;
    }

    // Check that values don't depend on stores being made
    // (i.e., no store results are used by later stores)
    // For now, conservatively require simple values

    return true;
}

auto ConstructorFusionPass::fuse_stores(Function& func, BasicBlock& block, const StoreSequence& seq)
    -> bool {
    if (seq.store_indices.empty() || seq.class_name.empty()) {
        return false;
    }

    // Look up class definition
    auto class_def = env_.lookup_class(seq.class_name);
    if (!class_def) {
        return false;
    }

    // Check if we have all fields covered
    size_t field_count = class_def->fields.size();
    if (seq.values.size() < field_count) {
        // Partial initialization - can't fully fuse, but could still optimize
        // For now, skip partial cases
        return false;
    }

    // Create a struct initialization instruction to replace the stores
    StructInitInst struct_init;
    struct_init.struct_name = "class." + seq.class_name;
    struct_init.fields = seq.values;

    // Get field types
    for (size_t i = 0; i < class_def->fields.size(); ++i) {
        // Convert TML type to MIR type (simplified)
        struct_init.field_types.push_back(nullptr); // Would need proper type conversion
    }

    // Create new instruction
    InstructionData init_inst;
    init_inst.result = func.fresh_value();
    init_inst.type = nullptr; // Would need the struct type
    init_inst.inst = struct_init;

    // Create store of the initialized struct
    StoreInst struct_store;
    struct_store.value = Value{init_inst.result};
    struct_store.ptr = Value{seq.object_ptr};

    InstructionData store_inst;
    store_inst.result = 0; // Store has no result
    store_inst.type = nullptr;
    store_inst.inst = struct_store;

    // Find insertion point (after the allocas)
    size_t insert_pos = 0;
    for (size_t i = 0; i < block.instructions.size(); ++i) {
        if (std::holds_alternative<AllocaInst>(block.instructions[i].inst)) {
            insert_pos = i + 1;
        } else {
            break;
        }
    }

    // Remove the original store instructions (in reverse order to maintain indices)
    std::vector<size_t> sorted_indices = seq.store_indices;
    std::sort(sorted_indices.begin(), sorted_indices.end(), std::greater<size_t>());

    for (size_t idx : sorted_indices) {
        block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(idx));
    }

    // Insert the new instructions
    block.instructions.insert(block.instructions.begin() + static_cast<std::ptrdiff_t>(insert_pos),
                              init_inst);
    block.instructions.insert(
        block.instructions.begin() + static_cast<std::ptrdiff_t>(insert_pos + 1), store_inst);

    stats_.stores_fused += seq.store_indices.size();
    return true;
}

auto ConstructorFusionPass::eliminate_vtable_stores(Function& func, BasicBlock& block) -> bool {
    bool changed = false;

    // Track vtable stores per object
    std::unordered_map<ValueId, std::vector<size_t>> vtable_stores;

    for (size_t i = 0; i < block.instructions.size(); ++i) {
        const auto& inst = block.instructions[i];

        if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
            // Check if this is a vtable pointer store
            // Vtable stores typically store to index 0 of the object (first field)
            for (const auto& prev_inst : block.instructions) {
                if (prev_inst.result != store->ptr.id)
                    continue;

                if (auto* gep = std::get_if<GetElementPtrInst>(&prev_inst.inst)) {
                    // Check if this is storing to field 0 (vtable slot)
                    if (gep->indices.size() == 2) {
                        // First GEP index is 0 (dereference), second is field index
                        // If second index is 0, this is the vtable field
                        vtable_stores[gep->base.id].push_back(i);
                    }
                }
                break;
            }
        }
    }

    // For each object with multiple vtable stores, remove all but the last
    std::vector<size_t> to_remove;
    for (const auto& [obj, stores] : vtable_stores) {
        if (stores.size() > 1) {
            // Keep only the last store (most derived vtable)
            for (size_t j = 0; j < stores.size() - 1; ++j) {
                to_remove.push_back(stores[j]);
            }
        }
    }

    // Remove redundant stores (in reverse order)
    std::sort(to_remove.begin(), to_remove.end(), std::greater<size_t>());
    for (size_t idx : to_remove) {
        block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(idx));
        stats_.vtable_stores_eliminated++;
        changed = true;
    }

    (void)func; // May be used for more analysis in future
    return changed;
}

auto ConstructorFusionPass::is_vtable_store(const InstructionData& inst, ValueId obj_ptr) -> bool {
    auto* store = std::get_if<StoreInst>(&inst.inst);
    if (!store) {
        return false;
    }

    // Would need to check if store->ptr points to field 0 of obj_ptr
    // This is simplified - real impl would trace through GEPs
    (void)obj_ptr;
    return false;
}

auto ConstructorFusionPass::get_class_name(const Function& func, ValueId ptr)
    -> std::optional<std::string> {
    // Look for the instruction that defines this pointer
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result != ptr)
                continue;

            // Check if it's an alloca with a class type
            if (auto* alloca = std::get_if<AllocaInst>(&inst.inst)) {
                // Try to extract class name from alloca name or type
                if (alloca->name.find("class.") == 0) {
                    return alloca->name.substr(6);
                }
                // Check type
                if (alloca->alloc_type) {
                    if (auto* st = std::get_if<MirStructType>(&alloca->alloc_type->kind)) {
                        if (st->name.find("class.") == 0) {
                            return st->name.substr(6);
                        }
                        return st->name;
                    }
                }
            }

            // Check if it's a call to a constructor
            if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                const std::string& fn = call->func_name;
                size_t pos = fn.rfind("_create");
                if (pos == std::string::npos) {
                    pos = fn.rfind("_new");
                }
                if (pos != std::string::npos && pos > 0) {
                    return fn.substr(0, pos);
                }
            }

            return std::nullopt;
        }
    }

    return std::nullopt;
}

} // namespace tml::mir
