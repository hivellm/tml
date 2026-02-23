TML_MODULE("compiler")

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
    // Method pattern: ClassName_methodName or ClassName__methodName
    // The first parameter is typically `this` for instance methods
    if (func.params.empty()) {
        return;
    }

    // Check if this looks like a method by examining the function name
    // Methods use double underscore (ClassName__methodName) or single underscore for older code
    auto double_underscore_pos = func.name.find("__");
    auto underscore_pos =
        double_underscore_pos != std::string::npos ? double_underscore_pos : func.name.find('_');
    if (underscore_pos == std::string::npos || underscore_pos == 0) {
        return; // Not a method
    }

    // Don't track constructors - they create new instances
    if (is_constructor_call(func.name)) {
        return;
    }

    // Extract potential class name
    std::string potential_class_name = func.name.substr(0, underscore_pos);

    // Validate that this is actually a class method by checking the first parameter type
    // The first parameter should be a struct type matching the class name
    const auto& first_param = func.params[0];
    if (!first_param.type) {
        return; // No type info, can't validate
    }

    // Check if first param is a struct type with matching name
    bool is_valid_method = false;
    if (auto* struct_type = std::get_if<MirStructType>(&first_param.type->kind)) {
        is_valid_method = (struct_type->name == potential_class_name);
    }

    if (!is_valid_method) {
        return; // First parameter is not the expected class type - not a method
    }

    // First parameter is `this` - mark it as potentially escaping through return
    // since we can't track what happens to it precisely
    if (first_param.value_id != INVALID_VALUE) {
        auto& info = escape_info_[first_param.value_id];
        info.state = EscapeState::NoEscape; // Start as no escape
        info.is_class_instance = true;
        info.class_name = potential_class_name;
    }
}

// ============================================================================
// EscapeAnalysisPass Implementation
// ============================================================================

