#include "tml/types/env.hpp"

namespace tml::types {

auto TypeEnv::lookup_struct(const std::string& name) const -> std::optional<StructDef> {
    auto it = structs_.find(name);
    if (it != structs_.end()) return it->second;
    return std::nullopt;
}

auto TypeEnv::lookup_enum(const std::string& name) const -> std::optional<EnumDef> {
    auto it = enums_.find(name);
    if (it != enums_.end()) return it->second;
    return std::nullopt;
}

auto TypeEnv::lookup_behavior(const std::string& name) const -> std::optional<BehaviorDef> {
    auto it = behaviors_.find(name);
    if (it != behaviors_.end()) return it->second;
    return std::nullopt;
}

auto TypeEnv::lookup_func(const std::string& name) const -> std::optional<FuncSig> {
    auto it = functions_.find(name);
    if (it != functions_.end()) return it->second;
    return std::nullopt;
}

auto TypeEnv::lookup_type_alias(const std::string& name) const -> std::optional<TypePtr> {
    auto it = type_aliases_.find(name);
    if (it != type_aliases_.end()) return it->second;
    return std::nullopt;
}

auto TypeEnv::all_enums() const -> const std::unordered_map<std::string, EnumDef>& {
    return enums_;
}

} // namespace tml::types
