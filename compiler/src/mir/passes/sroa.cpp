//! # Scalar Replacement of Aggregates (SROA) Pass
//!
//! Breaks up alloca of aggregates into multiple scalar allocas.

#include "mir/passes/sroa.hpp"

#include <algorithm>

namespace tml::mir {

auto SROAPass::run_on_function(Function& func) -> bool {
    bool changed = false;

    // Collect candidate allocas
    std::vector<AllocaInfo> candidates;

    for (size_t b = 0; b < func.blocks.size(); ++b) {
        auto& block = func.blocks[b];
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& inst = block.instructions[i];
            if (auto* alloca = std::get_if<AllocaInst>(&inst.inst)) {
                if (can_split_type(alloca->alloc_type)) {
                    auto info = analyze_alloca(func, inst.result, *alloca, b, i);
                    if (info.can_split && !info.accessed_fields.empty()) {
                        candidates.push_back(std::move(info));
                    }
                }
            }
        }
    }

    // Process candidates in reverse order to maintain indices
    for (auto it = candidates.rbegin(); it != candidates.rend(); ++it) {
        auto& info = *it;

        // Split the alloca
        auto splits = split_alloca(func, info);
        if (splits.empty()) {
            continue;
        }

        // Rewrite uses
        rewrite_uses(func, info, splits);

        changed = true;
    }

    return changed;
}

auto SROAPass::analyze_alloca(Function& func, ValueId alloca_id, const AllocaInst& alloca,
                               size_t block_idx, size_t inst_idx) -> AllocaInfo {
    AllocaInfo info;
    info.alloca_id = alloca_id;
    info.alloc_type = alloca.alloc_type;
    info.name = alloca.name;
    info.block_index = block_idx;
    info.inst_index = inst_idx;
    info.can_split = true;

    // Check all uses of this alloca
    is_simple_access(func, alloca_id, info);

    return info;
}

auto SROAPass::is_simple_access(const Function& func, ValueId alloca_id, AllocaInfo& info) -> bool {
    // Find all uses of the alloca
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            std::visit(
                [&](const auto& i) {
                    using T = std::decay_t<decltype(i)>;

                    if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                        if (i.base.id == alloca_id) {
                            // Check if this is a simple field access: GEP base, 0, field_idx
                            if (i.indices.size() == 2) {
                                // First index should be 0 (accessing the aggregate itself)
                                // Second index is the field
                                // For now, we need to check if indices are constants
                                // This is a simplified check - in practice we'd look at the constant value
                                info.accessed_fields.insert(0); // Mark as accessed
                            } else if (i.indices.size() == 1) {
                                // Single index access (e.g., array element or first-level field)
                                info.accessed_fields.insert(0);
                            } else {
                                // Complex access pattern - can't split
                                info.can_split = false;
                            }
                        }
                    } else if constexpr (std::is_same_v<T, LoadInst>) {
                        if (i.ptr.id == alloca_id) {
                            // Direct load of entire aggregate - can't split
                            info.can_split = false;
                        }
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        if (i.ptr.id == alloca_id) {
                            // Direct store to entire aggregate - can't split
                            // Unless it's a store of a struct init, which we could handle
                            info.can_split = false;
                        }
                    } else if constexpr (std::is_same_v<T, CallInst>) {
                        // If alloca is passed to a call, we can't split
                        for (const auto& arg : i.args) {
                            if (arg.id == alloca_id) {
                                info.can_split = false;
                            }
                        }
                    } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                        if (i.receiver.id == alloca_id) {
                            info.can_split = false;
                        }
                        for (const auto& arg : i.args) {
                            if (arg.id == alloca_id) {
                                info.can_split = false;
                            }
                        }
                    }
                },
                inst.inst);
        }
    }

    return info.can_split;
}

auto SROAPass::can_split_type(const MirTypePtr& type) -> bool {
    if (!type) return false;

    if (auto* st = std::get_if<MirStructType>(&type->kind)) {
        // Can split structs
        return true;
    }
    if (auto* tt = std::get_if<MirTupleType>(&type->kind)) {
        // Can split tuples with reasonable size
        return tt->elements.size() <= 8;
    }
    if (auto* at = std::get_if<MirArrayType>(&type->kind)) {
        // Can split small fixed-size arrays
        return at->size <= 8;
    }

    return false;
}

auto SROAPass::get_field_count(const MirTypePtr& type, const Function& /*func*/) -> size_t {
    if (auto* st = std::get_if<MirStructType>(&type->kind)) {
        // For structs, we need to look up the definition
        // For now, return a reasonable default
        (void)st;
        return 4; // Placeholder - would need module context
    }
    if (auto* tt = std::get_if<MirTupleType>(&type->kind)) {
        return tt->elements.size();
    }
    if (auto* at = std::get_if<MirArrayType>(&type->kind)) {
        return at->size;
    }
    return 0;
}

