//! # Alias Analysis Pass - Implementation
//!
//! Implements alias analysis to determine memory aliasing relationships.

#include "mir/passes/alias_analysis.hpp"

namespace tml::mir {

// ============================================================================
// AliasAnalysisPass - Main Implementation
// ============================================================================

auto AliasAnalysisPass::run_on_function(Function& func) -> bool {
    // Clear state from previous function
    pointer_info_.clear();
    stack_allocas_.clear();
    global_vars_.clear();
    heap_allocs_.clear();

    // Analyze all pointers in the function
    analyze_pointers(func);

    // Alias analysis is read-only - it doesn't modify the function
    return false;
}

void AliasAnalysisPass::analyze_pointers(const Function& func) {
    // First pass: identify all allocas and their properties
    for (const auto& block : func.blocks) {
        for (const auto& inst_data : block.instructions) {
            analyze_instruction(inst_data);
        }
    }

    // Second pass: trace GEPs and other derived pointers
    for (const auto& block : func.blocks) {
        for (const auto& inst_data : block.instructions) {
            if (auto* gep = std::get_if<GetElementPtrInst>(&inst_data.inst)) {
                PointerInfo info;
                info.origin = PointerOrigin::GEP;
                info.base = get_base_pointer(gep->base.id);

                // Track constant offsets
                for (const auto& idx : gep->indices) {
                    // If index is a constant, record the offset
                    auto it = pointer_info_.find(idx.id);
                    if (it != pointer_info_.end()) {
                        // Non-constant index - offset unknown
                        info.offsets.push_back(-1);
                    } else {
                        // Try to find the constant value
                        // For now, mark as unknown
                        info.offsets.push_back(-1);
                    }
                }

                info.pointee_type = gep->result_type;
                pointer_info_[inst_data.result] = std::move(info);
            }
        }
    }
}

void AliasAnalysisPass::analyze_instruction(const InstructionData& inst) {
    if (auto* alloca_inst = std::get_if<AllocaInst>(&inst.inst)) {
        PointerInfo info;
        info.origin = PointerOrigin::StackAlloca;
        info.base = inst.result;
        info.pointee_type = alloca_inst->alloc_type;
        info.is_restrict = true; // Stack allocations are inherently non-aliasing

        pointer_info_[inst.result] = std::move(info);
        stack_allocas_.insert(inst.result);
    } else if (auto* call = std::get_if<CallInst>(&inst.inst)) {
        // Check for heap allocation functions
        if (call->func_name.find("alloc") != std::string::npos ||
            call->func_name.find("malloc") != std::string::npos ||
            call->func_name.find("new") != std::string::npos) {
            PointerInfo info;
            info.origin = PointerOrigin::HeapAlloc;
            info.base = inst.result;
            info.is_restrict = true; // Fresh heap allocations don't alias existing memory

            pointer_info_[inst.result] = std::move(info);
            heap_allocs_.insert(inst.result);
        }
    }
}

auto AliasAnalysisPass::trace_pointer_origin(ValueId ptr) const -> PointerOrigin {
    auto it = pointer_info_.find(ptr);
    if (it != pointer_info_.end()) {
        return it->second.origin;
    }
    return PointerOrigin::Unknown;
}

auto AliasAnalysisPass::get_base_pointer(ValueId ptr) const -> ValueId {
    auto it = pointer_info_.find(ptr);
    if (it != pointer_info_.end() && it->second.base != INVALID_VALUE) {
        // Recursively find ultimate base
        if (it->second.base != ptr) {
            return get_base_pointer(it->second.base);
        }
        return it->second.base;
    }
    return ptr; // No info, return as-is
}

// ============================================================================
// Alias Query Methods
// ============================================================================

auto AliasAnalysisPass::query(const MemoryLocation& loc1, const MemoryLocation& loc2) const
    -> AliasResult {
    if (loc1.is_null() || loc2.is_null()) {
        return AliasResult::NoAlias;
    }

    stats_.queries_total++;
    auto result = alias(loc1.base, loc2.base);

    switch (result) {
    case AliasResult::NoAlias:
        stats_.no_alias_results++;
        break;
    case AliasResult::MayAlias:
        stats_.may_alias_results++;
        break;
    case AliasResult::MustAlias:
        stats_.must_alias_results++;
        break;
    case AliasResult::PartialAlias:
        stats_.partial_alias_results++;
        break;
    }

    return result;
}

auto AliasAnalysisPass::alias(ValueId ptr1, ValueId ptr2) const -> AliasResult {
    // Same pointer always aliases itself
    if (ptr1 == ptr2) {
        return AliasResult::MustAlias;
    }

    // Try basic alias analysis first
    auto basic_result = basic_alias(ptr1, ptr2);
    if (basic_result == AliasResult::NoAlias) {
        return AliasResult::NoAlias;
    }

    // Try type-based alias analysis
    auto tbaa_result = type_based_alias(ptr1, ptr2);
    if (tbaa_result == AliasResult::NoAlias) {
        return AliasResult::NoAlias;
    }

    // Try field-sensitive analysis
    auto field_result = field_alias(ptr1, ptr2);
    if (field_result == AliasResult::NoAlias) {
        return AliasResult::NoAlias;
    }

    // If we can't prove no-alias, assume may-alias
    return AliasResult::MayAlias;
}

auto AliasAnalysisPass::get_pointer_info(ValueId ptr) const -> const PointerInfo* {
    auto it = pointer_info_.find(ptr);
    if (it != pointer_info_.end()) {
        return &it->second;
    }
    return nullptr;
}

auto AliasAnalysisPass::is_stack_pointer(ValueId ptr) const -> bool {
    // Direct check
    if (stack_allocas_.count(ptr) > 0) {
        return true;
    }

    // Check base pointer
    auto base = get_base_pointer(ptr);
    return stack_allocas_.count(base) > 0;
}

auto AliasAnalysisPass::are_distinct_allocations(ValueId ptr1, ValueId ptr2) const -> bool {
    auto base1 = get_base_pointer(ptr1);
    auto base2 = get_base_pointer(ptr2);

    // Different base pointers from allocations don't alias
    if (base1 != base2) {
        bool is_alloc1 = stack_allocas_.count(base1) > 0 || heap_allocs_.count(base1) > 0;
        bool is_alloc2 = stack_allocas_.count(base2) > 0 || heap_allocs_.count(base2) > 0;
        return is_alloc1 && is_alloc2;
    }

    return false;
}

// ============================================================================
// Basic Alias Analysis
// ============================================================================

auto AliasAnalysisPass::basic_alias(ValueId ptr1, ValueId ptr2) const -> AliasResult {
    auto base1 = get_base_pointer(ptr1);
    auto base2 = get_base_pointer(ptr2);

    // Different stack allocations never alias
    if (stack_allocas_.count(base1) > 0 && stack_allocas_.count(base2) > 0) {
        if (base1 != base2) {
            return AliasResult::NoAlias;
        }
    }

    // Different heap allocations never alias
    if (heap_allocs_.count(base1) > 0 && heap_allocs_.count(base2) > 0) {
        if (base1 != base2) {
            return AliasResult::NoAlias;
        }
    }

    // Stack and heap allocations never alias each other
    if ((stack_allocas_.count(base1) > 0 && heap_allocs_.count(base2) > 0) ||
        (stack_allocas_.count(base2) > 0 && heap_allocs_.count(base1) > 0)) {
        return AliasResult::NoAlias;
    }

    // Stack allocations don't alias global variables
    if ((stack_allocas_.count(base1) > 0 && global_vars_.count(base2) > 0) ||
        (stack_allocas_.count(base2) > 0 && global_vars_.count(base1) > 0)) {
        return AliasResult::NoAlias;
    }

    return AliasResult::MayAlias;
}

// ============================================================================
// Type-Based Alias Analysis (TBAA)
// ============================================================================

auto AliasAnalysisPass::type_based_alias(ValueId ptr1, ValueId ptr2) const -> AliasResult {
    auto info1 = get_pointer_info(ptr1);
    auto info2 = get_pointer_info(ptr2);

    if (!info1 || !info2) {
        return AliasResult::MayAlias;
    }

    // If both have pointee types, check for type compatibility
    if (info1->pointee_type && info2->pointee_type) {
        // Different primitive types don't alias (strict aliasing rule)
        // Note: This assumes TML follows strict aliasing like Rust/C++
        auto* prim1 = std::get_if<MirPrimitiveType>(&info1->pointee_type->kind);
        auto* prim2 = std::get_if<MirPrimitiveType>(&info2->pointee_type->kind);

        if (prim1 && prim2) {
            // Different primitive types don't alias
            if (prim1->kind != prim2->kind) {
                return AliasResult::NoAlias;
            }
        }

        // Different struct types don't alias (assuming no inheritance/casting)
        auto* struct1 = std::get_if<MirStructType>(&info1->pointee_type->kind);
        auto* struct2 = std::get_if<MirStructType>(&info2->pointee_type->kind);

        if (struct1 && struct2) {
            if (struct1->name != struct2->name) {
                return AliasResult::NoAlias;
            }
        }
    }

    return AliasResult::MayAlias;
}

// ============================================================================
// Field-Sensitive Alias Analysis
// ============================================================================

auto AliasAnalysisPass::field_alias(ValueId ptr1, ValueId ptr2) const -> AliasResult {
    auto info1 = get_pointer_info(ptr1);
    auto info2 = get_pointer_info(ptr2);

    if (!info1 || !info2) {
        return AliasResult::MayAlias;
    }

    // If both are GEP-derived from the same base with known offsets
    if (info1->origin == PointerOrigin::GEP && info2->origin == PointerOrigin::GEP) {
        return gep_alias(*info1, *info2);
    }

    return AliasResult::MayAlias;
}

auto AliasAnalysisPass::gep_alias(const PointerInfo& info1, const PointerInfo& info2) const
    -> AliasResult {
    // Must have same base for meaningful comparison
    if (info1.base != info2.base) {
        return AliasResult::MayAlias;
    }

    // Check if offsets differ at any known position
    size_t min_len = std::min(info1.offsets.size(), info2.offsets.size());
    for (size_t i = 0; i < min_len; i++) {
        // If both offsets are known and different, no alias
        if (info1.offsets[i] >= 0 && info2.offsets[i] >= 0) {
            if (info1.offsets[i] != info2.offsets[i]) {
                return AliasResult::NoAlias;
            }
        }
    }

    // If same base and same known offsets, might be must-alias
    if (info1.offsets == info2.offsets && !info1.offsets.empty()) {
        bool all_known = true;
        for (auto off : info1.offsets) {
            if (off < 0) {
                all_known = false;
                break;
            }
        }
        if (all_known) {
            return AliasResult::MustAlias;
        }
    }

    return AliasResult::MayAlias;
}

// ============================================================================
// Module-Level Alias Analysis
// ============================================================================

auto ModuleAliasAnalysis::run(Module& module) -> bool {
    function_analyses_.clear();

    // Run alias analysis on each function
    for (auto& func : module.functions) {
        auto analysis = std::make_unique<AliasAnalysisPass>();
        // FunctionPass::run(Module&) iterates and calls run_on_function
        // We need to call it directly, so we create a temporary single-function module
        // For simplicity, we just run on the whole module and the pass handles one function
        Module temp_module;
        temp_module.name = func.name;
        temp_module.functions.push_back(std::move(func));

        analysis->run(temp_module);

        // Move function back
        func = std::move(temp_module.functions[0]);
        function_analyses_[func.name] = std::move(analysis);
    }

    return false; // Analysis doesn't modify module
}

auto ModuleAliasAnalysis::query_in_function(const std::string& func_name,
                                            const MemoryLocation& loc1,
                                            const MemoryLocation& loc2) const -> AliasResult {
    auto it = function_analyses_.find(func_name);
    if (it != function_analyses_.end()) {
        return it->second->query(loc1, loc2);
    }
    return AliasResult::MayAlias;
}

} // namespace tml::mir
