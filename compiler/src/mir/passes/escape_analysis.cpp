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
//! ## Class Instance Tracking
//!
//! For OOP support, we track class instances created via constructors:
//! - Constructor calls (`ClassName_new`) create class instances
//! - Method calls may cause `this` to escape
//! - Field stores may cause referenced objects to escape
//!
//! ## Analysis Process
//!
//! 1. Initialize all values as NoEscape
//! 2. Track `this` parameter for methods
//! 3. Analyze stores, calls, returns for escapes
//! 4. Propagate escape info through data flow
//! 5. Mark non-escaping allocations as stack-promotable

#include "mir/passes/escape_analysis.hpp"

#include <algorithm>

namespace tml::mir {

// ============================================================================
// Class Instance Detection Helpers
// ============================================================================

auto EscapeAnalysisPass::is_constructor_call(const std::string& func_name) const -> bool {
    // Constructor pattern: ClassName_new or ClassName_new_variant
    auto pos = func_name.rfind("_new");
    if (pos == std::string::npos) {
        return false;
    }
    // Must be at end or followed by underscore (variant)
    return pos + 4 == func_name.length() || func_name[pos + 4] == '_';
}

auto EscapeAnalysisPass::extract_class_name(const std::string& func_name) const -> std::string {
    // Extract class name from constructor pattern: ClassName_new -> ClassName
    auto pos = func_name.rfind("_new");
    if (pos == std::string::npos) {
        return "";
    }
    return func_name.substr(0, pos);
}

void EscapeAnalysisPass::track_this_parameter(const Function& func) {
    // Check if this is a method (has implicit this parameter)
    // Method pattern: ClassName_methodName
    // The first parameter is typically `this` for instance methods
    if (func.params.empty()) {
        return;
    }

    // Check if this looks like a method by examining the function name
    // Methods typically have format: ClassName_methodName
    auto underscore_pos = func.name.find('_');
    if (underscore_pos == std::string::npos || underscore_pos == 0) {
        return; // Not a method
    }

    // Don't track constructors - they create new instances
    if (is_constructor_call(func.name)) {
        return;
    }

    // First parameter is `this` - mark it as potentially escaping through return
    // since we can't track what happens to it precisely
    const auto& first_param = func.params[0];
    if (first_param.value_id != INVALID_VALUE) {
        auto& info = escape_info_[first_param.value_id];
        info.state = EscapeState::NoEscape; // Start as no escape
        info.is_class_instance = true;
        info.class_name = func.name.substr(0, underscore_pos);
    }
}

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
                escape_info_[inst.result] =
                    EscapeInfo{EscapeState::NoEscape, false, false, false, false, ""};
            }
        }
    }

    // Track `this` parameter for class methods
    track_this_parameter(func);

    // Analyze the function
    analyze_function(func);

    // Propagate escape information
    propagate_escapes(func);

    // Update statistics
    for (const auto& [value_id, info] : escape_info_) {
        // Track class instance statistics
        if (info.is_class_instance) {
            stats_.class_instances++;
            if (info.state == EscapeState::NoEscape) {
                stats_.class_instances_no_escape++;
            }
            if (info.is_stack_promotable) {
                stats_.class_instances_promotable++;
            }
        }

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
            } else if constexpr (std::is_same_v<T, MethodCallInst>) {
                analyze_method_call(i, inst.result);
            } else if constexpr (std::is_same_v<T, StoreInst>) {
                analyze_store(i);
            } else if constexpr (std::is_same_v<T, GetElementPtrInst>) {
                analyze_gep(i, inst.result);
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

    // Check if this is an arena allocation call
    bool is_arena_alloc = is_arena_alloc_call(call.func_name);

    if (is_arena_alloc && result_id != INVALID_VALUE) {
        stats_.total_allocations++;
        stats_.arena_allocations++;
        auto& info = escape_info_[result_id];
        info.may_alias_heap = false; // Arena memory, not standard heap
        info.state = EscapeState::NoEscape;
        info.is_stack_promotable = false; // Already arena-allocated, don't promote
        info.is_arena_allocated = true;
        info.free_can_be_removed = true; // Arena handles deallocation
    } else if (is_heap_alloc && result_id != INVALID_VALUE) {
        stats_.total_allocations++;
        auto& info = escape_info_[result_id];
        info.may_alias_heap = true;
        // Heap allocations start as NoEscape but may be promoted if they don't escape
        info.state = EscapeState::NoEscape;
        info.is_stack_promotable = true;
    }

    // Check if this is a class constructor call
    if (is_constructor_call(call.func_name) && result_id != INVALID_VALUE) {
        stats_.total_allocations++;
        auto& info = escape_info_[result_id];
        info.may_alias_heap = true;
        info.state = EscapeState::NoEscape;
        info.is_stack_promotable = true;
        info.is_class_instance = true;
        info.class_name = extract_class_name(call.func_name);
    }

    // Check if this is a free call that can be removed
    if (!call.args.empty()) {
        analyze_free_removal(call, call.args[0].id);
    }

    // Determine escape severity based on devirtualization info
    // Devirtualized calls are less conservative since we know the exact target
    EscapeState arg_escape_state = EscapeState::ArgEscape;

    if (call.devirt_info.has_value()) {
        // This call was devirtualized - we know the exact method being called
        // We can use less conservative escape analysis since we know the target
        // Still mark as ArgEscape but not GlobalEscape
        arg_escape_state = EscapeState::ArgEscape;
    }

    // Arguments passed to function calls may escape
    for (size_t i = 0; i < call.args.size(); ++i) {
        const auto& arg = call.args[i];
        if (arg.id != INVALID_VALUE) {
            auto arg_info = get_escape_info(arg.id);

            // For method calls on class instances, track more precisely
            // First argument to a method is typically `this`
            if (i == 0 && !is_heap_alloc && !is_constructor_call(call.func_name)) {
                // Check if function name looks like a method (ClassName_method)
                auto underscore = call.func_name.find('_');
                if (underscore != std::string::npos && underscore > 0) {
                    // This is a method call, `this` parameter might escape
                    if (arg_info.is_class_instance) {
                        stats_.method_call_escapes++;
                    }
                }
            }

            mark_escape(arg.id, arg_escape_state);
        }
    }
}

