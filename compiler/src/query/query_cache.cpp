#include "query/query_cache.hpp"

#include <queue>
#include <unordered_set>

namespace tml::query {

bool QueryCache::contains(const QueryKey& key) const {
    std::shared_lock lock(mutex_);
    return entries_.find(key) != entries_.end();
}

std::optional<CacheEntry> QueryCache::get_entry(const QueryKey& key) const {
    std::shared_lock lock(mutex_);
    auto it = entries_.find(key);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void QueryCache::invalidate(const QueryKey& key) {
    std::unique_lock lock(mutex_);
    entries_.erase(key);
}

void QueryCache::invalidate_dependents(const QueryKey& key) {
    std::unique_lock lock(mutex_);

    // BFS to find all entries that transitively depend on `key`
    std::queue<QueryKey> worklist;
    std::unordered_set<QueryKey, QueryKeyHash, QueryKeyEqual> to_invalidate;

    worklist.push(key);
    to_invalidate.insert(key);

    while (!worklist.empty()) {
        auto current = worklist.front();
        worklist.pop();

        // Find all entries that have `current` in their dependencies
        for (const auto& [entry_key, entry] : entries_) {
            if (to_invalidate.count(entry_key) > 0) {
                continue;
            }
            for (const auto& dep : entry.dependencies) {
                if (dep == current) {
                    to_invalidate.insert(entry_key);
                    worklist.push(entry_key);
                    break;
                }
            }
        }
    }

    // Remove all invalidated entries
    for (const auto& k : to_invalidate) {
        entries_.erase(k);
    }
}

void QueryCache::clear() {
    std::unique_lock lock(mutex_);
    entries_.clear();
    hits_.store(0, std::memory_order_relaxed);
    misses_.store(0, std::memory_order_relaxed);
}

QueryCache::Stats QueryCache::get_stats() const {
    std::shared_lock lock(mutex_);
    return {entries_.size(), hits_.load(std::memory_order_relaxed),
            misses_.load(std::memory_order_relaxed)};
}

} // namespace tml::query
