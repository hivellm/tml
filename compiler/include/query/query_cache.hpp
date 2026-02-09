//! # Query Cache
//!
//! Thread-safe memoization cache for query results.
//! Uses `std::shared_mutex` for concurrent read access.

#pragma once

#include "query/query_fingerprint.hpp"
#include "query/query_key.hpp"

#include <any>
#include <atomic>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace tml::query {

/// A single cache entry with result, fingerprints, and dependencies.
struct CacheEntry {
    std::any result;
    Fingerprint input_fingerprint;
    Fingerprint output_fingerprint;
    std::vector<QueryKey> dependencies;
};

/// Thread-safe query cache with memoization.
class QueryCache {
public:
    /// Look up a cached result. Returns nullopt if not cached.
    template <typename R> std::optional<R> lookup(const QueryKey& key) const {
        std::shared_lock lock(mutex_);
        auto it = entries_.find(key);
        if (it == entries_.end()) {
            misses_.fetch_add(1, std::memory_order_relaxed);
            return std::nullopt;
        }
        hits_.fetch_add(1, std::memory_order_relaxed);
        try {
            return std::any_cast<R>(it->second.result);
        } catch (const std::bad_any_cast&) {
            return std::nullopt;
        }
    }

    /// Check if a key is cached.
    [[nodiscard]] bool contains(const QueryKey& key) const;

    /// Insert a result into the cache.
    template <typename R>
    void insert(const QueryKey& key, R result, Fingerprint input_fp, Fingerprint output_fp,
                std::vector<QueryKey> deps) {
        std::unique_lock lock(mutex_);
        CacheEntry entry;
        entry.result = std::move(result);
        entry.input_fingerprint = input_fp;
        entry.output_fingerprint = output_fp;
        entry.dependencies = std::move(deps);
        entries_[key] = std::move(entry);
    }

    /// Get the cache entry (for dependency/fingerprint inspection).
    [[nodiscard]] std::optional<CacheEntry> get_entry(const QueryKey& key) const;

    /// Invalidate a specific entry.
    void invalidate(const QueryKey& key);

    /// Invalidate all entries that transitively depend on the given key.
    void invalidate_dependents(const QueryKey& key);

    /// Clear the entire cache.
    void clear();

    /// Cache statistics.
    struct Stats {
        size_t total_entries = 0;
        size_t hits = 0;
        size_t misses = 0;
    };

    /// Get cache statistics.
    [[nodiscard]] Stats get_stats() const;

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<QueryKey, CacheEntry, QueryKeyHash, QueryKeyEqual> entries_;
    mutable std::atomic<size_t> hits_{0};
    mutable std::atomic<size_t> misses_{0};
};

} // namespace tml::query
