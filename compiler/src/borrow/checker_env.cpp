//! # Borrow Checker Environment Implementation
//!
//! This file implements `BorrowEnv`, the environment that tracks the state of all
//! places (variables and memory locations) during borrow checking.
//!
//! ## Data Structures
//!
//! The environment maintains several key data structures:
//!
//! ```text
//! BorrowEnv
//! ├── name_to_place_: HashMap<String, Vec<PlaceId>>
//! │   └── Maps variable names to their PlaceIds (supports shadowing)
//! ├── places_: HashMap<PlaceId, PlaceState>
//! │   └── Maps PlaceIds to their full state
//! └── scopes_: Vec<Vec<PlaceId>>
//!     └── Stack of scopes, each containing PlaceIds defined in that scope
//! ```
//!
//! ## Variable Shadowing
//!
//! TML allows variable shadowing within nested scopes:
//!
//! ```tml
//! let x = 1              // x → PlaceId(0)
//! {
//!     let x = 2          // x → PlaceId(1), shadows PlaceId(0)
//!     println(x)         // Uses PlaceId(1)
//! }                      // PlaceId(1) goes out of scope
//! println(x)             // Uses PlaceId(0) again
//! ```
//!
//! This is implemented by storing a vector of PlaceIds for each name. The most
//! recent PlaceId is used for lookups, and is popped when its scope ends.
//!
//! ## Scope Management
//!
//! Scopes are managed as a stack. Each scope tracks which PlaceIds were defined
//! in it, allowing proper cleanup when the scope ends:
//!
//! ```text
//! push_scope() → [new empty scope]
//! define("x")  → scope gets PlaceId
//! pop_scope()  → removes PlaceId from name mapping
//! ```

#include "borrow/checker.hpp"

#include <algorithm>

namespace tml::borrow {

// ============================================================================
// BorrowEnv Implementation
// ============================================================================

/// Defines a new variable in the current scope.
///
/// Creates a new `PlaceState` for the variable and registers it in both the
/// name-to-place mapping and the current scope.
///
/// ## Parameters
///
/// - `name`: The variable name in source code
/// - `type`: The resolved type (may be nullptr if not yet known)
/// - `is_mut`: Whether the variable was declared with `mut`
/// - `loc`: Source location for error reporting
///
/// ## Returns
///
/// The unique `PlaceId` assigned to this variable.
///
/// ## Initial State
///
/// New variables start in `OwnershipState::Owned` with no active borrows.
auto BorrowEnv::define(const std::string& name, types::TypePtr type, bool is_mut, Location loc,
                       bool is_mut_ref) -> PlaceId {
    PlaceId id = next_id_++;

    PlaceState state{
        .name = name,
        .type = std::move(type),
        .state = OwnershipState::Owned,
        .is_mutable = is_mut,
        .is_mut_ref = is_mut_ref,
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

/// Looks up a place by name, returning the most recent definition.
///
/// Due to variable shadowing, the same name may refer to different PlaceIds
/// in different scopes. This method returns the innermost (most recent)
/// definition.
///
/// ## Returns
///
/// - `Some(PlaceId)` if the variable exists
/// - `None` if the variable is not defined
auto BorrowEnv::lookup(const std::string& name) const -> std::optional<PlaceId> {
    auto it = name_to_place_.find(name);
    if (it == name_to_place_.end() || it->second.empty()) {
        return std::nullopt;
    }
    return it->second.back();
}

/// Returns the state of a place (read-only).
auto BorrowEnv::get_state(PlaceId id) const -> const PlaceState& {
    return places_.at(id);
}

/// Returns the state of a place (mutable).
auto BorrowEnv::get_state_mut(PlaceId id) -> PlaceState& {
    return places_.at(id);
}

/// Marks a place as used at the given location.
///
/// This updates the `last_use` field for NLL (Non-Lexical Lifetimes) tracking.
/// The last use determines when a borrow can end, allowing more flexible
/// code to pass borrow checking.
///
/// ## NLL Example
///
/// ```tml
/// let mut x = 42
/// let r = ref x       // Borrow starts
/// println(r)          // Last use of r - marked here
/// x = 100             // OK! Borrow ended at last use, not at scope end
/// ```
void BorrowEnv::mark_used(PlaceId id, Location loc) {
    if (places_.count(id)) {
        places_[id].last_use = loc;
    }
}

/// Pushes a new scope onto the scope stack.
///
/// Called when entering a new lexical scope (function body, block, if branch,
/// loop body, etc.). Variables defined in this scope will be tracked and
/// cleaned up when `pop_scope()` is called.
void BorrowEnv::push_scope() {
    scopes_.emplace_back();
}

/// Pops the current scope, cleaning up all variables defined in it.
///
/// This method:
/// 1. Removes all variable names defined in this scope from the lookup table
/// 2. Handles shadowing by only removing the innermost definition
/// 3. Removes the scope from the stack
///
/// Note: The PlaceState itself is NOT removed from `places_` because we may
/// still need it for error reporting (e.g., "value moved here").
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

/// Returns the places defined in the current scope.
///
/// Used by the borrow checker to determine which places need to be dropped
/// when a scope ends.
auto BorrowEnv::current_scope_places() const -> const std::vector<PlaceId>& {
    static const std::vector<PlaceId> empty;
    return scopes_.empty() ? empty : scopes_.back();
}

/// Releases all borrows created at the given scope depth.
///
/// When a scope ends, all borrows created within that scope must end. This
/// method finds all such borrows and marks them as ended, then recomputes
/// the ownership state of affected places.
///
/// ## State Recomputation
///
/// After releasing borrows, the ownership state is updated:
/// - If any mutable borrow remains → `MutBorrowed`
/// - Else if any shared borrow remains → `Borrowed`
/// - Else → `Owned` (if was previously borrowed)
///
/// ## Parameters
///
/// - `depth`: The scope depth (obtained from `scope_depth()`)
/// - `loc`: The location where the scope ends (for lifetime tracking)
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