auto EscapeAnalysisPass::run_on_function(Function& func) -> bool {
    // Reset state for this function
    escape_info_.clear();
    conditional_allocs_.clear();
    loop_allocs_.clear();
    loop_headers_.clear();
    block_to_loop_.clear();
    stats_ = Stats{};

    // Initialize all values as NoEscape
    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (inst.result != INVALID_VALUE) {
                escape_info_[inst.result] = EscapeInfo{
                    EscapeState::NoEscape, false, false, false, false, "", false, false, {}};
            }
        }
    }

    // Track `this` parameter for class methods
    track_this_parameter(func);

    // Analyze the function
    analyze_function(func);

    // Propagate escape information
    propagate_escapes(func);

    // Find conditional allocations (phi nodes merging allocations)
    find_conditional_allocations(func);

    // Identify loops and find loop allocations
    identify_loops(func);
    find_loop_allocations(func);

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
    return EscapeInfo{EscapeState::Unknown, false, false, false, false, "", false, false, {}};
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

        // Apply sealed class optimizations
        apply_sealed_class_optimization(info.class_name, info);
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

    // Check if receiver type is a sealed class - enables fast-path analysis
    bool is_sealed = is_sealed_class(call.receiver_type);

    // Check if this is a simple getter/setter pattern (common in OOP)
    // Getters: return a field value, don't store `this`
    // Setters: store to `this` field, don't escape `this`
    bool is_likely_accessor = false;
    if (is_devirtualized || is_sealed) {
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
            stats_.sealed_method_noescapes++;
        } else if (is_sealed) {
            // Sealed class method calls - we know all implementations
            // For sealed classes, method calls don't cause global escape
            // because there are no unknown overrides
            if (is_likely_accessor) {
                // Accessor on sealed class - definitely doesn't escape
                stats_.sealed_method_noescapes++;
            } else {
                // Non-accessor on sealed class - still ArgEscape (better than GlobalEscape)
                mark_escape(call.receiver.id, EscapeState::ArgEscape);
            }
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
            } else if (is_sealed) {
                // Sealed class method - we know all implementations
                // Use ArgEscape instead of GlobalEscape
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
        if (is_devirtualized || is_sealed) {
            // Devirtualized or sealed class call - result may alias heap but not necessarily global
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
            (store.value.type && std::holds_alternative<MirPointerType>(store.value.type->kind))) {
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
// Sealed Class Optimization Helpers
// ============================================================================

auto EscapeAnalysisPass::is_sealed_class(const std::string& class_name) const -> bool {
    if (!module_) {
        return false;
    }
    return module_->is_class_sealed(class_name);
}

auto EscapeAnalysisPass::is_stack_allocatable_class(const std::string& class_name) const -> bool {
    if (!module_) {
        return false;
    }
    return module_->can_stack_allocate(class_name);
}

auto EscapeAnalysisPass::get_class_metadata(const std::string& class_name) const
    -> std::optional<ClassMetadata> {
    if (!module_) {
        return std::nullopt;
    }
    return module_->get_class_metadata(class_name);
}

auto EscapeAnalysisPass::get_conditional_allocations() const
    -> const std::vector<ConditionalAllocation>& {
    return conditional_allocs_;
}

void EscapeAnalysisPass::find_conditional_allocations(Function& func) {
    // Find phi nodes that merge allocations from different branches
    // These can share a single stack slot since only one allocation
    // is active at any time

    for (const auto& block : func.blocks) {
        for (const auto& inst : block.instructions) {
            if (auto* phi = std::get_if<PhiInst>(&inst.inst)) {
                analyze_phi_for_allocations(*phi, inst.result, func);
            }
        }
    }
}

void EscapeAnalysisPass::analyze_phi_for_allocations(const PhiInst& phi, ValueId phi_result,
                                                     [[maybe_unused]] Function& func) {
    // Check if all incoming values are allocations
    bool all_allocations = true;
    bool all_same_class = true;
    std::string common_class_name;
    size_t max_size = 0;

    ConditionalAllocation cond_alloc;
    cond_alloc.phi_result = phi_result;

    for (const auto& [incoming_val, from_block] : phi.incoming) {
        auto it = escape_info_.find(incoming_val.id);
        if (it == escape_info_.end()) {
            all_allocations = false;
            break;
        }

        const auto& info = it->second;

        // Check if this is an allocation that can be promoted
        if (!info.is_stack_promotable && info.state != EscapeState::NoEscape) {
            all_allocations = false;
            break;
        }

        // Track class information
        if (info.is_class_instance) {
            if (common_class_name.empty()) {
                common_class_name = info.class_name;
            } else if (common_class_name != info.class_name) {
                all_same_class = false;
            }
        }

        cond_alloc.alloc_ids.push_back(incoming_val.id);
        cond_alloc.from_blocks.push_back(from_block);
    }

    // Only track if we have multiple incoming allocations
    if (all_allocations && cond_alloc.alloc_ids.size() >= 2) {
        cond_alloc.max_size = max_size;
        cond_alloc.can_share_slot = true; // Conservative for now
        cond_alloc.class_name = all_same_class ? common_class_name : "";

        conditional_allocs_.push_back(cond_alloc);
        stats_.conditional_allocations_found++;

        if (cond_alloc.can_share_slot) {
            stats_.conditional_allocs_shareable++;
        }

        // Mark all allocation ids as stack-promotable if they can share
        for (ValueId alloc_id : cond_alloc.alloc_ids) {
            auto it = escape_info_.find(alloc_id);
            if (it != escape_info_.end()) {
                it->second.is_stack_promotable = true;
            }
        }
    }
}

auto EscapeAnalysisPass::get_loop_allocations() const -> const std::vector<LoopAllocation>& {
    return loop_allocs_;
}

void EscapeAnalysisPass::identify_loops(Function& func) {
    // Simple loop detection using back edges
    // A back edge is an edge from a successor to a predecessor (or self-loop)
    // The target of a back edge is a loop header

    // Build a visited order for each block
    std::unordered_map<uint32_t, size_t> visit_order;
    size_t order = 0;

    for (const auto& block : func.blocks) {
        visit_order[block.id] = order++;
    }

    // Find back edges
    for (const auto& block : func.blocks) {
        for (uint32_t succ_id : block.successors) {
            // Back edge: edge to a block visited earlier (or same block)
            if (visit_order.count(succ_id) > 0 && visit_order[succ_id] <= visit_order[block.id]) {
                // succ_id is a loop header
                loop_headers_.insert(succ_id);
            }
        }
    }

    // Compute block-to-loop mapping using a simple DFS
    // For each loop header, find all blocks that can reach it via back edge
    for (uint32_t header : loop_headers_) {
        // Mark the header itself
        block_to_loop_[header] = header;

        // Find all blocks that are part of this loop
        // A block is in a loop if it's on a path from header to a back edge source
        std::unordered_set<uint32_t> loop_blocks;
        loop_blocks.insert(header);

        // Simple approach: mark all predecessors of header that are dominated by header
        // For simplicity, we'll just mark blocks that have the header as a successor (back edge)
        for (const auto& block : func.blocks) {
            for (uint32_t succ_id : block.successors) {
                if (succ_id == header && block.id != header) {
                    // This block has a back edge to header
                    // Mark all blocks between header and this block as in the loop
                    // Simplified: just mark this block
                    if (block_to_loop_.count(block.id) == 0) {
                        block_to_loop_[block.id] = header;
                    }
                }
            }
        }
    }
}

void EscapeAnalysisPass::find_loop_allocations(Function& func) {
    // Find allocations inside loops
    for (const auto& block : func.blocks) {
        uint32_t loop_header = get_loop_header(block.id);
        if (loop_header == 0 && !is_in_loop(block.id)) {
            continue; // Block is not in a loop
        }

        for (const auto& inst : block.instructions) {
            // Check if this is an allocation
            bool is_alloc = false;
            std::string class_name;
            size_t est_size = 64; // Default estimate

            if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                is_alloc = call->func_name == "alloc" || call->func_name == "heap_alloc" ||
                           call->func_name == "Heap::new" || call->func_name == "tml_alloc" ||
                           is_constructor_call(call->func_name);
                if (is_constructor_call(call->func_name)) {
                    class_name = extract_class_name(call->func_name);
                }
            } else if (std::holds_alternative<AllocaInst>(inst.inst)) {
                is_alloc = true;
            }

            if (is_alloc && inst.result != INVALID_VALUE) {
                LoopAllocation loop_alloc;
                loop_alloc.alloc_id = inst.result;
                loop_alloc.loop_header = loop_header != 0 ? loop_header : block.id;
                loop_alloc.alloc_block = block.id;
                loop_alloc.estimated_size = est_size;
                loop_alloc.class_name = class_name;

                // Analyze if the allocation escapes the iteration
                analyze_loop_escape(loop_alloc, func);

                loop_allocs_.push_back(loop_alloc);
                stats_.loop_allocations_found++;

                if (!loop_alloc.escapes_iteration) {
                    stats_.loop_allocs_promotable++;
                }
                if (loop_alloc.is_loop_invariant) {
                    stats_.loop_allocs_hoistable++;
                }
            }
        }
    }
}

auto EscapeAnalysisPass::is_in_loop(uint32_t block_id) const -> bool {
    return block_to_loop_.count(block_id) > 0 || loop_headers_.count(block_id) > 0;
}

auto EscapeAnalysisPass::get_loop_header(uint32_t block_id) const -> uint32_t {
    auto it = block_to_loop_.find(block_id);
    if (it != block_to_loop_.end()) {
        return it->second;
    }
    if (loop_headers_.count(block_id) > 0) {
        return block_id;
    }
    return 0;
}

void EscapeAnalysisPass::analyze_loop_escape(LoopAllocation& loop_alloc,
                                             [[maybe_unused]] Function& func) {
    // Check if the allocation escapes the current loop iteration
    // An allocation escapes if:
    // 1. It's stored to a location outside the loop
    // 2. It's passed to a function that might store it
    // 3. It's used across loop iterations (phi node with the allocation)

    auto info = get_escape_info(loop_alloc.alloc_id);

    // If the value escapes globally, it definitely escapes the iteration
    if (info.state == EscapeState::GlobalEscape || info.state == EscapeState::ReturnEscape) {
        loop_alloc.escapes_iteration = true;
        return;
    }

    // If passed to a function, conservatively assume it escapes
    if (info.state == EscapeState::ArgEscape) {
        loop_alloc.escapes_iteration = true;
        return;
    }

    // Check if allocation is used in a phi node that spans iterations
    // (This would require more sophisticated analysis)
    // For now, we're conservative and allow promotion only for NoEscape

    loop_alloc.escapes_iteration = false;

    // Check for loop-invariant allocation (could be hoisted)
    // An allocation is loop-invariant if its size/type doesn't depend on loop variables
    // For simplicity, we mark all non-escaping allocations as potentially hoistable
    loop_alloc.is_loop_invariant = !loop_alloc.escapes_iteration;
}

void EscapeAnalysisPass::apply_sealed_class_optimization(const std::string& class_name,
                                                         EscapeInfo& info) {
    auto metadata = get_class_metadata(class_name);
    if (!metadata) {
        return;
    }

    // Track sealed class statistics
    if (metadata->is_sealed) {
        stats_.sealed_class_instances++;

        // Sealed classes with stack_allocatable flag can be stack-promoted
        if (metadata->stack_allocatable) {
            info.is_stack_promotable = true;
            stats_.sealed_class_promotable++;
        }

        // For sealed classes, method calls don't cause global escape
        // because we know all possible implementations
        // Keep the info state as NoEscape if it was NoEscape
    }

    // Value classes (no vtable) are always stack-promotable
    if (metadata->is_value) {
        info.is_stack_promotable = true;
    }
}

// ============================================================================
// StackPromotionPass Implementation
// ============================================================================

auto StackPromotionPass::run_on_function(Function& func) -> bool {
    bool changed = false;
    stats_ = Stats{};
    promoted_values_.clear();
    shared_stack_slots_.clear();
    hoisted_loop_allocs_.clear();

    // First, handle conditional allocations (phi nodes merging allocations)
    promote_conditional_allocations(func);

    // Handle loop allocations (promote to stack, optionally hoist)
    promote_loop_allocations(func);

    // Find and promote stack-promotable allocations
    auto promotable = escape_analysis_.get_stack_promotable();

    for (auto& block : func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& inst = block.instructions[i];

            // Check if this instruction result is promotable
            if (inst.result != INVALID_VALUE) {
                // Skip if already promoted as part of conditional allocation
                if (promoted_values_.count(inst.result) > 0) {
                    continue;
                }

                auto it = std::find(promotable.begin(), promotable.end(), inst.result);
                if (it != promotable.end()) {
                    // Mark instruction as stack-eligible
                    mark_stack_eligible(inst);

                    if (promote_allocation(block, i, func)) {
                        promoted_values_.insert(inst.result);
                        changed = true;

                        // Check if this is a class instance that needs destructor
                        auto info = escape_analysis_.get_escape_info(inst.result);
                        if (info.is_class_instance && !info.class_name.empty()) {
                            insert_destructor(func, inst.result, info.class_name);
                        }
                    }
                }
            }
        }
    }

    // Remove free calls for promoted allocations
    for (ValueId promoted : promoted_values_) {
        remove_free_calls(func, promoted);
    }

    return changed || !shared_stack_slots_.empty();
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

void StackPromotionPass::mark_stack_eligible(InstructionData& inst) {
    std::visit(
        [](auto& i) {
            using T = std::decay_t<decltype(i)>;

            if constexpr (std::is_same_v<T, CallInst>) {
                i.is_stack_eligible = true;
            } else if constexpr (std::is_same_v<T, AllocaInst>) {
                i.is_stack_eligible = true;
            } else if constexpr (std::is_same_v<T, StructInitInst>) {
                i.is_stack_eligible = true;
            }
            // Other instruction types don't have stack_eligible flag
        },
        inst.inst);
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

void StackPromotionPass::remove_free_calls(Function& func, ValueId promoted_value) {
    // Find and remove free/dealloc calls for a promoted allocation
    // These calls are no longer needed since the object is stack-allocated
    for (auto& block : func.blocks) {
        std::vector<size_t> indices_to_remove;

        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& inst = block.instructions[i];

            if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                // Check if this is a free/dealloc call
                bool is_free_call = call->func_name == "free" || call->func_name == "dealloc" ||
                                    call->func_name == "heap_free" ||
                                    call->func_name == "tml_free" || call->func_name == "drop";

                if (is_free_call && !call->args.empty()) {
                    // Check if freeing the promoted value
                    if (call->args[0].id == promoted_value) {
                        indices_to_remove.push_back(i);
                        stats_.free_calls_removed++;
                    }
                }
            }
        }

        // Remove instructions in reverse order to maintain indices
        for (auto it = indices_to_remove.rbegin(); it != indices_to_remove.rend(); ++it) {
            block.instructions.erase(block.instructions.begin() + static_cast<std::ptrdiff_t>(*it));
        }
    }
}

