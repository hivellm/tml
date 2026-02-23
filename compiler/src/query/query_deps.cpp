TML_MODULE("compiler")

#include "query/query_deps.hpp"

namespace tml::query {

void DependencyTracker::push_active(const QueryKey& key) {
    std::lock_guard lock(mutex_);
    active_stack_.push_back(key);
    state_stack_.push_back({key, {}});
}

void DependencyTracker::pop_active() {
    std::lock_guard lock(mutex_);
    if (!active_stack_.empty()) {
        active_stack_.pop_back();
        state_stack_.pop_back();
    }
}

void DependencyTracker::record_dependency(const QueryKey& callee) {
    std::lock_guard lock(mutex_);
    if (!state_stack_.empty()) {
        state_stack_.back().dependencies.push_back(callee);
    }
}

std::vector<QueryKey> DependencyTracker::current_dependencies() const {
    std::lock_guard lock(mutex_);
    if (state_stack_.empty()) {
        return {};
    }
    return state_stack_.back().dependencies;
}

std::optional<std::vector<QueryKey>> DependencyTracker::detect_cycle(const QueryKey& key) const {
    std::lock_guard lock(mutex_);

    // Check if `key` is already on the active stack
    for (size_t i = 0; i < active_stack_.size(); ++i) {
        if (active_stack_[i] == key) {
            // Found a cycle: return the path from the first occurrence to the end + key
            std::vector<QueryKey> cycle;
            for (size_t j = i; j < active_stack_.size(); ++j) {
                cycle.push_back(active_stack_[j]);
            }
            cycle.push_back(key); // Close the cycle
            return cycle;
        }
    }
    return std::nullopt;
}

size_t DependencyTracker::depth() const {
    std::lock_guard lock(mutex_);
    return active_stack_.size();
}

void DependencyTracker::clear() {
    std::lock_guard lock(mutex_);
    active_stack_.clear();
    state_stack_.clear();
}

} // namespace tml::query
