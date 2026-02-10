//! # Type Environment - Core
//!
//! This file implements core TypeEnv functionality.
//!
//! ## Scope Management
//!
//! | Method         | Description                      |
//! |----------------|----------------------------------|
//! | `push_scope()` | Enter new local scope            |
//! | `pop_scope()`  | Exit current scope               |
//! | `define()`     | Add variable to current scope    |
//!
//! ## Type Inference
//!
//! | Method            | Description                     |
//! |-------------------|---------------------------------|
//! | `fresh_type_var()`| Create new unknown type         |
//! | `unify()`         | Add type constraint             |
//! | `resolve()`       | Get final type after inference  |
//!
//! ## Initialization
//!
//! Constructor calls `init_builtins()` to register primitive types,
//! behaviors, and standard library functions.

#include "types/env.hpp"

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
    if (!type)
        return type;

    // Track visited type variables to detect cycles
    std::unordered_set<uint64_t> visited;
    return resolve_impl(type, visited);
}

auto TypeEnv::resolve_impl(TypePtr type, std::unordered_set<uint64_t>& visited) -> TypePtr {
    if (!type)
        return type;

    if (type->is<TypeVar>()) {
        auto id = type->as<TypeVar>().id;
        // Check for cycle
        if (visited.count(id)) {
            return type; // Cycle detected, return as-is
        }
        visited.insert(id);

        auto it = substitutions_.find(id);
        if (it != substitutions_.end()) {
            return resolve_impl(it->second, visited);
        }
    }
    return type;
}

auto TypeEnv::builtin_types() const -> const std::unordered_map<std::string, TypePtr>& {
    return builtins_;
}

// ============================================================================
// Snapshot Support
// ============================================================================

TypeEnv::TypeEnv(SnapshotTag, const TypeEnv& source)
    : // Type definition tables (shared across all compilation units)
      structs_(source.structs_), enums_(source.enums_), behaviors_(source.behaviors_),
      functions_(source.functions_), behavior_impls_(source.behavior_impls_),
      type_aliases_(source.type_aliases_), type_alias_generics_(source.type_alias_generics_),
      builtins_(source.builtins_),
      // OOP type definition tables
      classes_(source.classes_), interfaces_(source.interfaces_),
      class_interfaces_(source.class_interfaces_),
      // Fresh per-file state
      current_scope_(std::make_shared<Scope>()), type_var_counter_(0),
      // substitutions_ - default empty (fresh inference state)
      // Module system - fresh registry, per-file paths reset
      module_registry_(std::make_shared<ModuleRegistry>()),
      // current_module_path_ - default empty
      // source_directory_ - default empty
      // imported_symbols_ - default empty
      // import_conflicts_ - default empty
      abort_on_module_error_(source.abort_on_module_error_)
// loading_modules_ - default empty
{}

TypeEnv TypeEnv::snapshot() const {
    return TypeEnv(SnapshotTag{}, *this);
}

} // namespace tml::types