void EscapeAnalysisPass::analyze_method_call(const MethodCallInst& call, ValueId result_id) {
    // Determine if this is a devirtualized call (we know the exact method)
    // Devirtualized calls are less conservative because we can analyze the target
    bool is_devirtualized = call.devirt_info.has_value();

    // Check if this is a simple getter/setter pattern (common in OOP)
    // Getters: return a field value, don't store `this`
    // Setters: store to `this` field, don't escape `this`
    bool is_likely_accessor = false;
    if (is_devirtualized) {
        // Simple heuristic: short method names starting with get_/set_/is_/has_
        // and methods returning primitive types are likely accessors
        const auto& method_name = call.method_name;
        is_likely_accessor =
            method_name.starts_with("get_") || method_name.starts_with("set_") ||
            method_name.starts_with("is_") || method_name.starts_with("has_") ||
            method_name == "area" || method_name == "perimeter" || method_name == "length" ||
            method_name == "size" || method_name == "to_bits" || method_name == "from_bits" ||
            method_name == "compute" || method_name == "update" || method_name == "handle" ||
            method_name == "distance" || method_name == "build";
    }

    // The receiver (`this`) may escape through the method call
    if (call.receiver.id != INVALID_VALUE) {
        auto receiver_info = get_escape_info(call.receiver.id);
        if (receiver_info.is_class_instance) {
            stats_.method_call_escapes++;
        }

        if (is_devirtualized && is_likely_accessor) {
            // Devirtualized accessor methods typically don't escape `this`
            // Keep current escape state (likely NoEscape if local)
        } else if (is_devirtualized) {
            // Devirtualized non-accessor methods - mark as ArgEscape (conservative but not global)
            // The method may store `this` or pass it to other functions
            mark_escape(call.receiver.id, EscapeState::ArgEscape);
        } else {
            // Virtual method calls are conservative - the receiver may be stored
            // or returned by any override in the inheritance hierarchy.
            // Mark as GlobalEscape since we can't track what the virtual method does.
            mark_escape(call.receiver.id, EscapeState::GlobalEscape);
        }
    }

    // Arguments passed to method calls
    for (const auto& arg : call.args) {
        if (arg.id != INVALID_VALUE) {
            if (is_devirtualized && is_likely_accessor) {
                // Accessor methods typically don't escape arguments
                mark_escape(arg.id, EscapeState::ArgEscape);
            } else if (is_devirtualized) {
                // Devirtualized method - less conservative
                mark_escape(arg.id, EscapeState::ArgEscape);
            } else {
                // Virtual dispatch - be conservative
                mark_escape(arg.id, EscapeState::GlobalEscape);
            }
        }
    }

    // Result of method call
    if (result_id != INVALID_VALUE) {
        auto& info = escape_info_[result_id];
        if (is_devirtualized) {
            // Devirtualized call - result may alias heap but not necessarily global
            info.may_alias_heap = true;
            info.may_alias_global = false;
        } else {
            // Virtual call - result may alias anything
            info.may_alias_heap = true;
            info.may_alias_global = true;
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

    // If storing to a class instance field, check if the stored value is a pointer
    // that could escape through the field
    if (ptr_info.is_class_instance) {
        // Storing to a field of a class instance
        // If the stored value is a pointer, it may escape through the instance
        auto value_info = get_escape_info(store.value.id);
        if (value_info.may_alias_heap ||
            store.value.type && std::holds_alternative<MirPointerType>(store.value.type->kind)) {
            stats_.field_store_escapes++;
            mark_escape(store.value.id, EscapeState::GlobalEscape);
        }
    }
}

void EscapeAnalysisPass::analyze_gep(const GetElementPtrInst& gep, ValueId result_id) {
    // GetElementPtr derives a pointer to a field from a base pointer
    // The result may alias the base and inherits its escape state
    auto base_info = get_escape_info(gep.base.id);

    if (result_id != INVALID_VALUE) {
        auto& info = escape_info_[result_id];
        info.may_alias_heap = base_info.may_alias_heap;
        info.may_alias_global = base_info.may_alias_global;

        // If base is a class instance, the GEP result is a field pointer
        if (base_info.is_class_instance) {
            info.is_class_instance = true;
            info.class_name = base_info.class_name;
        }
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
        // Preserve class instance information
    } else {
        EscapeInfo info;
        info.state = state;
        info.is_stack_promotable = (state == EscapeState::NoEscape);
        info.is_class_instance = false;
        info.class_name = "";
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
// Advanced Escape Analysis Helpers
// ============================================================================

auto EscapeAnalysisPass::is_arena_alloc_call(const std::string& func_name) const -> bool {
    // Arena allocation patterns:
    // - Arena_alloc_raw
    // - Arena_alloc
    // - Arena_alloc_slice
    // - ScopedArena_alloc
    return func_name.find("Arena_alloc") != std::string::npos ||
           func_name.find("ScopedArena_alloc") != std::string::npos;
}

void EscapeAnalysisPass::analyze_conditional_branch(const CondBranchTerm& branch,
                                                    [[maybe_unused]] Function& func) {
    // Analyze branch conditions for conditional escapes
    // When a value escapes only in one branch, we can track it as conditional

    // For now, just track that we analyzed a branch
    // A full implementation would:
    // 1. Track which values are used differently in true/false branches
    // 2. Mark values with conditional escapes based on control flow

    if (branch.condition.id == INVALID_VALUE) {
        return;
    }

    // Mark the condition as potentially affecting escapes
    auto& cond_info = escape_info_[branch.condition.id];
    cond_info.state = EscapeState::NoEscape; // Condition itself doesn't escape
}

void EscapeAnalysisPass::track_conditional_escape(ValueId value, ValueId condition,
                                                  EscapeState true_state, EscapeState false_state) {
    if (value == INVALID_VALUE || condition == INVALID_VALUE) {
        return;
    }

    auto it = escape_info_.find(value);
    if (it == escape_info_.end()) {
        return;
    }

    ConditionalEscape ce;
    ce.condition = condition;
    ce.true_state = true_state;
    ce.false_state = false_state;

    it->second.conditional_escapes.push_back(ce);
    stats_.conditional_escapes++;
}

void EscapeAnalysisPass::analyze_free_removal(const CallInst& call, ValueId value) {
    // Check if this is a free/dealloc call
    if (call.func_name != "free" && call.func_name != "dealloc" && call.func_name != "heap_free" &&
        call.func_name != "tml_free") {
        return;
    }

    if (value == INVALID_VALUE) {
        return;
    }

    auto it = escape_info_.find(value);
    if (it == escape_info_.end()) {
        return;
    }

    // If the value was arena-allocated, the free can be removed
    if (it->second.is_arena_allocated) {
        it->second.free_can_be_removed = true;
        stats_.free_removals++;
    }
    // If the value doesn't escape and is stack-promotable, free can be removed
    else if (it->second.state == EscapeState::NoEscape && it->second.is_stack_promotable) {
        it->second.free_can_be_removed = true;
        stats_.free_removals++;
    }
}

auto EscapeAnalysisPass::can_remove_free(ValueId value) const -> bool {
    auto it = escape_info_.find(value);
    if (it != escape_info_.end()) {
        return it->second.free_can_be_removed;
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

// ============================================================================
// EscapeAndPromotePass Implementation
// ============================================================================

auto EscapeAndPromotePass::run_on_function(Function& func) -> bool {
    // First run escape analysis
    escape_pass_.run_on_function(func);

    // Then run stack promotion using the analysis results
    if (!promotion_pass_) {
        promotion_pass_ = std::make_unique<StackPromotionPass>(escape_pass_);
    }

    return promotion_pass_->run_on_function(func);
}

} // namespace tml::mir
