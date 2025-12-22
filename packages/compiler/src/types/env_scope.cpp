#include "tml/types/env.hpp"

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
