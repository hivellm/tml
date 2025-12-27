#include "tml/types/env.hpp"
#include "tml/types/module.hpp"

namespace tml::types {

auto TypeEnv::lookup_struct(const std::string& name) const -> std::optional<StructDef> {
    auto it = structs_.find(name);
    if (it != structs_.end())
        return it->second;
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
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
    auto it = enums_.find(name);
    if (it != enums_.end())
        return it->second;
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
    auto it = behaviors_.find(name);
    if (it != behaviors_.end())
        return it->second;
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

bool TypeEnv::types_match(const TypePtr& a, const TypePtr& b) {
    if (!a || !b)
        return false;
    if (a->kind.index() != b->kind.index())
        return false;
    if (a->is<PrimitiveType>() && b->is<PrimitiveType>()) {
        return a->as<PrimitiveType>().kind == b->as<PrimitiveType>().kind;
    }
    if (a->is<NamedType>() && b->is<NamedType>()) {
        return a->as<NamedType>().name == b->as<NamedType>().name;
    }
    if (a->is<RefType>() && b->is<RefType>()) {
        const auto& ref_a = a->as<RefType>();
        const auto& ref_b = b->as<RefType>();
        return ref_a.is_mut == ref_b.is_mut && types_match(ref_a.inner, ref_b.inner);
    }
    if (a->is<FuncType>() && b->is<FuncType>()) {
        const auto& func_a = a->as<FuncType>();
        const auto& func_b = b->as<FuncType>();
        if (func_a.params.size() != func_b.params.size())
            return false;
        for (size_t i = 0; i < func_a.params.size(); ++i) {
            if (!types_match(func_a.params[i], func_b.params[i]))
                return false;
        }
        return types_match(func_a.return_type, func_b.return_type);
    }
    return false;
}

auto TypeEnv::lookup_func(const std::string& name) const -> std::optional<FuncSig> {
    auto it = functions_.find(name);
    if (it != functions_.end() && !it->second.empty()) {
        return it->second[0];
    }
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
        // If name contains "::" (like "Range::next" or "SDL2::init"), try direct lookup
        auto method_pos = name.find("::");
        if (method_pos != std::string::npos) {
            std::string module_name = name.substr(0, method_pos);
            std::string func_name = name.substr(method_pos + 2);

            // First, try direct module lookup (works for FFI modules like SDL2::init)
            auto direct_result = module_registry_->lookup_function(module_name, func_name);
            if (direct_result) {
                return direct_result;
            }

            // Try to resolve the type to its module (for Type::method patterns)
            auto type_import_path = resolve_imported_symbol(module_name);
            if (type_import_path) {
                auto pos = type_import_path->rfind("::");
                if (pos != std::string::npos) {
                    std::string module_path = type_import_path->substr(0, pos);
                    // Lookup "Type::method" in the module
                    return module_registry_->lookup_function(module_path, name);
                }
            }
        }
    }
    return std::nullopt;
}

auto TypeEnv::lookup_func_overload(const std::string& name,
                                   const std::vector<TypePtr>& arg_types) const
    -> std::optional<FuncSig> {
    auto it = functions_.find(name);
    if (it != functions_.end()) {
        for (const auto& sig : it->second) {
            if (sig.params.size() != arg_types.size())
                continue;
            bool matches = true;
            for (size_t i = 0; i < arg_types.size(); ++i) {
                if (!types_match(arg_types[i], sig.params[i])) {
                    matches = false;
                    break;
                }
            }
            if (matches)
                return sig;
        }
    }
    if (module_registry_) {
        auto import_path = resolve_imported_symbol(name);
        if (import_path) {
            auto pos = import_path->rfind("::");
            if (pos != std::string::npos) {
                std::string module_path = import_path->substr(0, pos);
                std::string symbol_name = import_path->substr(pos + 2);
                auto sig = module_registry_->lookup_function(module_path, symbol_name);
                if (sig && sig->params.size() == arg_types.size()) {
                    bool matches = true;
                    for (size_t i = 0; i < arg_types.size(); ++i) {
                        if (!types_match(arg_types[i], sig->params[i])) {
                            matches = false;
                            break;
                        }
                    }
                    if (matches)
                        return sig;
                }
            }
        }
    }
    return std::nullopt;
}

auto TypeEnv::get_all_overloads(const std::string& name) const -> std::vector<FuncSig> {
    auto it = functions_.find(name);
    if (it != functions_.end())
        return it->second;
    return {};
}

auto TypeEnv::lookup_type_alias(const std::string& name) const -> std::optional<TypePtr> {
    auto it = type_aliases_.find(name);
    if (it != type_aliases_.end())
        return it->second;
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

auto TypeEnv::all_structs() const -> const std::unordered_map<std::string, StructDef>& {
    return structs_;
}

auto TypeEnv::all_behaviors() const -> const std::unordered_map<std::string, BehaviorDef>& {
    return behaviors_;
}

auto TypeEnv::all_func_names() const -> std::vector<std::string> {
    std::vector<std::string> names;
    for (const auto& [name, _] : functions_) {
        names.push_back(name);
    }
    return names;
}

auto TypeEnv::get_module(const std::string& module_path) const -> std::optional<Module> {
    if (!module_registry_)
        return std::nullopt;
    return module_registry_->get_module(module_path);
}

auto TypeEnv::get_all_modules() const -> std::vector<std::pair<std::string, Module>> {
    std::vector<std::pair<std::string, Module>> result;
    if (module_registry_) {
        for (const auto& [path, mod] : module_registry_->get_all_modules()) {
            result.emplace_back(path, mod);
        }
    }
    return result;
}

void TypeEnv::register_impl(const std::string& type_name, const std::string& behavior_name) {
    behavior_impls_[type_name].push_back(behavior_name);
}

bool TypeEnv::type_implements(const std::string& type_name,
                              const std::string& behavior_name) const {
    auto it = behavior_impls_.find(type_name);
    if (it == behavior_impls_.end())
        return false;
    const auto& behaviors = it->second;
    return std::find(behaviors.begin(), behaviors.end(), behavior_name) != behaviors.end();
}

} // namespace tml::types
