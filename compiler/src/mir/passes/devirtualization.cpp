//! # Devirtualization Pass Implementation
//!
//! Converts virtual method calls to direct calls when possible.

#include "mir/passes/devirtualization.hpp"

#include <algorithm>
#include <queue>

namespace tml::mir {

// ============================================================================
// Class Hierarchy Analysis
// ============================================================================

void DevirtualizationPass::build_class_hierarchy() const {
    if (hierarchy_built_)
        return;

    // First pass: collect all classes and their direct relationships
    for (const auto& [name, class_def] : env_.all_classes()) {
        ClassHierarchyInfo info;
        info.name = name;
        info.base_class = class_def.base_class;
        info.interfaces = class_def.interfaces;
        info.is_sealed = class_def.is_sealed;
        info.is_abstract = class_def.is_abstract;

        // Collect final methods
        for (const auto& method : class_def.methods) {
            if (method.is_final) {
                info.final_methods.insert(method.sig.name);
            }
        }

        class_hierarchy_[name] = std::move(info);
    }

    // Second pass: build subclass relationships
    for (auto& [name, info] : class_hierarchy_) {
        if (info.base_class) {
            auto it = class_hierarchy_.find(*info.base_class);
            if (it != class_hierarchy_.end()) {
                it->second.subclasses.insert(name);
            }
        }
    }

    // Third pass: compute transitive subclasses
    compute_transitive_subclasses();

    hierarchy_built_ = true;
}

void DevirtualizationPass::compute_transitive_subclasses() const {
    // Use BFS from each class to find all transitive subclasses
    for (auto& [name, info] : class_hierarchy_) {
        std::queue<std::string> to_visit;
        for (const auto& sub : info.subclasses) {
            to_visit.push(sub);
            info.all_subclasses.insert(sub);
        }

        while (!to_visit.empty()) {
            std::string current = to_visit.front();
            to_visit.pop();

            auto it = class_hierarchy_.find(current);
            if (it != class_hierarchy_.end()) {
                for (const auto& sub : it->second.subclasses) {
                    if (info.all_subclasses.insert(sub).second) {
                        to_visit.push(sub);
                    }
                }
            }
        }
    }
}

auto DevirtualizationPass::get_class_info(const std::string& class_name) const
    -> const ClassHierarchyInfo* {
    // Build class hierarchy if not already done
    build_class_hierarchy();

    auto it = class_hierarchy_.find(class_name);
    return it != class_hierarchy_.end() ? &it->second : nullptr;
}

// ============================================================================
// Type Queries
// ============================================================================

auto DevirtualizationPass::is_sealed_class(const std::string& class_name) const -> bool {
    auto info = get_class_info(class_name);
    return info && info->is_sealed;
}

auto DevirtualizationPass::is_virtual_method(const std::string& class_name,
                                             const std::string& method_name) const -> bool {
    auto class_def = env_.lookup_class(class_name);
    if (!class_def)
        return false;

    for (const auto& method : class_def->methods) {
        if (method.sig.name == method_name) {
            // A method is virtual if explicitly marked virtual OR if it's an override
            // (override methods participate in virtual dispatch)
            if (method.is_virtual || method.is_override) {
                return true;
            }
            // If we found the method but it's neither virtual nor override,
            // it's a non-virtual method that shadows a potential parent method
            return false;
        }
    }

    // Check parent class if method not found in this class
    if (class_def->base_class) {
        return is_virtual_method(*class_def->base_class, method_name);
    }

    return false;
}

auto DevirtualizationPass::is_final_method(const std::string& class_name,
                                           const std::string& method_name) const -> bool {
    // Check if the method is marked final in this class
    auto info = get_class_info(class_name);
    if (info && info->is_method_final(method_name)) {
        return true;
    }

    // Check parent class - final methods are inherited
    auto class_def = env_.lookup_class(class_name);
    if (class_def && class_def->base_class) {
        return is_final_method(*class_def->base_class, method_name);
    }

    return false;
}

auto DevirtualizationPass::get_single_implementation(const std::string& class_name,
                                                     const std::string& method_name) const
    -> std::optional<std::string> {
    auto info = get_class_info(class_name);
    if (!info)
        return std::nullopt;

    // If the class itself implements the method and has no subclasses that override it,
    // return this class as the single implementation
    auto class_def = env_.lookup_class(class_name);
    if (!class_def)
        return std::nullopt;

    // Check if this class has the method
    bool has_method = false;
    for (const auto& method : class_def->methods) {
        if (method.sig.name == method_name) {
            has_method = true;
            break;
        }
    }

    if (!has_method && !class_def->base_class) {
        return std::nullopt;
    }

    // If sealed or leaf class with the method, it's the single implementation
    if ((info->is_sealed || info->is_leaf()) && has_method) {
        return class_name;
    }

    // Check if only one subclass implements it
    std::optional<std::string> single_impl;
    if (has_method && !info->is_abstract) {
        single_impl = class_name;
    }

    for (const auto& subclass : info->all_subclasses) {
        auto sub_def = env_.lookup_class(subclass);
        if (!sub_def)
            continue;

        for (const auto& method : sub_def->methods) {
            if (method.sig.name == method_name && method.is_override) {
                if (single_impl && *single_impl != class_name) {
                    // More than one implementation
                    return std::nullopt;
                }
                single_impl = subclass;
            }
        }
    }

    return single_impl;
}

// ============================================================================
// Devirtualization Logic
// ============================================================================

auto DevirtualizationPass::can_devirtualize(const std::string& receiver_type,
                                            const std::string& method_name) const -> DevirtReason {
    // Build class hierarchy if not already done
    build_class_hierarchy();

    // Check if it's even a class type
    auto class_def = env_.lookup_class(receiver_type);
    if (!class_def) {
        return DevirtReason::NotDevirtualized;
    }

    // Check if method is virtual at all
    if (!is_virtual_method(receiver_type, method_name)) {
        return DevirtReason::NoOverride;
    }

    // Final method: can always devirtualize (cannot be overridden)
    if (is_final_method(receiver_type, method_name)) {
        return DevirtReason::FinalMethod;
    }

    // Sealed class: always devirtualizable
    if (is_sealed_class(receiver_type)) {
        return DevirtReason::SealedClass;
    }

    // Leaf class (no subclasses): devirtualizable
    auto info = get_class_info(receiver_type);
    if (info && info->is_leaf()) {
        return DevirtReason::ExactType;
    }

    // Check for single implementation
    auto single_impl = get_single_implementation(receiver_type, method_name);
    if (single_impl) {
        return DevirtReason::SingleImpl;
    }

    return DevirtReason::NotDevirtualized;
}

auto DevirtualizationPass::try_devirtualize(const MethodCallInst& call) const
    -> std::pair<std::string, DevirtReason> {
    DevirtReason reason = can_devirtualize(call.receiver_type, call.method_name);

    if (reason == DevirtReason::NotDevirtualized) {
        return {"", reason};
    }

    // Build direct function name
    // Format: @tml_<ClassName>_<method_name>
    std::string target_class = call.receiver_type;

    // For single implementation, use the implementing class
    if (reason == DevirtReason::SingleImpl) {
        auto impl = get_single_implementation(call.receiver_type, call.method_name);
        if (impl) {
            target_class = *impl;
        }
    }

    std::string direct_name = target_class + "__" + call.method_name;
    return {direct_name, reason};
}

// ============================================================================
// Pass Execution
// ============================================================================

auto DevirtualizationPass::run(Module& module) -> bool {
    // Build class hierarchy if not already done
    build_class_hierarchy();

    bool changed = false;

    // Process each function
    for (auto& func : module.functions) {
        if (process_function(func)) {
            changed = true;
        }
    }

    return changed;
}

auto DevirtualizationPass::process_function(Function& func) -> bool {
    bool changed = false;

    // Clear type narrowing state from previous functions
    block_type_narrowing_.clear();
    current_narrowing_.clear();

    // First pass: analyze branch narrowing for all blocks
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        analyze_branch_narrowing(func, i);
    }

