#include "borrow/checker.hpp"

namespace tml::borrow {

void BorrowChecker::create_borrow(PlaceId place, BorrowKind kind, Location loc) {
    auto& state = env_.get_state_mut(place);

    // Create a simple place with no projections
    Place full_place{place, {}};

    Borrow borrow{
        .place = place,
        .full_place = full_place,
        .kind = kind,
        .start = loc,
        .end = std::nullopt,
        .last_use = std::nullopt, // NLL: Will be updated when reference is used
        .scope_depth = env_.scope_depth(),
        .lifetime = env_.next_lifetime_id(),
        .ref_place = 0, // Will be set when reference is stored in a variable
    };

    state.active_borrows.push_back(borrow);

    if (kind == BorrowKind::Mutable) {
        state.state = OwnershipState::MutBorrowed;
    } else {
        if (state.state == OwnershipState::Owned) {
            state.state = OwnershipState::Borrowed;
        }
    }
}

void BorrowChecker::release_borrow(PlaceId place, BorrowKind kind, Location loc) {
    auto& state = env_.get_state_mut(place);

    // Find and end the borrow
    for (auto& borrow : state.active_borrows) {
        if (borrow.kind == kind && !borrow.end) {
            borrow.end = loc;
            break;
        }
    }

    // Update ownership state
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
    } else {
        state.state = OwnershipState::Owned;
    }
}

void BorrowChecker::move_value(PlaceId place, Location loc) {
    auto& state = env_.get_state_mut(place);

    if (state.state == OwnershipState::Moved) {
        error("use of moved value: `" + state.name + "`", loc.span);
        return;
    }

    if (state.state == OwnershipState::Borrowed || state.state == OwnershipState::MutBorrowed) {
        error("cannot move out of `" + state.name + "` because it is borrowed", loc.span);
        return;
    }

    state.state = OwnershipState::Moved;
}

void BorrowChecker::check_can_use(PlaceId place, Location loc) {
    const auto& state = env_.get_state(place);

    if (state.state == OwnershipState::Moved) {
        error("use of moved value: `" + state.name + "`", loc.span);
    }

    if (state.state == OwnershipState::Dropped) {
        error("use of dropped value: `" + state.name + "`", loc.span);
    }
}

void BorrowChecker::check_can_mutate(PlaceId place, Location loc) {
    const auto& state = env_.get_state(place);

    if (!state.is_mutable) {
        error("cannot assign to `" + state.name + "` because it is not mutable", loc.span);
        return;
    }

    if (state.state == OwnershipState::Moved) {
        error("cannot assign to moved value: `" + state.name + "`", loc.span);
        return;
    }

    if (state.state == OwnershipState::Borrowed) {
        error("cannot assign to `" + state.name + "` because it is borrowed", loc.span);
        return;
    }

    if (state.state == OwnershipState::MutBorrowed) {
        error("cannot assign to `" + state.name + "` because it is mutably borrowed", loc.span);
        return;
    }
}

void BorrowChecker::check_can_borrow(PlaceId place, BorrowKind kind, Location loc) {
    const auto& state = env_.get_state(place);

    if (state.state == OwnershipState::Moved) {
        error("cannot borrow moved value: `" + state.name + "`", loc.span);
        return;
    }

    // Check if this is a reborrow (borrowing from a reference)
    // Reborrows are allowed: you can create &T from &mut T, or &mut T from &mut T
    bool is_reborrow = state.borrowed_from.has_value();

    if (kind == BorrowKind::Mutable) {
        if (!state.is_mutable && !is_reborrow) {
            error("cannot borrow `" + state.name +
                      "` as mutable because it is not declared as mutable",
                  loc.span);
            return;
        }

        // For reborrows from mutable references, allow creating new mutable borrows
        if (is_reborrow && state.borrowed_from->second == BorrowKind::Shared) {
            error("cannot reborrow `" + state.name +
                      "` as mutable because it was borrowed as immutable",
                  loc.span);
            return;
        }

        if (state.state == OwnershipState::Borrowed && !is_reborrow) {
            error("cannot borrow `" + state.name +
                      "` as mutable because it is also borrowed as immutable",
                  loc.span);
            return;
        }

        // Allow two-phase borrows: during method calls, we can have a mutable borrow
        // that is temporarily shared while evaluating arguments
        if (state.state == OwnershipState::MutBorrowed && !is_two_phase_borrow_active_) {
            error("cannot borrow `" + state.name + "` as mutable more than once at a time",
                  loc.span);
            return;
        }
    } else {
        // Shared borrow
        // Allow shared reborrow from mutable borrow (coercion &mut T -> &T)
        if (state.state == OwnershipState::MutBorrowed && !is_reborrow &&
            !is_two_phase_borrow_active_) {
            error("cannot borrow `" + state.name +
                      "` as immutable because it is also borrowed as mutable",
                  loc.span);
            return;
        }
    }
}

void BorrowChecker::create_reborrow(PlaceId source, PlaceId target, BorrowKind kind, Location loc) {
    auto& target_state = env_.get_state_mut(target);
    target_state.borrowed_from = std::make_pair(source, kind);

    // Create a borrow on the source
    create_borrow(source, kind, loc);
}

void BorrowChecker::begin_two_phase_borrow() {
    is_two_phase_borrow_active_ = true;
}

void BorrowChecker::end_two_phase_borrow() {
    is_two_phase_borrow_active_ = false;
}

void BorrowChecker::drop_scope_places() {
    auto loc = Location{current_stmt_, SourceSpan{}};

    // First, release all borrows that were created at the current scope depth
    // This handles cases like: { let r = ref x; } - when scope ends, x is no longer borrowed
    env_.release_borrows_at_depth(env_.scope_depth(), loc);

    // Then mark all places in the current scope as dropped
    for (PlaceId place : env_.current_scope_places()) {
        auto& state = env_.get_state_mut(place);

        // Release any active borrows on this place (from inner scopes that weren't cleaned up)
        for (auto& borrow : state.active_borrows) {
            if (!borrow.end) {
                borrow.end = loc;
            }
        }

        state.state = OwnershipState::Dropped;
    }
}

void BorrowChecker::error(const std::string& message, SourceSpan span) {
    errors_.push_back(BorrowError{
        .message = message,
        .span = span,
        .notes = {},
        .related_span = std::nullopt,
    });
}

void BorrowChecker::error_with_note(const std::string& message, SourceSpan span,
                                    const std::string& note, SourceSpan note_span) {
    errors_.push_back(BorrowError{
        .message = message,
        .span = span,
        .notes = {note},
        .related_span = note_span,
    });
}

auto BorrowChecker::current_location(SourceSpan span) const -> Location {
    return Location{current_stmt_, span};
}

} // namespace tml::borrow
