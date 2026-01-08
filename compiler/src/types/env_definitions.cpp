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

void TypeEnv::define_type_alias(const std::string& name, TypePtr type) {
    type_aliases_[name] = std::move(type);
}

} // namespace tml::types
