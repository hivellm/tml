//! # Escape Analysis Pass
//!
//! This pass determines which allocations can be stack-promoted.
//!
//! ## Escape States
//!
//! | State        | Meaning                              |
//! |--------------|--------------------------------------|
//! | NoEscape     | Value stays within function          |
//! | ArgEscape    | Value passed to called function      |
//! | ReturnEscape | Value returned from function         |
//! | GlobalEscape | Value stored to global/escaped ptr   |
//!
//! ## Stack Promotion
//!
//! Allocations with NoEscape can be converted from heap to stack:
//! - `alloc(size)` → `alloca`
//! - Eliminates heap allocation overhead
//! - Automatic deallocation on function return
//!
//! ## Analysis Process
//!
//! 1. Initialize all values as NoEscape
//! 2. Analyze stores, calls, returns for escapes
//! 3. Propagate escape info through data flow
//! 4. Mark non-escaping allocations as stack-promotable

#include "mir/passes/escape_analysis.hpp"

#include <algorithm>

namespace tml::mir {

// ============================================================================
// EscapeAnalysisPass Implementation
// ============================================================================

auto EscapeAnalysisPass::run_on_function(Function& func) -> bool {
    // Reset state for this function
    escape_info_.clear();
    stats_ = Stats{};

    // Initialize all values as NoEscape
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result != INVALID_VALUE) {
                escape_info_[inst.result] = EscapeInfo{EscapeState::NoEscape, false, false, false};
            }
        }
    }

    // Analyze the function
    analyze_function(func);

    // Propagate escape information
    propagate_escapes(func);

    // Update statistics
    for (const auto& [value_id, info] : escape_info_) {
        switch (info.state) {
        case EscapeState::NoEscape:
            stats_.no_escape++;
            if (info.is_stack_promotable) {
                stats_.stack_promotable++;
            }
            break;
        case EscapeState::ArgEscape:
            stats_.arg_escape++;
            break;
        case EscapeState::ReturnEscape:
            stats_.return_escape++;
            break;
        case EscapeState::GlobalEscape:
            stats_.global_escape++;
            break;
        case EscapeState::Unknown:
            break;
        }
    }

    // This pass is analysis-only, doesn't modify the function
    return false;
}

auto EscapeAnalysisPass::get_escape_info(ValueId value) const -> EscapeInfo {
    auto it = escape_info_.find(value);
    if (it != escape_info_.end()) {
        return it->second;
    }
    return EscapeInfo{EscapeState::Unknown, false, false, false};
}

auto EscapeAnalysisPass::can_stack_promote(ValueId value) const -> bool {
    auto it = escape_info_.find(value);
    if (it != escape_info_.end()) {
        return it->second.is_stack_promotable;
    }
    return false;
}

auto EscapeAnalysisPass::get_stack_promotable() const -> std::vector<ValueId> {
    std::vector<ValueId> result;
    for (const auto& [value_id, info] : escape_info_) {
        if (info.is_stack_promotable) {
            result.push_back(value_id);
        }
    }
    return result;
}

void EscapeAnalysisPass::analyze_function(Function& func) {
    // Analyze all instructions
    for (auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            analyze_instruction(inst, func);
        }

        // Analyze terminator for returns
        if (block.terminator) {
            if (auto* ret = std::get_if<ReturnTerm>(&*block.terminator)) {
                analyze_return(*ret);
            }
        }
    }
}

void EscapeAnalysisPass::analyze_instruction(const InstructionData& inst,
                                             [[maybe_unused]] Function& func) {
    std::visit(
        [this, &inst](const auto& i) {
            using T = std::decay_t<decltype(i)>;

            if constexpr (std::is_same_v<T, CallInst>) {
                analyze_call(i, inst.result);
            } else if constexpr (std::is_same_v<T, StoreInst>) {
                analyze_store(i);
            } else if constexpr (std::is_same_v<T, AllocaInst>) {
                // Mark as allocation and potential stack promotable
                stats_.total_allocations++;
                auto& info = escape_info_[inst.result];
                info.state = EscapeState::NoEscape;
                info.is_stack_promotable = true;
            }
        },
        inst.inst);
}

