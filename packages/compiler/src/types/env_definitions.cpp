#include "tml/types/env.hpp"

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
    functions_[sig.name] = std::move(sig);
}

void TypeEnv::define_type_alias(const std::string& name, TypePtr type) {
    type_aliases_[name] = std::move(type);
}

} // namespace tml::types
