#include "tml/types/env.hpp"
#include "tml/types/module.hpp"

namespace tml::types {

auto TypeEnv::lookup_struct(const std::string& name) const -> std::optional<StructDef> {
    // 1. Check local module
    auto it = structs_.find(name);
    if (it != structs_.end()) return it->second;

    // 2. Check imported symbols
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            // Parse module_path::symbol_name
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_struct(module_path, symbol_name);
            }
        }
    }

    return std::nullopt;
}

auto TypeEnv::lookup_enum(const std::string& name) const -> std::optional<EnumDef> {
    // 1. Check local module
    auto it = enums_.find(name);
    if (it != enums_.end()) return it->second;

    // 2. Check imported symbols
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_enum(module_path, symbol_name);
            }
        }
    }

    return std::nullopt;
}

auto TypeEnv::lookup_behavior(const std::string& name) const -> std::optional<BehaviorDef> {
    // 1. Check local module
    auto it = behaviors_.find(name);
    if (it != behaviors_.end()) return it->second;

    // 2. Check imported symbols
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_behavior(module_path, symbol_name);
            }
        }
    }

    return std::nullopt;
}

auto TypeEnv::lookup_func(const std::string& name) const -> std::optional<FuncSig> {
    // 1. Check local module
    auto it = functions_.find(name);
    if (it != functions_.end()) return it->second;

    // 2. Check imported symbols
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_function(module_path, symbol_name);
            }
        }
    }

    return std::nullopt;
}

auto TypeEnv::lookup_type_alias(const std::string& name) const -> std::optional<TypePtr> {
    // 1. Check local module
    auto it = type_aliases_.find(name);
    if (it != type_aliases_.end()) return it->second;

    // 2. Check imported symbols
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                return module_registry_->lookup_type_alias(module_path, symbol_name);
            }
        }
    }

    return std::nullopt;
}

auto TypeEnv::all_enums() const -> const std::unordered_map<std::string, EnumDef>& {
    return enums_;
}

auto TypeEnv::get_module(const std::string &module_path) const -> std::optional<Module> {
    if (!module_registry_) {
        return std::nullopt;
    }
    return module_registry_->get_module(module_path);
}

} // namespace tml::types
