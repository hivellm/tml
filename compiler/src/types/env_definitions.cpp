TML_MODULE("compiler")

//! # Type Environment - Definitions
//!
//! This file implements type definition registration.
//!
//! ## Definition Methods
//!
//! | Method              | Registers                        |
//! |---------------------|----------------------------------|
//! | `define_struct()`   | Struct type definition           |
//! | `define_enum()`     | Enum type definition             |
//! | `define_behavior()` | Behavior (trait) definition      |
//! | `define_function()` | Function signature               |
//! | `register_impl()`   | Behavior implementation          |
//!
//! ## Drop and Copy Traits
//!
//! Special handling for ownership semantics:
//! - `type_needs_drop()`: Check if type requires destructor
//! - `type_implements()`: Check behavior implementation

#include "types/env.hpp"

namespace tml::types {

void TypeEnv::define_struct(StructDef def) {
    structs_[def.name] = std::move(def);
}

void TypeEnv::define_enum(EnumDef def) {
    enums_[def.name] = std::move(def);
}

void TypeEnv::define_behavior(BehaviorDef def) {
    behaviors_[def.name] = std::move(def);
}

void TypeEnv::define_func(FuncSig sig) {
    // Function overloading: add to vector of overloads instead of replacing
    functions_[sig.name].push_back(sig);

    // If this is an FFI function with a module namespace, register it in module registry
    // This enables qualified calls like SDL2::init()
    if (sig.has_ffi_module() && module_registry_) {
        const std::string& ffi_mod = *sig.ffi_module;

        // Get or create the FFI module
        if (!module_registry_->has_module(ffi_mod)) {
            Module ffi_module;
            ffi_module.name = ffi_mod;
            ffi_module.file_path = ""; // FFI modules have no source file
            module_registry_->register_module(ffi_mod, ffi_module);
        }

        // Add function to the FFI module
        if (auto* mod = module_registry_->get_module_mut(ffi_mod)) {
            mod->functions[sig.name] = sig;
        }
    }
}

void TypeEnv::define_type_alias(const std::string& name, TypePtr type,
                                std::vector<std::string> generic_params) {
    type_aliases_[name] = std::move(type);
    if (!generic_params.empty()) {
        type_alias_generics_[name] = std::move(generic_params);
    }
}

// ============================================================================
// OOP Type Definitions (C#-style)
// ============================================================================

void TypeEnv::define_class(ClassDef def) {
    // Register implemented interfaces
    for (const auto& iface : def.interfaces) {
        class_interfaces_[def.name].push_back(iface);
    }
    classes_[def.name] = std::move(def);
}

void TypeEnv::define_interface(InterfaceDef def) {
    interfaces_[def.name] = std::move(def);
}

auto TypeEnv::all_classes() const -> const std::unordered_map<std::string, ClassDef>& {
    return classes_;
}

auto TypeEnv::all_interfaces() const -> const std::unordered_map<std::string, InterfaceDef>& {
    return interfaces_;
}

void TypeEnv::register_class_interface(const std::string& class_name,
                                       const std::string& interface_name) {
    class_interfaces_[class_name].push_back(interface_name);
}

bool TypeEnv::class_implements_interface(const std::string& class_name,
                                         const std::string& interface_name) const {
    auto it = class_interfaces_.find(class_name);
    if (it == class_interfaces_.end())
        return false;

    for (const auto& iface : it->second) {
        if (iface == interface_name)
            return true;
        // Check if the implemented interface extends the target interface
        if (auto iface_def = lookup_interface(iface)) {
            for (const auto& parent : iface_def->extends) {
                if (parent == interface_name)
                    return true;
            }
        }
    }
    return false;
}

bool TypeEnv::is_subclass_of(const std::string& derived, const std::string& base) const {
    if (derived == base)
        return true;

    auto it = classes_.find(derived);
    if (it == classes_.end())
        return false;

    if (!it->second.base_class)
        return false;

    // Direct parent
    if (*it->second.base_class == base)
        return true;

    // Check recursively
    return is_subclass_of(*it->second.base_class, base);
}

} // namespace tml::types