void EscapeAnalysisPass::analyze_call(const CallInst& call, ValueId result_id) {
    // Check if this is a heap allocation call
    bool is_heap_alloc = call.func_name == "alloc" || call.func_name == "heap_alloc" ||
                         call.func_name == "malloc" || call.func_name == "Heap::new" ||
                         call.func_name == "tml_alloc";

    if (is_heap_alloc && result_id != INVALID_VALUE) {
        stats_.total_allocations++;
        auto& info = escape_info_[result_id];
        info.may_alias_heap = true;
        // Heap allocations start as NoEscape but may be promoted if they don't escape
        info.state = EscapeState::NoEscape;
        info.is_stack_promotable = true;
    }

    // Arguments passed to function calls may escape
    for (const auto& arg : call.args) {
        if (arg.id != INVALID_VALUE) {
            mark_escape(arg.id, EscapeState::ArgEscape);
        }
    }
}

void EscapeAnalysisPass::analyze_store(const StoreInst& store) {
    // If storing to a pointer that might be global, mark value as escaping
    auto ptr_info = get_escape_info(store.ptr.id);
    if (ptr_info.may_alias_global) {
        mark_escape(store.value.id, EscapeState::GlobalEscape);
    }

    // If storing a pointer value, it may escape through the store destination
    if (store.value.type && std::holds_alternative<MirPointerType>(store.value.type->kind)) {
        // The pointer value being stored may now escape
        mark_escape(store.value.id, EscapeState::GlobalEscape);
    }
}

void EscapeAnalysisPass::analyze_return(const ReturnTerm& ret) {
    if (ret.value && ret.value->id != INVALID_VALUE) {
        mark_escape(ret.value->id, EscapeState::ReturnEscape);
    }
}

void EscapeAnalysisPass::mark_escape(ValueId value, EscapeState state) {
    if (value == INVALID_VALUE)
        return;

    auto it = escape_info_.find(value);
    if (it != escape_info_.end()) {
        // Update to more severe escape state
        // Order: NoEscape < ArgEscape < ReturnEscape < GlobalEscape < Unknown
        auto current = it->second.state;
        if (state > current) {
            it->second.state = state;
            // If value escapes, it can't be stack promoted
            if (state != EscapeState::NoEscape) {
                it->second.is_stack_promotable = false;
            }
        }
    } else {
        EscapeInfo info;
        info.state = state;
        info.is_stack_promotable = (state == EscapeState::NoEscape);
        escape_info_[value] = info;
    }
}

void EscapeAnalysisPass::propagate_escapes(Function& func) {
    // Fixed-point iteration to propagate escape information
    bool changed = true;
    while (changed) {
        changed = false;

        for (const auto& block : func.blocks) {
            for (const auto& inst : block.instructions) {
                // If this instruction uses values that escape, the result may also escape
                std::visit(
                    [this, &inst, &changed](const auto& i) {
                        using T = std::decay_t<decltype(i)>;

                        if constexpr (std::is_same_v<T, BinaryInst>) {
                            // Binary operations on escaping values
                            auto left_info = get_escape_info(i.left.id);
                            auto right_info = get_escape_info(i.right.id);
                            if (left_info.escapes() || right_info.escapes()) {
                                auto new_state = left_info.state > right_info.state
                                                     ? left_info.state
                                                     : right_info.state;
                                auto old_info = get_escape_info(inst.result);
                                if (new_state > old_info.state) {
                                    mark_escape(inst.result, new_state);
                                    changed = true;
                                }
                            }
                        } else if constexpr (std::is_same_v<T, LoadInst>) {
                            // Loading from an escaping pointer may produce escaping value
                            auto ptr_info = get_escape_info(i.ptr.id);
                            if (ptr_info.escapes()) {
                                auto old_info = get_escape_info(inst.result);
                                if (ptr_info.state > old_info.state) {
                                    mark_escape(inst.result, ptr_info.state);
                                    changed = true;
                                }
                            }
                        } else if constexpr (std::is_same_v<T, PhiInst>) {
                            // Phi nodes propagate escape from any incoming value
                            EscapeState max_state = EscapeState::NoEscape;
                            for (const auto& [value, block_id] : i.incoming) {
                                auto info = get_escape_info(value.id);
                                if (info.state > max_state) {
                                    max_state = info.state;
                                }
                            }
                            auto old_info = get_escape_info(inst.result);
                            if (max_state > old_info.state) {
                                mark_escape(inst.result, max_state);
                                changed = true;
                            }
                        } else if constexpr (std::is_same_v<T, SelectInst>) {
                            // Select propagates from either branch
                            auto true_info = get_escape_info(i.true_val.id);
                            auto false_info = get_escape_info(i.false_val.id);
                            auto new_state = true_info.state > false_info.state ? true_info.state
                                                                                : false_info.state;
                            auto old_info = get_escape_info(inst.result);
                            if (new_state > old_info.state) {
                                mark_escape(inst.result, new_state);
                                changed = true;
                            }
                        }
                    },
                    inst.inst);
            }
        }
    }
}