    // Second pass: process blocks with narrowing info
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        // Load narrowing info for this block
        auto it = block_type_narrowing_.find(i);
        if (it != block_type_narrowing_.end()) {
            current_narrowing_ = it->second;
        } else {
            current_narrowing_.clear();
        }

        // Propagate narrowing to successors (for next iteration)
        propagate_narrowing_to_successors(func.blocks[i], i);

        // Process the block
        if (process_block(func.blocks[i])) {
            changed = true;
        }
    }

    return changed;
}

auto DevirtualizationPass::process_block(BasicBlock& block) -> bool {
    bool changed = false;

    for (auto& inst_data : block.instructions) {
        // Check if this is a method call
        if (!std::holds_alternative<MethodCallInst>(inst_data.inst)) {
            continue;
        }

        auto& method_call = std::get<MethodCallInst>(inst_data.inst);
        stats_.method_calls_analyzed++;

        // Check for type narrowing on the receiver
        std::string effective_type = method_call.receiver_type;
        bool has_narrowing = false;

        auto narrowed = get_narrowed_type(method_call.receiver.id);
        if (narrowed) {
            effective_type = *narrowed;
            has_narrowing = true;
        }

        // Try to devirtualize with effective type
        auto [direct_name, reason] = try_devirtualize(
            MethodCallInst{method_call.receiver, effective_type, method_call.method_name,
                           method_call.args, method_call.arg_types, method_call.return_type});

        // Track narrowing-enabled devirtualization separately
        if (has_narrowing && reason != DevirtReason::NotDevirtualized) {
            stats_.devirtualized_narrowing++;
        }

        switch (reason) {
        case DevirtReason::SealedClass:
            if (!has_narrowing)
                stats_.devirtualized_sealed++;
            break;
        case DevirtReason::ExactType:
            if (!has_narrowing)
                stats_.devirtualized_exact++;
            break;
        case DevirtReason::SingleImpl:
            if (!has_narrowing)
                stats_.devirtualized_single++;
            break;
        case DevirtReason::FinalMethod:
            if (!has_narrowing)
                stats_.devirtualized_final++;
            break;
        case DevirtReason::NoOverride:
            if (!has_narrowing)
                stats_.devirtualized_nonvirtual++;
            break;
        case DevirtReason::TypeNarrowing:
            // Already counted above
            break;
        case DevirtReason::NotDevirtualized:
            stats_.not_devirtualized++;
            continue; // Can't devirtualize
        }

        // Convert to direct call
        // Create CallInst instead of MethodCallInst
        CallInst direct_call;
        direct_call.func_name = direct_name;

        // First arg is receiver (this) - must be ptr type
        direct_call.args.push_back(method_call.receiver);
        direct_call.arg_types.push_back(make_ptr_type());

        // Add remaining args
        for (size_t i = 0; i < method_call.args.size(); ++i) {
            direct_call.args.push_back(method_call.args[i]);
            if (i < method_call.arg_types.size()) {
                direct_call.arg_types.push_back(method_call.arg_types[i]);
            }
        }

        direct_call.return_type = method_call.return_type;

        // Set devirtualization info for inlining pass
        DevirtInfo info;
        info.original_class = method_call.receiver_type;
        info.method_name = method_call.method_name;
        info.from_sealed_class = (reason == DevirtReason::SealedClass);
        info.from_exact_type = (reason == DevirtReason::ExactType);
        info.from_single_impl = (reason == DevirtReason::SingleImpl);
        info.from_final_method = (reason == DevirtReason::FinalMethod);
        direct_call.devirt_info = info;

        // Replace instruction
        inst_data.inst = direct_call;
        changed = true;
    }

    return changed;
}