auto SROAPass::get_field_type(const MirTypePtr& type, uint32_t index,
                               const Function& /*func*/) -> MirTypePtr {
    if (auto* tt = std::get_if<MirTupleType>(&type->kind)) {
        if (index < tt->elements.size()) {
            return tt->elements[index];
        }
    }
    if (auto* at = std::get_if<MirArrayType>(&type->kind)) {
        return at->element;
    }
    // For structs, would need module context to look up field types
    return make_i32_type(); // Placeholder
}

auto SROAPass::split_alloca(Function& func, const AllocaInfo& info)
    -> std::unordered_map<uint32_t, SplitAlloca> {
    std::unordered_map<uint32_t, SplitAlloca> splits;

    // Get field count
    size_t field_count = get_field_count(info.alloc_type, func);
    if (field_count == 0) {
        return splits;
    }

    // Create new allocas for each field
    auto& entry_block = func.entry_block();

    for (size_t i = 0; i < field_count; ++i) {
        SplitAlloca split;
        split.new_alloca_id = func.fresh_value();
        split.field_type = get_field_type(info.alloc_type, static_cast<uint32_t>(i), func);
        split.name = info.name + "_" + std::to_string(i);

        // Create the alloca instruction
        AllocaInst new_alloca;
        new_alloca.alloc_type = split.field_type;
        new_alloca.name = split.name;

        InstructionData inst_data;
        inst_data.result = split.new_alloca_id;
        inst_data.type = make_pointer_type(split.field_type);
        inst_data.inst = new_alloca;

        // Insert at the beginning of entry block (after existing allocas)
        size_t insert_pos = 0;
        for (size_t j = 0; j < entry_block.instructions.size(); ++j) {
            if (!std::holds_alternative<AllocaInst>(entry_block.instructions[j].inst)) {
                insert_pos = j;
                break;
            }
            insert_pos = j + 1;
        }
        entry_block.instructions.insert(
            entry_block.instructions.begin() + static_cast<std::ptrdiff_t>(insert_pos), inst_data);

        splits[static_cast<uint32_t>(i)] = split;
    }

    return splits;
}

auto SROAPass::rewrite_uses(Function& func, const AllocaInfo& info,
                            const std::unordered_map<uint32_t, SplitAlloca>& splits) -> void {
    // Track GEPs to remove and their replacements
    std::unordered_map<ValueId, ValueId> gep_replacements;
    std::vector<std::pair<size_t, size_t>> to_remove; // (block_idx, inst_idx)

    // Find and process GEPs that use the original alloca
    for (size_t b = 0; b < func.blocks.size(); ++b) {
        auto& block = func.blocks[b];
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& inst = block.instructions[i];

            if (auto* gep = std::get_if<GetElementPtrInst>(&inst.inst)) {
                if (gep->base.id == info.alloca_id) {
                    // Determine which field this GEP accesses
                    // For simplicity, assume second index is the field index
                    uint32_t field_idx = 0;
                    if (gep->indices.size() >= 2) {
                        // In a real implementation, we'd extract the constant value
                        // For now, use index 0 as placeholder
                        field_idx = 0;
                    }

                    auto it = splits.find(field_idx);
                    if (it != splits.end()) {
                        // Replace this GEP's result with the new alloca
                        gep_replacements[inst.result] = it->second.new_alloca_id;
                        to_remove.emplace_back(b, i);
                    }
                }
            }
        }
    }

    // Apply replacements to all uses
    for (auto& block : func.blocks) {
        for (auto& inst : block.instructions) {
            std::visit(
                [&gep_replacements](auto& i) {
                    using T = std::decay_t<decltype(i)>;

                    if constexpr (std::is_same_v<T, LoadInst>) {
                        auto it = gep_replacements.find(i.ptr.id);
                        if (it != gep_replacements.end()) {
                            i.ptr.id = it->second;
                        }
                    } else if constexpr (std::is_same_v<T, StoreInst>) {
                        auto it = gep_replacements.find(i.ptr.id);
                        if (it != gep_replacements.end()) {
                            i.ptr.id = it->second;
                        }
                    }
                },
                inst.inst);
        }
    }

    // Remove dead GEPs (in reverse order)
    std::sort(to_remove.begin(), to_remove.end(),
              [](const auto& a, const auto& b) {
                  if (a.first != b.first) return a.first > b.first;
                  return a.second > b.second;
              });

    for (const auto& [block_idx, inst_idx] : to_remove) {
        auto& block = func.blocks[block_idx];
        block.instructions.erase(block.instructions.begin() +
                                 static_cast<std::ptrdiff_t>(inst_idx));
    }

    // Remove the original alloca
    auto& orig_block = func.blocks[info.block_index];
    // Find and remove the original alloca
    for (size_t i = 0; i < orig_block.instructions.size(); ++i) {
        if (orig_block.instructions[i].result == info.alloca_id) {
            orig_block.instructions.erase(orig_block.instructions.begin() +
                                          static_cast<std::ptrdiff_t>(i));
            break;
        }
    }
}

auto SROAPass::cleanup(Function& /*func*/, const AllocaInfo& /*info*/,
                       const std::unordered_set<ValueId>& /*dead_values*/) -> void {
    // Cleanup is handled in rewrite_uses for now
}

} // namespace tml::mir
