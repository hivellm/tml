//! # Query Provider Registry
//!
//! Maps query kinds to their provider functions.
//! A provider function takes a QueryContext and QueryKey, executes the
//! computation, and returns the result as std::any.

#pragma once

#include "query/query_key.hpp"

#include <any>
#include <array>
#include <functional>

namespace tml::query {

// Forward declaration
class QueryContext;

/// Type-erased provider function.
using ProviderFn = std::function<std::any(QueryContext&, const QueryKey&)>;

/// Registry mapping query kinds to their provider functions.
class QueryProviderRegistry {
public:
    /// Register a provider for a query kind.
    void register_provider(QueryKind kind, ProviderFn provider);

    /// Get the provider for a query kind. Returns nullptr if not registered.
    [[nodiscard]] const ProviderFn* get_provider(QueryKind kind) const;

    /// Register all core providers (read_source, tokenize, parse, etc.)
    void register_core_providers();

private:
    std::array<ProviderFn, static_cast<size_t>(QueryKind::COUNT)> providers_{};
};

} // namespace tml::query
