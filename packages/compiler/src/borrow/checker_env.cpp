#include "borrow/checker.hpp"

#include <algorithm>

namespace tml::borrow {

// ============================================================================
// BorrowEnv Implementation
// ============================================================================

auto BorrowEnv::define(const std::string& name, types::TypePtr type, bool is_mut,
                       Location loc) -> PlaceId {
    PlaceId id = next_id_++;

    PlaceState state{
        .name = name,
        .type = std::move(type),
        .state = OwnershipState::Owned,
        .is_mutable = is_mut,
        .active_borrows = {},
        .definition = loc,
        .last_use = std::nullopt,
        .borrowed_from = std::nullopt,
        .moved_fields = {},
        .is_initialized = true,
    };

    places_[id] = std::move(state);
    name_to_place_[name].push_back(id);

    if (!scopes_.empty()) {
        scopes_.back().push_back(id);
    }

    return id;
}

auto BorrowEnv::lookup(const std::string& name) const -> std::optional<PlaceId> {
    auto it = name_to_place_.find(name);
    if (it == name_to_place_.end() || it->second.empty()) {
        return std::nullopt;
    }
    return it->second.back();
}

auto BorrowEnv::get_state(PlaceId id) const -> const PlaceState& {
    return places_.at(id);
}

auto BorrowEnv::get_state_mut(PlaceId id) -> PlaceState& {
    return places_.at(id);
}

void BorrowEnv::mark_used(PlaceId id, Location loc) {
    if (places_.count(id)) {
        places_[id].last_use = loc;
    }
}

void BorrowEnv::push_scope() {
    scopes_.emplace_back();
}

void BorrowEnv::pop_scope() {
    if (!scopes_.empty()) {
        // Remove names from current scope
        for (PlaceId id : scopes_.back()) {
            const auto& state = places_[id];
            auto it = name_to_place_.find(state.name);
            if (it != name_to_place_.end() && !it->second.empty()) {
                it->second.pop_back();
            }
        }
        scopes_.pop_back();
    }
}

auto BorrowEnv::current_scope_places() const -> const std::vector<PlaceId>& {
    static const std::vector<PlaceId> empty;
    return scopes_.empty() ? empty : scopes_.back();
}

void BorrowEnv::release_borrows_at_depth(size_t depth, Location loc) {
    // Iterate over all places and release borrows created at this scope depth
    for (auto& [id, state] : places_) {
        bool state_changed = false;
        for (auto& borrow : state.active_borrows) {
            if (!borrow.end && borrow.scope_depth == depth) {
                borrow.end = loc;
                state_changed = true;
            }
        }

        if (state_changed) {
            // Recompute ownership state
            bool has_active_mut = false;
            bool has_active_shared = false;
            for (const auto& borrow : state.active_borrows) {
                if (!borrow.end) {
                    if (borrow.kind == BorrowKind::Mutable)
                        has_active_mut = true;
                    else
                        has_active_shared = true;
                }
            }

            if (has_active_mut) {
                state.state = OwnershipState::MutBorrowed;
            } else if (has_active_shared) {
                state.state = OwnershipState::Borrowed;
            } else if (state.state == OwnershipState::Borrowed ||
                       state.state == OwnershipState::MutBorrowed) {
                state.state = OwnershipState::Owned;
            }
        }
    }
}

} // namespace tml::borrow
