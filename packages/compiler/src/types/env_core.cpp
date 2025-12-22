#include "tml/types/env.hpp"

namespace tml::types {

TypeEnv::TypeEnv() {
    current_scope_ = std::make_shared<Scope>();
    init_builtins();
}

void TypeEnv::push_scope() {
    current_scope_ = std::make_shared<Scope>(current_scope_);
}

void TypeEnv::pop_scope() {
    if (current_scope_->parent()) {
        current_scope_ = current_scope_->parent();
    }
}

auto TypeEnv::current_scope() -> std::shared_ptr<Scope> {
    return current_scope_;
}

auto TypeEnv::fresh_type_var() -> TypePtr {
    auto type = std::make_shared<Type>();
    type->kind = TypeVar{type_var_counter_++, std::nullopt};
    return type;
}

void TypeEnv::unify(TypePtr a, TypePtr b) {
    if (a->is<TypeVar>()) {
        substitutions_[a->as<TypeVar>().id] = b;
    } else if (b->is<TypeVar>()) {
        substitutions_[b->as<TypeVar>().id] = a;
    }
}

auto TypeEnv::resolve(TypePtr type) -> TypePtr {
    if (!type) return type;

    if (type->is<TypeVar>()) {
        auto id = type->as<TypeVar>().id;
        auto it = substitutions_.find(id);
        if (it != substitutions_.end()) {
            return resolve(it->second);
        }
    }
    return type;
}

auto TypeEnv::builtin_types() const -> const std::unordered_map<std::string, TypePtr>& {
    return builtins_;
}

} // namespace tml::types