// ============================================================================
// Type Narrowing Analysis
// ============================================================================

void DevirtualizationPass::analyze_branch_narrowing(const Function& func, size_t block_idx) {
    if (block_idx >= func.blocks.size())
        return;

    const auto& block = func.blocks[block_idx];

    // Track exact types from constructor calls within this block
    // This enables devirtualization when we know the exact runtime type
    for (const auto& inst : block.instructions) {
        // Check for constructor calls (::create, ::new patterns in function names)
        if (auto* call = std::get_if<CallInst>(&inst.inst)) {
            const std::string& func_name = call->func_name;

            // Look for constructor patterns: ClassName_create or ClassName_new
            size_t separator_pos = func_name.rfind("_create");
            if (separator_pos == std::string::npos) {
                separator_pos = func_name.rfind("_new");
            }

            if (separator_pos != std::string::npos && separator_pos > 0) {
                // Extract class name from function name
                std::string class_name = func_name.substr(0, separator_pos);

                // Remove potential module prefix (e.g., "tml_" or "module_")
                size_t underscore_pos = class_name.find('_');
                if (underscore_pos != std::string::npos &&
                    underscore_pos < class_name.length() - 1) {
                    // Check if what remains is a known class
                    std::string potential_name = class_name.substr(underscore_pos + 1);
                    if (get_class_info(potential_name)) {
                        class_name = potential_name;
                    }
                }

                // Check if this is a known class
                if (get_class_info(class_name)) {
                    TypeNarrowingInfo narrow_info;
                    narrow_info.value = inst.result;
                    narrow_info.original_type = class_name;
                    narrow_info.narrowed_type = class_name;
                    narrow_info.is_exact = true; // Constructor always creates exact type

                    block_type_narrowing_[block_idx][inst.result] = narrow_info;
                }
            }
        }

        // Also track struct initialization that creates class instances
        if (auto* struct_init = std::get_if<StructInitInst>(&inst.inst)) {
            const std::string& struct_name = struct_init->struct_name;

            // Remove "class." prefix if present
            std::string class_name = struct_name;
            if (class_name.find("class.") == 0) {
                class_name = class_name.substr(6);
            }

            // Check if this is a class struct
            if (get_class_info(class_name)) {
                TypeNarrowingInfo narrow_info;
                narrow_info.value = inst.result;
                narrow_info.original_type = class_name;
                narrow_info.narrowed_type = class_name;
                narrow_info.is_exact = true;

                block_type_narrowing_[block_idx][inst.result] = narrow_info;
            }
        }
    }

    // Analyze "is T" type check patterns for `when` expression narrowing
    // Pattern: binary comparison against vtable pointer followed by conditional branch
    // When we see: if (obj.vtable == @vtable.Dog) then block_true else block_false
    // We can narrow the type of obj to Dog in block_true
    analyze_when_type_checks(func, block_idx);
}