auto EscapeAnalysisPass::is_allocation(const Instruction& inst) const -> bool {
    if (std::holds_alternative<AllocaInst>(inst)) {
        return true;
    }
    if (auto* call = std::get_if<CallInst>(&inst)) {
        return call->func_name == "alloc" || call->func_name == "heap_alloc" ||
               call->func_name == "malloc" || call->func_name == "Heap::new" ||
               call->func_name == "tml_alloc";
    }
    return false;
}

// ============================================================================
// StackPromotionPass Implementation
// ============================================================================

auto StackPromotionPass::run_on_function(Function& func) -> bool {
    bool changed = false;
    stats_ = Stats{};

    // Find and promote stack-promotable allocations
    auto promotable = escape_analysis_.get_stack_promotable();

    for (auto& block : func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            const auto& inst = block.instructions[i];

            // Check if this instruction result is promotable
            if (inst.result != INVALID_VALUE) {
                auto it = std::find(promotable.begin(), promotable.end(), inst.result);
                if (it != promotable.end()) {
                    if (promote_allocation(block, i, func)) {
                        changed = true;
                    }
                }
            }
        }
    }

    return changed;
}

auto StackPromotionPass::promote_allocation(BasicBlock& block, size_t inst_index,
                                            [[maybe_unused]] Function& func) -> bool {
    auto& inst = block.instructions[inst_index];

    // Check if this is a heap allocation call
    if (auto* call = std::get_if<CallInst>(&inst.inst)) {
        if (call->func_name == "alloc" || call->func_name == "heap_alloc" ||
            call->func_name == "Heap::new" || call->func_name == "tml_alloc") {
            // Convert to alloca instruction
            size_t alloc_size = estimate_allocation_size(inst.inst);

            // Replace call with alloca
            AllocaInst alloca_inst;
            alloca_inst.alloc_type = call->return_type;
            alloca_inst.name = "promoted_" + std::to_string(inst.result);

            inst.inst = alloca_inst;

            stats_.allocations_promoted++;
            stats_.bytes_saved += alloc_size;

            return true;
        }
    }

    return false;
}

auto StackPromotionPass::estimate_allocation_size(const Instruction& inst) const -> size_t {
    if (auto* call = std::get_if<CallInst>(&inst)) {
        // Try to get size from first argument if it's a constant
        if (!call->args.empty()) {
            // For now, return a default estimate
            return 64; // Default estimate in bytes
        }
    } else if (auto* alloca = std::get_if<AllocaInst>(&inst)) {
        // Estimate based on type
        if (alloca->alloc_type) {
            if (auto* prim = std::get_if<MirPrimitiveType>(&alloca->alloc_type->kind)) {
                switch (prim->kind) {
                case PrimitiveType::I8:
                case PrimitiveType::U8:
                case PrimitiveType::Bool:
                    return 1;
                case PrimitiveType::I16:
                case PrimitiveType::U16:
                    return 2;
                case PrimitiveType::I32:
                case PrimitiveType::U32:
                case PrimitiveType::F32:
                    return 4;
                case PrimitiveType::I64:
                case PrimitiveType::U64:
                case PrimitiveType::F64:
                case PrimitiveType::Ptr:
                    return 8;
                case PrimitiveType::I128:
                case PrimitiveType::U128:
                    return 16;
                default:
                    return 8;
                }
            }
        }
    }

    return 8; // Default pointer size
}

} // namespace tml::mir
