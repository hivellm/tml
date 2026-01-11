//! # LLVM IR Generator - Dynamic Dispatch
//!
//! This file implements vtables for `dyn Behavior` types.
//!
//! ## Vtable Structure
//!
//! Each `impl Behavior for Type` generates a vtable:
//! ```llvm
//! @vtable.Point.Display = global [1 x ptr] [ptr @Point_display]
//! ```
//!
//! ## Dyn Type Layout
//!
//! `dyn Behavior` is a fat pointer: `{ data: ptr, vtable: ptr }`
//!
//! ## Key Methods
//!
//! | Method            | Purpose                              |
//! |-------------------|--------------------------------------|
//! | `register_impl`   | Register impl for vtable generation  |
//! | `emit_dyn_type`   | Emit fat pointer struct              |
//! | `get_vtable`      | Get vtable name for type+behavior    |
//! | `emit_vtables`    | Emit all registered vtables          |
//!
//! ## Method Order
//!
//! `behavior_method_order_` ensures consistent vtable slot ordering
//! across all implementations of a behavior.

#include "codegen/llvm_ir_gen.hpp"

namespace tml::codegen {

// ============ Vtable Support ============

void LLVMIRGen::register_impl(const parser::ImplDecl* impl) {
    pending_impls_.push_back(impl);

    // Eagerly populate behavior_method_order_ and vtables_ for dyn dispatch
    std::string behavior_name;
    if (impl->trait_type && impl->trait_type->is<parser::NamedType>()) {
        const auto& named = impl->trait_type->as<parser::NamedType>();
        if (!named.path.segments.empty()) {
            behavior_name = named.path.segments.back();
        }
    }
    if (!behavior_name.empty()) {

        // Populate behavior_method_order_ only once per behavior
        if (behavior_method_order_.find(behavior_name) == behavior_method_order_.end()) {
            auto behavior_def = env_.lookup_behavior(behavior_name);
            if (behavior_def) {
                std::vector<std::string> methods;
                for (const auto& m : behavior_def->methods) {
                    methods.push_back(m.name);
                }
                behavior_method_order_[behavior_name] = methods;
            }
        }

        // Register vtable for EVERY impl (not just the first per behavior)
        std::string type_name;
        if (impl->self_type->kind.index() == 0) {
            const auto& named = std::get<parser::NamedType>(impl->self_type->kind);
            if (!named.path.segments.empty()) {
                type_name = named.path.segments.back();
            }
        }
        if (!type_name.empty()) {
            std::string vtable_name = "@vtable." + type_name + "." + behavior_name;
            std::string key = type_name + "::" + behavior_name;
            vtables_[key] = vtable_name;
        }
    }
}

void LLVMIRGen::emit_dyn_type(const std::string& behavior_name) {
    if (emitted_dyn_types_.count(behavior_name))
        return;
    emitted_dyn_types_.insert(behavior_name);

    // Emit the dyn type as a fat pointer struct: { data_ptr, vtable_ptr }
    emit_line("%dyn." + behavior_name + " = type { ptr, ptr }");
}

auto LLVMIRGen::get_vtable(const std::string& type_name, const std::string& behavior_name)
    -> std::string {
    std::string key = type_name + "::" + behavior_name;
    auto it = vtables_.find(key);
    if (it != vtables_.end()) {
        return it->second;
    }
    return ""; // No vtable found
}

void LLVMIRGen::emit_vtables() {
    // For each registered impl block, generate a vtable
    for (const auto* impl : pending_impls_) {
        if (!impl->trait_type)
            continue; // Skip inherent impls

        // Get the type name and behavior name
        std::string type_name;
        if (impl->self_type->kind.index() == 0) { // NamedType
            const auto& named = std::get<parser::NamedType>(impl->self_type->kind);
            if (!named.path.segments.empty()) {
                type_name = named.path.segments.back();
            }
        }

        std::string behavior_name;
        if (impl->trait_type->is<parser::NamedType>()) {
            const auto& named = impl->trait_type->as<parser::NamedType>();
            if (!named.path.segments.empty()) {
                behavior_name = named.path.segments.back();
            }
        }

        if (type_name.empty() || behavior_name.empty())
            continue;

        // Emit the dyn type for this behavior
        emit_dyn_type(behavior_name);

        // Get behavior method order
        auto behavior_def = env_.lookup_behavior(behavior_name);
        if (!behavior_def)
            continue;

        // Build vtable type: array of function pointers
        std::string vtable_name = "@vtable." + type_name + "." + behavior_name;
        std::string vtable_type = "{ ";
        for (size_t i = 0; i < behavior_def->methods.size(); ++i) {
            if (i > 0)
                vtable_type += ", ";
            vtable_type += "ptr";
        }
        vtable_type += " }";

        // Build vtable value with function pointers
        std::string vtable_value = "{ ";
        for (size_t i = 0; i < behavior_def->methods.size(); ++i) {
            if (i > 0)
                vtable_value += ", ";
            const auto& method = behavior_def->methods[i];
            // Look up in functions_ to get the correct LLVM name
            std::string method_lookup_key = type_name + "_" + method.name;
            auto method_it = functions_.find(method_lookup_key);
            if (method_it != functions_.end()) {
                vtable_value += "ptr " + method_it->second.llvm_name;
            } else {
                vtable_value += "ptr @tml_" + get_suite_prefix() + type_name + "_" + method.name;
            }
        }
        vtable_value += " }";

        // Emit vtable global constant
        emit_line(vtable_name + " = internal constant " + vtable_type + " " + vtable_value);

        // Register vtable
        std::string key = type_name + "::" + behavior_name;
        vtables_[key] = vtable_name;

        // Store method order for this behavior
        if (behavior_method_order_.find(behavior_name) == behavior_method_order_.end()) {
            std::vector<std::string> methods;
            for (const auto& m : behavior_def->methods) {
                methods.push_back(m.name);
            }
            behavior_method_order_[behavior_name] = methods;
        }
    }
}

} // namespace tml::codegen
