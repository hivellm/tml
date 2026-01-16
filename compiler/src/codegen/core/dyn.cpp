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
    // Write to type_defs_buffer_ to ensure type definition appears before use
    TML_DEBUG_LN("[DYN] Emitting dyn type: %dyn." << behavior_name);
    type_defs_buffer_ << "%dyn." << behavior_name << " = type { ptr, ptr }\n";
}

auto LLVMIRGen::get_vtable(const std::string& type_name, const std::string& behavior_name)
    -> std::string {
    std::string key = type_name + "::" + behavior_name;

    // First check behavior vtables (impl blocks)
    auto it = vtables_.find(key);
    if (it != vtables_.end()) {
        return it->second;
    }

    // Check interface vtables (class implements)
    auto iface_it = interface_vtables_.find(key);
    if (iface_it != interface_vtables_.end()) {
        return iface_it->second;
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

        // Build vtable name and check if already emitted
        std::string vtable_name = "@vtable." + type_name + "." + behavior_name;
        if (emitted_vtables_.count(vtable_name)) {
            TML_DEBUG_LN("[DYN] Skipping duplicate vtable: " << vtable_name);
            continue;
        }
        emitted_vtables_.insert(vtable_name);

        // Emit the dyn type for this behavior
        emit_dyn_type(behavior_name);

        // Get behavior method order
        auto behavior_def = env_.lookup_behavior(behavior_name);
        if (!behavior_def)
            continue;

        // Build vtable type: array of function pointers
        std::string vtable_type = "{ ";
        for (size_t i = 0; i < behavior_def->methods.size(); ++i) {
            if (i > 0)
                vtable_type += ", ";
            vtable_type += "ptr";
        }
        vtable_type += " }";

        // Collect method names that the impl explicitly provides
        std::set<std::string> impl_method_names;
        for (const auto& m : impl->methods) {
            impl_method_names.insert(m.name);
        }

        // First pass: collect all function references and check which need declarations
        std::vector<std::string> vtable_entries;
        std::vector<std::string> missing_decls;
        bool all_methods_available = true;

        for (size_t i = 0; i < behavior_def->methods.size(); ++i) {
            const auto& method = behavior_def->methods[i];
            // Look up in functions_ to get the correct LLVM name
            // Try both with and without suite prefix, since impl methods for test-local
            // types are registered with suite prefix in suite mode
            std::string method_lookup_key = type_name + "_" + method.name;
            auto method_it = functions_.find(method_lookup_key);

            // If not found, try with suite prefix (for test-local types in suite mode)
            if (method_it == functions_.end() && !get_suite_prefix().empty()) {
                std::string prefixed_key = get_suite_prefix() + method_lookup_key;
                method_it = functions_.find(prefixed_key);
            }
            std::string func_name;

            if (method_it != functions_.end()) {
                // Method was compiled in this compilation unit, use its registered name
                func_name = method_it->second.llvm_name;
            } else {
                // Method not found in local functions_
                // Check if it's explicitly provided by the impl block
                bool explicitly_provided = (impl_method_names.count(method.name) > 0);

                // If explicitly provided but not in functions_, it means the impl comes
                // from an imported module and wasn't compiled in this unit - skip this vtable
                if (explicitly_provided) {
                    TML_DEBUG_LN("[DYN] Skipping vtable " << vtable_name
                                << " - method " << method.name << " from imported module");
                    all_methods_available = false;
                    break;
                }

                // Not explicitly provided - check if it has a default implementation
                // AND if we have the trait definition to generate it
                bool has_default = (behavior_def->methods_with_defaults.count(method.name) > 0);
                bool can_generate_default = has_default &&
                    (trait_decls_.find(behavior_name) != trait_decls_.end());

                if (!has_default) {
                    // Required method with no default and not provided by impl - can't generate vtable
                    TML_DEBUG_LN("[DYN] Skipping vtable " << vtable_name
                                << " - missing required method " << method.name);
                    all_methods_available = false;
                    break;
                }

                if (!can_generate_default) {
                    // Has default but we don't have the trait definition to generate it
                    // This happens when impl comes from imported module
                    TML_DEBUG_LN("[DYN] Skipping vtable " << vtable_name
                                << " - cannot generate default for " << method.name
                                << " (trait " << behavior_name << " not in trait_decls_)");
                    all_methods_available = false;
                    break;
                }

                // Use non-prefixed name for default methods
                func_name = "@tml_" + type_name + "_" + method.name;
                // Mark as needing an external declaration only if not already generated
                if (functions_.find(type_name + "_" + method.name) == functions_.end()) {
                    missing_decls.push_back(func_name);
                }
            }
            vtable_entries.push_back(func_name);
        }

        // Skip this vtable if not all methods are available
        if (!all_methods_available) {
            continue;
        }

        // Emit external declarations for missing default implementations
        // This allows linking to work even if the implementations are in another compilation unit
        for (const auto& decl : missing_decls) {
            // Check if we've already declared this function
            if (declared_externals_.find(decl) == declared_externals_.end()) {
                declared_externals_.insert(decl);
                // Declare as external function (generic signature, will be resolved at link time)
                emit_line("declare ptr " + decl + "(ptr) #0");
            }
        }

        // Build vtable value with function pointers
        std::string vtable_value = "{ ";
        for (size_t i = 0; i < vtable_entries.size(); ++i) {
            if (i > 0)
                vtable_value += ", ";
            vtable_value += "ptr " + vtable_entries[i];
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
