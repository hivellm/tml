//! # Query Dependency Tracking
//!
//! Tracks dependencies between queries during execution and detects cycles.
//! When query Q1 calls force(Q2), Q2 is recorded as a dependency of Q1.

#pragma once

#include "query/query_key.hpp"

#include <mutex>
#include <optional>
#include <vector>

namespace tml::query {

/// Tracks dependencies between queries and detects cycles.
class DependencyTracker {
public:
    /// Push a query onto the execution stack (called when a query starts).
    void push_active(const QueryKey& key);

    /// Pop a query from the execution stack (called when a query completes).
    void pop_active();

    /// Record that the currently active query depends on `callee`.
    void record_dependency(const QueryKey& callee);

    /// Get all dependencies recorded for the current active query.
    [[nodiscard]] std::vector<QueryKey> current_dependencies() const;

    /// Check if executing `key` would create a cycle.
    /// Returns the cycle path if detected, or nullopt otherwise.
    [[nodiscard]] std::optional<std::vector<QueryKey>> detect_cycle(const QueryKey& key) const;

    /// Returns the current query stack depth.
    [[nodiscard]] size_t depth() const;

    /// Clear all tracking state.
    void clear();

private:
    mutable std::mutex mutex_;
    std::vector<QueryKey> active_stack_;

    // Maps each active query to its recorded dependencies
    struct ActiveQueryState {
        QueryKey key;
        std::vector<QueryKey> dependencies;
    };
    std::vector<ActiveQueryState> state_stack_;
};

} // namespace tml::query
