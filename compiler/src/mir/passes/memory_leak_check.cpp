//! # Memory Leak Detection Pass
//!
//! Static analysis pass to detect potential memory leaks.

#include "mir/passes/memory_leak_check.hpp"

#include "log/log.hpp"

#include <algorithm>

namespace tml::mir {

auto MemoryLeakCheckPass::run(Module& module) -> bool {
    warnings_.clear();
    module_ = &module;

    for (const auto& func : module.functions) {
        // Skip extern functions (FFI declarations) - they have no blocks
        if (func.blocks.empty()) {
            continue;
        }
        analyze_function(func);
    }

    return false; // This pass doesn't modify the IR
}

auto MemoryLeakCheckPass::has_errors() const -> bool {
    for (const auto& w : warnings_) {
        if (w.is_error) {
            return true;
        }
    }
    return false;
}

void MemoryLeakCheckPass::print_warnings() const {
    for (const auto& warning : warnings_) {
        if (warning.is_error) {
            TML_LOG_ERROR("mir", "potential memory leak in function '"
                                     << warning.function_name << "' at " << warning.allocation_site
                                     << ": " << warning.reason);
        } else {
            TML_LOG_WARN("mir", "potential memory leak in function '"
                                    << warning.function_name << "' at " << warning.allocation_site
                                    << ": " << warning.reason);
        }
        TML_LOG_INFO("mir", "  note: ensure all heap allocations are freed or "
                            "ownership is properly transferred");
    }
}

void MemoryLeakCheckPass::analyze_function(const Function& func) {
    // Find all heap allocations
    auto allocations = find_allocations(func);
    if (allocations.empty()) {
        return; // No allocations to check
    }

    // Find all free/destroy calls
    auto freed_values = find_frees(func);

    // Check each allocation
    for (const auto& [alloc_value, block_id] : allocations) {
        // Skip if freed
        if (freed_values.count(alloc_value) > 0) {
            continue;
        }

        // Skip if returned (ownership transferred to caller)
        if (is_returned(func, alloc_value)) {
            continue;
        }

        // Skip if stored to a field (ownership transferred)
        if (is_stored_to_field(func, alloc_value)) {
            continue;
        }

        // Skip if passed to a function that takes ownership
        if (escapes_via_arg(func, alloc_value)) {
            continue;
        }

        // This allocation is potentially leaked
        MemoryLeakWarning warning;
        warning.function_name = func.name;
        warning.block_id = block_id;
        warning.alloc_value = alloc_value;
        warning.allocation_site = "block '" + get_block_name(func, block_id) + "'";
        warning.reason = "heap allocation is never freed";
        warning.is_error = true; // Memory leaks are errors

        warnings_.push_back(warning);
    }
}

auto MemoryLeakCheckPass::find_allocations(const Function& func)
    -> std::unordered_map<ValueId, uint32_t> {
    std::unordered_map<ValueId, uint32_t> allocations;

    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (is_heap_allocation(inst.inst) && !is_arena_allocation(inst.inst)) {
                allocations[inst.result] = block.id;
            }
        }
    }

    return allocations;
}

auto MemoryLeakCheckPass::find_frees(const Function& func) -> std::unordered_set<ValueId> {
    std::unordered_set<ValueId> freed;

    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            ValueId freed_value;
            if (is_free_call(inst.inst, freed_value)) {
                freed.insert(freed_value);
            }
        }
    }

    return freed;
}

auto MemoryLeakCheckPass::is_returned(const Function& func, ValueId value) -> bool {
    for (const auto& block : func.blocks) {
        if (!block.terminator)
            continue;

        if (auto* ret = std::get_if<ReturnTerm>(&*block.terminator)) {
            if (ret->value && ret->value->id == value) {
                return true;
            }
        }
    }
    return false;
}

auto MemoryLeakCheckPass::is_stored_to_field(const Function& func, ValueId value) -> bool {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (auto* store = std::get_if<StoreInst>(&inst.inst)) {
                // Check if we're storing the allocation value
                if (store->value.id == value) {
                    // Check if destination is a GEP (field access)
                    for (const auto& b : func.blocks) {
                        for (const auto& i : b.instructions) {
                            if (i.result == store->ptr.id) {
                                if (std::holds_alternative<GetElementPtrInst>(i.inst)) {
                                    return true; // Stored to a field
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return false;
}

auto MemoryLeakCheckPass::escapes_via_arg(const Function& func, ValueId value) -> bool {
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                for (const auto& arg : call->args) {
                    if (arg.id == value) {
                        // Check if the function name suggests ownership transfer
                        // Common patterns: push, add, insert, set, store
                        const auto& name = call->func_name;
                        if (name.find("push") != std::string::npos ||
                            name.find("add") != std::string::npos ||
                            name.find("insert") != std::string::npos ||
                            name.find("set_") != std::string::npos ||
                            name.find("store") != std::string::npos ||
                            name.find("take") != std::string::npos ||
                            name.find("consume") != std::string::npos) {
                            return true;
                        }
                    }
                }
            }
            if (auto* method_call = std::get_if<MethodCallInst>(&inst.inst)) {
                for (const auto& arg : method_call->args) {
                    if (arg.id == value) {
                        const auto& name = method_call->method_name;
                        if (name.find("push") != std::string::npos ||
                            name.find("add") != std::string::npos ||
                            name.find("insert") != std::string::npos ||
                            name.find("set_") != std::string::npos ||
                            name.find("store") != std::string::npos ||
                            name.find("take") != std::string::npos ||
                            name.find("consume") != std::string::npos) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

auto MemoryLeakCheckPass::get_block_name(const Function& func, uint32_t block_id) -> std::string {
    for (const auto& block : func.blocks) {
        if (block.id == block_id) {
            return block.name.empty() ? std::to_string(block_id) : block.name;
        }
    }
    return std::to_string(block_id);
}

auto MemoryLeakCheckPass::is_heap_allocation(const Instruction& inst) const -> bool {
    if (auto* call = std::get_if<CallInst>(&inst)) {
        const auto& name = call->func_name;
        // Standard allocation functions
        if (name == "malloc" || name == "calloc" || name == "realloc" || name == "alloc" ||
            name == "tml_alloc" || name == "heap_alloc" ||
            name.find("::create") != std::string::npos || name.find("::new") != std::string::npos ||
            name.find("_create") != std::string::npos || name.find("_new") != std::string::npos) {
            return true;
        }
    }
    return false;
}

auto MemoryLeakCheckPass::is_free_call(const Instruction& inst, ValueId& freed_value) const
    -> bool {
    if (auto* call = std::get_if<CallInst>(&inst)) {
        const auto& name = call->func_name;
        if (name == "free" || name == "tml_free" || name == "heap_free" ||
            name.find("::destroy") != std::string::npos ||
            name.find("::drop") != std::string::npos ||
            name.find("::delete") != std::string::npos ||
            name.find("_destroy") != std::string::npos || name.find("_drop") != std::string::npos ||
            name.find("_free") != std::string::npos) {
            if (!call->args.empty()) {
                freed_value = call->args[0].id;
                return true;
            }
        }
    }
    return false;
}

auto MemoryLeakCheckPass::is_arena_allocation(const Instruction& inst) const -> bool {
    if (auto* call = std::get_if<CallInst>(&inst)) {
        const auto& name = call->func_name;
        // Arena allocations are managed by the arena
        if (name.find("arena") != std::string::npos || name.find("Arena") != std::string::npos ||
            name.find("pool") != std::string::npos || name.find("Pool") != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace tml::mir