void StackPromotionPass::insert_destructor(Function& func, ValueId value,
                                           const std::string& class_name) {
    // Find all return paths and insert destructor calls before them
    // For stack-allocated class instances, we need to call their destructor
    // at scope exit to ensure proper cleanup

    std::string destructor_name = "drop_" + class_name;

    for (auto& block : func.blocks) {
        // Check if block has a return terminator
        if (!block.terminator) {
            continue;
        }

        if (!std::holds_alternative<ReturnTerm>(*block.terminator)) {
            continue;
        }

        // Insert destructor call before return
        // Create a call instruction to the destructor
        CallInst dtor_call;
        dtor_call.func_name = destructor_name;

        // Pass `this` (the promoted value) as argument
        Value this_arg;
        this_arg.id = value;
        this_arg.type = nullptr; // Type will be pointer to class
        dtor_call.args.push_back(this_arg);

        // Create instruction data for the destructor call
        InstructionData dtor_inst;
        dtor_inst.inst = dtor_call;
        dtor_inst.result = INVALID_VALUE; // Destructor returns void

        // Insert at end of block (before terminator)
        block.instructions.push_back(dtor_inst);

        stats_.destructors_inserted++;
    }
}

void StackPromotionPass::promote_conditional_allocations(Function& func) {
    // Handle conditional allocations that can share stack slots
    // For each phi node that merges allocations from different branches,
    // just mark the individual allocations as stack-eligible.
    // NOTE: Creating shared_slot allocas is disabled because it requires complex
    // phi node rewriting to avoid ID conflicts. Individual branches are still
    // promoted by the regular promotion logic.

    const auto& cond_allocs = escape_analysis_.get_conditional_allocations();

    for (const auto& cond_alloc : cond_allocs) {
        if (!cond_alloc.can_share_slot || cond_alloc.alloc_ids.empty()) {
            continue;
        }

        // Mark all allocations as promoted (stack-eligible)
        // Each allocation in each branch will be handled individually
        for (ValueId alloc_id : cond_alloc.alloc_ids) {
            promoted_values_.insert(alloc_id);

            // Find and mark the original allocation instruction as stack-eligible
            for (auto& block : func.blocks) {
                for (auto& inst : block.instructions) {
                    if (inst.result == alloc_id) {
                        mark_stack_eligible(inst);
                        break;
                    }
                }
            }
        }

        stats_.conditional_allocs_promoted += cond_alloc.alloc_ids.size();
    }
}