void DevirtualizationPass::analyze_when_type_checks(const Function& func, size_t block_idx) {
    if (block_idx >= func.blocks.size())
        return;

    const auto& block = func.blocks[block_idx];

    // Look for patterns that indicate "is T" checks:
    // 1. Load vtable pointer from object
    // 2. Compare with known vtable (BinaryInst with Eq)
    // 3. Conditional branch based on comparison result

    // Track potential vtable loads: value_id -> object being loaded from
    std::unordered_map<ValueId, ValueId> vtable_load_sources;

    // Track comparison results: condition_value -> (object, class_name)
    std::unordered_map<ValueId, std::pair<ValueId, std::string>> type_check_conditions;

    for (const auto& inst : block.instructions) {
        // Pattern 1: Load instruction that loads from a class object (vtable ptr)
        if (auto* load = std::get_if<LoadInst>(&inst.inst)) {
            // The source of a vtable load is typically a GEP to field 0
            vtable_load_sources[inst.result] = load->ptr.id;
        }

        // Pattern 2: Binary comparison - track equality checks that might be type checks
        // Full implementation would trace GlobalRef values for vtable comparisons
        if (auto* binary = std::get_if<BinaryInst>(&inst.inst)) {
            if (binary->op == BinOp::Eq || binary->op == BinOp::Ne) {
                // Check if left operand is a vtable load
                auto left_it = vtable_load_sources.find(binary->left.id);
                if (left_it != vtable_load_sources.end()) {
                    // This comparison involves a vtable - potentially a type check
                    // For now, we mark this as a potential type narrowing point
                    // A full implementation would track the constant operand to extract class name
                    (void)type_check_conditions;
                }
            }
        }
    }

    // Now check if the block ends with a conditional branch using one of our type checks
    if (block.terminator.has_value()) {
        if (auto* cond_br = std::get_if<CondBranchTerm>(&*block.terminator)) {
            auto check_it = type_check_conditions.find(cond_br->condition.id);
            if (check_it != type_check_conditions.end()) {
                ValueId object_id = check_it->second.first;
                const std::string& class_name = check_it->second.second;

                // In the true branch, the object has the checked type
                TypeNarrowingInfo narrow_info;
                narrow_info.value = object_id;
                narrow_info.original_type = ""; // Unknown original type
                narrow_info.narrowed_type = class_name;
                narrow_info.is_exact = true; // vtable comparison gives exact type

                // Add narrowing for true block
                block_type_narrowing_[cond_br->true_block][object_id] = narrow_info;
            }
        }
    }
}

void DevirtualizationPass::propagate_narrowing_to_successors(const BasicBlock& block,
                                                             size_t block_idx) {
    // Get successors from terminator
    std::vector<size_t> successors;

    // Extract successors based on terminator type
    if (block.terminator.has_value()) {
        const auto& term = *block.terminator;
        if (auto* br = std::get_if<BranchTerm>(&term)) {
            successors.push_back(br->target);
        } else if (auto* cond_br = std::get_if<CondBranchTerm>(&term)) {
            successors.push_back(cond_br->true_block);
            successors.push_back(cond_br->false_block);
        } else if (auto* sw = std::get_if<SwitchTerm>(&term)) {
            successors.push_back(sw->default_block);
            for (const auto& sw_case : sw->cases) {
                successors.push_back(sw_case.second);
            }
        }
        // ReturnTerm and UnreachableTerm have no successors
    }

    // Propagate current narrowing to successors that don't have their own narrowing
    for (size_t succ : successors) {
        auto& succ_narrowing = block_type_narrowing_[succ];
        for (const auto& [value_id, info] : current_narrowing_) {
            if (succ_narrowing.find(value_id) == succ_narrowing.end()) {
                // Only propagate if successor doesn't already have narrowing
                // (branch-specific narrowing takes priority)
                succ_narrowing[value_id] = info;
            }
        }
    }

    (void)block_idx;
}

