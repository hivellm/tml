//! # Module Registry
//!
//! This file implements the module registry for managing compiled modules.
//!
//! ## Module Registration
//!
//! | Method              | Description                      |
//! |---------------------|----------------------------------|
//! | `register_module()` | Add compiled module to registry  |
//! | `get_module()`      | Retrieve module by path          |
//! | `has_module()`      | Check if module is registered    |
//!
//! ## Symbol Lookup
//!
//! | Method           | Looks Up                          |
//! |------------------|-----------------------------------|
//! | `lookup_struct()`| Struct in specific module         |
//! | `lookup_enum()`  | Enum in specific module           |
//! | `lookup_func()`  | Function in specific module       |
//!
//! ## Module Paths
//!
//! Modules are identified by their qualified path (e.g., `std::io`).

#include "types/module.hpp"

#include "types/env.hpp"

#include <algorithm>

namespace tml::types {

void ModuleRegistry::register_module(const std::string& path, Module module) {
    modules_[path] = std::move(module);
}

auto ModuleRegistry::get_module(const std::string& path) const -> std::optional<Module> {
    auto it = modules_.find(path);
    if (it != modules_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ModuleRegistry::get_module_mut(const std::string& path) -> Module* {
    auto it = modules_.find(path);
    if (it != modules_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ModuleRegistry::has_module(const std::string& path) const {
    return modules_.find(path) != modules_.end();
}

auto ModuleRegistry::list_modules() const -> std::vector<std::string> {
    std::vector<std::string> result;
    result.reserve(modules_.size());
    for (const auto& [path, _] : modules_) {
        result.push_back(path);
    }
    return result;
}

auto ModuleRegistry::resolve_file_module(const std::string& path) const
    -> std::optional<std::string> {
    auto it = file_to_module_.find(path);
    if (it != file_to_module_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void ModuleRegistry::register_file_mapping(const std::string& file_path,
                                           const std::string& module_path) {
    file_to_module_[file_path] = module_path;
}

auto ModuleRegistry::lookup_function(const std::string& module_path,
                                     const std::string& symbol_name) const
    -> std::optional<FuncSig> {
    auto module = get_module(module_path);
    if (!module)
        return std::nullopt;

    auto it = module->functions.find(symbol_name);
    if (it != module->functions.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ModuleRegistry::lookup_struct(const std::string& module_path,
                                   const std::string& symbol_name) const
    -> std::optional<StructDef> {
    auto module = get_module(module_path);
    if (!module)
        return std::nullopt;

    auto it = module->structs.find(symbol_name);
    if (it != module->structs.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ModuleRegistry::lookup_enum(const std::string& module_path,
                                 const std::string& symbol_name) const -> std::optional<EnumDef> {
    // Track visited modules to prevent infinite recursion
    std::unordered_set<std::string> visited;
    return lookup_enum_impl(module_path, symbol_name, visited);
}

auto ModuleRegistry::lookup_enum_impl(const std::string& module_path,
                                      const std::string& symbol_name,
                                      std::unordered_set<std::string>& visited) const
    -> std::optional<EnumDef> {
    // Prevent infinite recursion
    if (visited.count(module_path) > 0) {
        return std::nullopt;
    }
    visited.insert(module_path);

    auto module = get_module(module_path);
    if (!module)
        return std::nullopt;

    // First, try to find the enum directly in this module
    auto it = module->enums.find(symbol_name);
    if (it != module->enums.end()) {
        return it->second;
    }

    // Not found directly - check re-exports
    for (const auto& re_export : module->re_exports) {
        bool should_follow = false;

        if (re_export.is_glob) {
            // Glob import - try to find the symbol in the source module
            should_follow = true;
        } else {
            // Check if this specific symbol is in the re-export list
            for (const auto& sym : re_export.symbols) {
                if (sym == symbol_name) {
                    should_follow = true;
                    break;
                }
            }
        }

        if (should_follow) {
            // Recursively look up in the source module
            auto result = lookup_enum_impl(re_export.source_path, symbol_name, visited);
            if (result) {
                return result;
            }
        }
    }

    return std::nullopt;
}

auto ModuleRegistry::lookup_behavior(const std::string& module_path,
                                     const std::string& symbol_name) const
    -> std::optional<BehaviorDef> {
    auto module = get_module(module_path);
    if (!module)
        return std::nullopt;

    auto it = module->behaviors.find(symbol_name);
    if (it != module->behaviors.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ModuleRegistry::lookup_type_alias(const std::string& module_path,
                                       const std::string& symbol_name) const
    -> std::optional<TypePtr> {
    auto module = get_module(module_path);
    if (!module)
        return std::nullopt;

    auto it = module->type_aliases.find(symbol_name);
    if (it != module->type_aliases.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ModuleRegistry::lookup_type_alias_generics(const std::string& module_path,
                                                const std::string& symbol_name) const
    -> std::optional<std::vector<std::string>> {
    auto module = get_module(module_path);
    if (!module)
        return std::nullopt;

    auto it = module->type_alias_generics.find(symbol_name);
    if (it != module->type_alias_generics.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ModuleRegistry::lookup_constant(const std::string& module_path,
                                     const std::string& symbol_name) const
    -> std::optional<std::string> {
    // Track visited modules to prevent infinite recursion
    std::unordered_set<std::string> visited;
    return lookup_constant_impl(module_path, symbol_name, visited);
}

auto ModuleRegistry::lookup_constant_impl(const std::string& module_path,
                                          const std::string& symbol_name,
                                          std::unordered_set<std::string>& visited) const
    -> std::optional<std::string> {
    // Prevent infinite recursion
    if (visited.count(module_path) > 0) {
        return std::nullopt;
    }
    visited.insert(module_path);

    auto module = get_module(module_path);
    if (!module)
        return std::nullopt;

    // First, try to find the constant directly in this module
    auto it = module->constants.find(symbol_name);
    if (it != module->constants.end()) {
        return it->second.value;
    }

    // Not found directly - check re-exports
    for (const auto& re_export : module->re_exports) {
        bool should_follow = false;

        if (re_export.is_glob) {
            // Glob import - try to find the symbol in the source module
            should_follow = true;
        } else {
            // Check if this specific symbol is in the re-export list
            for (const auto& sym : re_export.symbols) {
                if (sym == symbol_name) {
                    should_follow = true;
                    break;
                }
            }
        }

        if (should_follow) {
            // Recursively look up in the source module
            auto result = lookup_constant_impl(re_export.source_path, symbol_name, visited);
            if (result) {
                return result;
            }
        }
    }

    return std::nullopt;
}

auto ModuleRegistry::lookup_class(const std::string& module_path,
                                  const std::string& symbol_name) const -> std::optional<ClassDef> {
    auto module = get_module(module_path);
    if (!module)
        return std::nullopt;

    auto it = module->classes.find(symbol_name);
    if (it != module->classes.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ModuleRegistry::lookup_interface(const std::string& module_path,
                                      const std::string& symbol_name) const
    -> std::optional<InterfaceDef> {
    auto module = get_module(module_path);
    if (!module)
        return std::nullopt;

    auto it = module->interfaces.find(symbol_name);
    if (it != module->interfaces.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto ModuleRegistry::lookup_symbol(const std::string& module_path,
                                   const std::string& symbol_name) const
    -> std::optional<ModuleSymbol> {
    auto module = get_module(module_path);
    if (!module)
        return std::nullopt;

    // Try each symbol type in order
    {
        auto it = module->functions.find(symbol_name);
        if (it != module->functions.end()) {
            return ModuleSymbol{it->second};
        }
    }

    {
        auto it = module->structs.find(symbol_name);
        if (it != module->structs.end()) {
            return ModuleSymbol{it->second};
        }
    }

    {
        auto it = module->enums.find(symbol_name);
        if (it != module->enums.end()) {
            return ModuleSymbol{it->second};
        }
    }

    {
        auto it = module->behaviors.find(symbol_name);
        if (it != module->behaviors.end()) {
            return ModuleSymbol{it->second};
        }
    }

    {
        auto it = module->type_aliases.find(symbol_name);
        if (it != module->type_aliases.end()) {
            return ModuleSymbol{it->second};
        }
    }

    return std::nullopt;
}

// ============================================================================
// GlobalModuleCache Implementation
// ============================================================================

GlobalModuleCache& GlobalModuleCache::instance() {
    static GlobalModuleCache cache;
    return cache;
}

bool GlobalModuleCache::has(const std::string& module_path) const {
    std::shared_lock lock(mutex_);
    return cache_.find(module_path) != cache_.end();
}

std::optional<Module> GlobalModuleCache::get(const std::string& module_path) const {
    std::shared_lock lock(mutex_);
    auto it = cache_.find(module_path);
    if (it != cache_.end()) {
        ++hits_;
        return it->second;
    }
    ++misses_;
    return std::nullopt;
}

void GlobalModuleCache::put(const std::string& module_path, const Module& module) {
    // Only cache library modules
    if (!should_cache(module_path)) {
        return;
    }

    std::unique_lock lock(mutex_);
    cache_[module_path] = module;
}

void GlobalModuleCache::clear() {
    std::unique_lock lock(mutex_);
    cache_.clear();
    hits_.store(0);
    misses_.store(0);
}

GlobalModuleCache::Stats GlobalModuleCache::get_stats() const {
    std::shared_lock lock(mutex_);
    return Stats{
        .total_entries = cache_.size(), .cache_hits = hits_.load(), .cache_misses = misses_.load()};
}

bool GlobalModuleCache::should_cache(const std::string& module_path) {
    // Cache library modules: core::*, std::*, test
    if (module_path.starts_with("core::") || module_path.starts_with("std::") ||
        module_path == "test" || module_path.starts_with("test::")) {
        return true;
    }
    return false;
}

// ============================================================================
// ModuleRegistry::clone
// ============================================================================

ModuleRegistry ModuleRegistry::clone() const {
    ModuleRegistry copy;
    copy.modules_ = modules_;
    copy.file_to_module_ = file_to_module_;
    return copy;
}

} // namespace tml::types