void StackPromotionPass::promote_loop_allocations(Function& func) {
    // Handle loop allocations that can be promoted to stack
    // For allocations that don't escape the loop iteration,
    // mark them as stack-eligible. The codegen will handle stack allocation.
    // NOTE: Loop hoisting is disabled because inserting allocas with the same ID
    // as the original allocation creates duplicate value definitions.

    const auto& loop_allocs = escape_analysis_.get_loop_allocations();

    for (const auto& loop_alloc : loop_allocs) {
        if (loop_alloc.escapes_iteration) {
            continue; // Can't promote if it escapes
        }

        // Check if already promoted
        if (promoted_values_.count(loop_alloc.alloc_id) > 0 ||
            hoisted_loop_allocs_.count(loop_alloc.alloc_id) > 0) {
            continue;
        }

        // Mark as stack-eligible but keep allocation in place
        // The allocation instruction will be modified to use stack instead of heap
        {
            // Just mark as stack-eligible but keep in place
            // The allocation will happen each iteration but on stack

            for (auto& block : func.blocks) {
                if (block.id != loop_alloc.alloc_block) {
                    continue;
                }

                for (auto& inst : block.instructions) {
                    if (inst.result == loop_alloc.alloc_id) {
                        mark_stack_eligible(inst);

                        // Find and replace the allocation call with alloca
                        if (auto* call = std::get_if<CallInst>(&inst.inst)) {
                            if (call->func_name == "alloc" || call->func_name == "heap_alloc" ||
                                call->func_name == "Heap::new" || call->func_name == "tml_alloc") {
                                AllocaInst alloca_inst;
                                alloca_inst.alloc_type = call->return_type;
                                alloca_inst.name = "loop_promoted_" + std::to_string(inst.result);
                                alloca_inst.is_stack_eligible = true;
                                inst.inst = alloca_inst;

                                promoted_values_.insert(loop_alloc.alloc_id);
                                stats_.loop_allocs_promoted++;
                                stats_.bytes_saved += loop_alloc.estimated_size;

                                // Insert destructor for class instances at loop exit
                                if (!loop_alloc.class_name.empty()) {
                                    // Note: For non-hoisted allocations, destructor should be
                                    // inserted at the end of each iteration, not function exit
                                    // This is a simplification - full implementation would
                                    // track loop exits and insert destructors there
                                    insert_destructor(func, loop_alloc.alloc_id,
                                                      loop_alloc.class_name);
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
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
