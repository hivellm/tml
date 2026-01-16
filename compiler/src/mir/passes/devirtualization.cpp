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

    std::string direct_name = target_class + "_" + call.method_name;
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

    for (auto& block : func.blocks) {
        if (process_block(block)) {
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

        // Try to devirtualize
        auto [direct_name, reason] = try_devirtualize(method_call);

        switch (reason) {
        case DevirtReason::SealedClass:
            stats_.devirtualized_sealed++;
            break;
        case DevirtReason::ExactType:
            stats_.devirtualized_exact++;
            break;
        case DevirtReason::SingleImpl:
            stats_.devirtualized_single++;
            break;
        case DevirtReason::FinalMethod:
            stats_.devirtualized_final++;
            break;
        case DevirtReason::NoOverride:
            stats_.devirtualized_nonvirtual++;
            break;
        case DevirtReason::NotDevirtualized:
            stats_.not_devirtualized++;
            continue; // Can't devirtualize
        }

        // Convert to direct call
        // Create CallInst instead of MethodCallInst
        CallInst direct_call;
        direct_call.func_name = direct_name;

        // First arg is receiver (this)
        direct_call.args.push_back(method_call.receiver);
        direct_call.arg_types.push_back(nullptr); // ptr type

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

} // namespace tml::mir
