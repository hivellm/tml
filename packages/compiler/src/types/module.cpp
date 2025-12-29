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
    auto module = get_module(module_path);
    if (!module)
        return std::nullopt;

    auto it = module->enums.find(symbol_name);
    if (it != module->enums.end()) {
        return it->second;
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

} // namespace tml::types
