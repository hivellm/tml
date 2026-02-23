TML_MODULE("compiler")

//! # Type Environment - Scopes
//!
//! This file implements the Scope class for variable tracking.
//!
//! ## Scope Structure
//!
//! Scopes form a linked list (child → parent) for lexical scoping.
//! Variable lookup walks up the chain until found or reaching root.
//!
//! ## Symbol Information
//!
//! Each symbol tracks:
//! - `name`: Variable identifier
//! - `type`: Resolved type
//! - `is_mutable`: Whether `var` or `let mut`
//! - `span`: Source location for error messages
//!
//! ## Methods
//!
//! - `define()`: Add symbol to current scope
//! - `lookup()`: Find symbol in current or parent scopes
//! - `lookup_local()`: Find symbol only in current scope

#include "types/env.hpp"

namespace tml::types {

// Scope implementation
Scope::Scope(std::shared_ptr<Scope> parent) : parent_(std::move(parent)) {}

void Scope::define(const std::string& name, TypePtr type, bool is_mutable, SourceSpan span) {
    symbols_[name] = Symbol{name, std::move(type), is_mutable, span};
}

auto Scope::lookup(const std::string& name) const -> std::optional<Symbol> {
    auto it = symbols_.find(name);
    if (it != symbols_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->lookup(name);
    }
    return std::nullopt;
}

auto Scope::lookup_local(const std::string& name) const -> std::optional<Symbol> {
    auto it = symbols_.find(name);
    if (it != symbols_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto Scope::parent() const -> std::shared_ptr<Scope> {
    return parent_;
}

} // namespace tml::types