auto DevirtualizationPass::get_narrowed_type(ValueId value) const -> std::optional<std::string> {
    auto it = current_narrowing_.find(value);
    if (it != current_narrowing_.end()) {
        return it->second.narrowed_type;
    }
    return std::nullopt;
}

auto DevirtualizationPass::is_exact_type(ValueId value) const -> bool {
    auto it = current_narrowing_.find(value);
    if (it != current_narrowing_.end()) {
        return it->second.is_exact;
    }
    return false;
}

// ============================================================================
// Whole-Program Analysis (Phase 2.4.3)
// ============================================================================

void DevirtualizationPass::build_whole_program_hierarchy() const {
    if (!whole_program_config_.enabled) {
        return;
    }

    // Build standard hierarchy first
    build_class_hierarchy();

    // Collect all classes from all modules
    whole_program_classes_.clear();
    for (const auto& [name, _] : class_hierarchy_) {
        whole_program_classes_.insert(name);
    }

    // In whole-program mode, we can be more aggressive about devirtualization
    // because we know all possible implementations
    // This enables devirtualization for leaf classes even if they're not sealed
}

auto DevirtualizationPass::is_safe_for_whole_program(const std::string& class_name) const -> bool {
    if (!whole_program_config_.enabled) {
        return false;
    }

    // Check if class is excluded
    for (const auto& excluded : whole_program_config_.excluded_classes) {
        if (class_name == excluded) {
            return false;
        }
    }

    // Check if class is in our whole-program analysis
    if (whole_program_classes_.count(class_name) == 0) {
        return false;
    }

    // If dynamic loading invalidation is enabled, don't devirtualize
    // non-sealed classes that could have subclasses loaded later
    if (whole_program_config_.invalidate_on_dynamic_load) {
        auto info = get_class_info(class_name);
        if (info && !info->is_sealed && !info->is_leaf()) {
            return false;
        }
    }

    return true;
}

void DevirtualizationPass::invalidate_hierarchy() {
    hierarchy_built_ = false;
    class_hierarchy_.clear();
    whole_program_classes_.clear();
}

// ============================================================================
// Profile-Guided Optimization (Phase 3.1.5/3.1.6)
// ============================================================================

auto DevirtualizationPass::get_profile_for_call(const std::string& call_site,
                                                const std::string& method_name) const
    -> std::optional<TypeProfileData> {
    if (!has_profile_data_) {
        return std::nullopt;
    }

    for (const auto& data : profile_data_.call_sites) {
        if (data.call_site == call_site && data.method_name == method_name) {
            return data;
        }
    }

    return std::nullopt;
}

void DevirtualizationPass::record_type_observation(const std::string& call_site,
                                                   const std::string& method_name,
                                                   const std::string& receiver_type) {
    if (!instrumentation_enabled_) {
        return;
    }

    // Find or create the call site entry
    TypeProfileData* entry = nullptr;
    for (auto& data : instrumentation_data_.call_sites) {
        if (data.call_site == call_site && data.method_name == method_name) {
            entry = &data;
            break;
        }
    }

    if (!entry) {
        instrumentation_data_.call_sites.push_back({call_site, method_name, {}});
        entry = &instrumentation_data_.call_sites.back();
    }

    // Increment type count
    entry->type_counts[receiver_type]++;
}

auto DevirtualizationPass::profile_guided_devirt(const std::string& call_site,
                                                 const std::string& method_name) const
    -> std::optional<std::pair<std::string, float>> {
    auto profile = get_profile_for_call(call_site, method_name);
    if (!profile) {
        return std::nullopt;
    }

    auto [most_frequent, confidence] = profile->most_frequent_type();
    if (most_frequent.empty() || confidence < 0.5f) {
        // Need at least 50% confidence for speculative devirtualization
        return std::nullopt;
    }

    return {{most_frequent, confidence}};
}

// ============================================================================
// TypeProfileFile Implementation
// ============================================================================

auto TypeProfileFile::load(const std::string& path) -> std::optional<TypeProfileFile> {
    // Simplified implementation - in production, use proper JSON parsing
    (void)path;
    // TODO: Implement JSON parsing for profile data
    return std::nullopt;
}

auto TypeProfileFile::save(const std::string& path) const -> bool {
    // Simplified implementation - in production, use proper JSON serialization
    (void)path;
    // TODO: Implement JSON serialization for profile data
    return false;
}

} // namespace tml::mir
