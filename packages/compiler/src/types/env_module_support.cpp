#include "tml/types/env.hpp"
#include "tml/types/module.hpp"
#include <algorithm>

namespace tml::types {

void TypeEnv::set_module_registry(std::shared_ptr<ModuleRegistry> registry) {
    module_registry_ = std::move(registry);
    // Automatically initialize test module when registry is set
    init_test_module();
}

void TypeEnv::set_current_module(const std::string& module_path) {
    current_module_path_ = module_path;
}

auto TypeEnv::module_registry() const -> std::shared_ptr<ModuleRegistry> {
    return module_registry_;
}

auto TypeEnv::current_module() const -> const std::string& {
    return current_module_path_;
}

void TypeEnv::import_symbol(const std::string& module_path, const std::string& symbol_name,
                           std::optional<std::string> alias) {
    // Determine the local name (use alias if provided, otherwise original name)
    std::string local_name = alias.value_or(symbol_name);

    // Create the imported symbol entry
    ImportedSymbol import{
        .original_name = symbol_name,
        .local_name = local_name,
        .module_path = module_path,
        .visibility = parser::Visibility::Public  // Imported symbols are accessible
    };

    // Store the import
    imported_symbols_[local_name] = import;
}

void TypeEnv::import_all_from(const std::string& module_path) {
    if (!module_registry_) {
        return;  // No module registry available
    }

    auto module = module_registry_->get_module(module_path);
    if (!module) {
        return;  // Module not found
    }

    // Import all functions
    for (const auto& [name, func_sig] : module->functions) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all structs
    for (const auto& [name, struct_def] : module->structs) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all enums
    for (const auto& [name, enum_def] : module->enums) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all behaviors
    for (const auto& [name, behavior_def] : module->behaviors) {
        import_symbol(module_path, name, std::nullopt);
    }

    // Import all type aliases
    for (const auto& [name, type_ptr] : module->type_aliases) {
        import_symbol(module_path, name, std::nullopt);
    }
}

auto TypeEnv::resolve_imported_symbol(const std::string& name) const
    -> std::optional<std::string> {
    auto it = imported_symbols_.find(name);
    if (it != imported_symbols_.end()) {
        // Return the full qualified name: module_path::original_name
        return it->second.module_path + "::" + it->second.original_name;
    }
    return std::nullopt;
}

} // namespace tml::types
