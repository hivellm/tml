#include "borrow/checker.hpp"

namespace tml::borrow {

// ============================================================================
// BorrowError Static Helpers - Create rich diagnostics for common error patterns
// ============================================================================

auto BorrowError::use_after_move(const std::string& name, SourceSpan use_span, SourceSpan move_span)
    -> BorrowError {
    BorrowError err;
    err.code = BorrowErrorCode::UseAfterMove;
    err.message = "use of moved value: `" + name + "`";
    err.span = use_span;
    err.related_span = move_span;
    err.related_message = "value moved here";
    err.notes.push_back("move occurs because `" + name +
                        "` has type that does not implement the `Duplicate` behavior");
    err.suggestions.push_back(BorrowSuggestion{
        .message = "consider cloning the value before the move",
        .fix = ".duplicate()",
    });
    return err;
}

auto BorrowError::double_mut_borrow(const std::string& name, SourceSpan second_span,
                                    SourceSpan first_span) -> BorrowError {
    BorrowError err;
    err.code = BorrowErrorCode::DoubleMutBorrow;
    err.message = "cannot borrow `" + name + "` as mutable more than once at a time";
    err.span = second_span;
    err.related_span = first_span;
    err.related_message = "first mutable borrow occurs here";
    err.notes.push_back("first borrow is still active when second borrow occurs");
    err.suggestions.push_back(BorrowSuggestion{
        .message = "consider borrowing at different scopes, or using interior mutability",
        .fix = std::nullopt,
    });
    return err;
}

auto BorrowError::mut_borrow_while_immut(const std::string& name, SourceSpan mut_span,
                                         SourceSpan immut_span) -> BorrowError {
    BorrowError err;
    err.code = BorrowErrorCode::MutBorrowWhileImmut;
    err.message =
        "cannot borrow `" + name + "` as mutable because it is also borrowed as immutable";
    err.span = mut_span;
    err.related_span = immut_span;
    err.related_message = "immutable borrow occurs here";
    err.notes.push_back("immutable borrow is still active when mutable borrow occurs");
    return err;
}

auto BorrowError::immut_borrow_while_mut(const std::string& name, SourceSpan immut_span,
                                         SourceSpan mut_span) -> BorrowError {
    BorrowError err;
    err.code = BorrowErrorCode::ImmutBorrowWhileMut;
    err.message =
        "cannot borrow `" + name + "` as immutable because it is also borrowed as mutable";
    err.span = immut_span;
    err.related_span = mut_span;
    err.related_message = "mutable borrow occurs here";
    err.notes.push_back("mutable borrow is still active when immutable borrow occurs");
    return err;
}

auto BorrowError::return_local_ref(const std::string& name, SourceSpan return_span,
                                   SourceSpan def_span) -> BorrowError {
    BorrowError err;
    err.code = BorrowErrorCode::ReturnLocalRef;
    err.message = "cannot return reference to local variable `" + name + "`";
    err.span = return_span;
    err.related_span = def_span;
    err.related_message = "`" + name + "` is declared here";
    err.notes.push_back("returns a reference to data owned by the current function");
    err.suggestions.push_back(BorrowSuggestion{
        .message = "consider returning an owned value instead",
        .fix = std::nullopt,
    });
    return err;
}

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
        // Use rich error with move location
        if (state.move_location) {
            errors_.push_back(
                BorrowError::use_after_move(state.name, loc.span, state.move_location->span));
        } else {
            error("use of moved value: `" + state.name + "`", loc.span);
        }
        return;
    }

    if (state.state == OwnershipState::Borrowed || state.state == OwnershipState::MutBorrowed) {
        // Find the active borrow span
        for (const auto& borrow : state.active_borrows) {
            if (!borrow.end) {
                BorrowError err;
                err.code = BorrowErrorCode::MoveWhileBorrowed;
                err.message = "cannot move out of `" + state.name + "` because it is borrowed";
                err.span = loc.span;
                err.related_span = borrow.start.span;
                err.related_message = "borrow of `" + state.name + "` occurs here";
                errors_.push_back(err);
                return;
            }
        }
        error("cannot move out of `" + state.name + "` because it is borrowed", loc.span);
        return;
    }

    state.state = OwnershipState::Moved;
    state.move_location = loc; // Track where the move happened
}

void BorrowChecker::check_can_use(PlaceId place, Location loc) {
    const auto& state = env_.get_state(place);

    if (state.state == OwnershipState::Moved) {
        if (state.move_location) {
            errors_.push_back(
                BorrowError::use_after_move(state.name, loc.span, state.move_location->span));
        } else {
            error("use of moved value: `" + state.name + "`", loc.span);
        }
    }

    if (state.state == OwnershipState::Dropped) {
        error("use of dropped value: `" + state.name + "`", loc.span);
    }
}

void BorrowChecker::check_can_mutate(PlaceId place, Location loc) {
    const auto& state = env_.get_state(place);

    if (!state.is_mutable) {
        BorrowError err;
        err.code = BorrowErrorCode::AssignNotMutable;
        err.message = "cannot assign to `" + state.name + "` because it is not mutable";
        err.span = loc.span;
        err.related_span = state.definition.span;
        err.related_message = "`" + state.name + "` is declared here";
        err.suggestions.push_back(BorrowSuggestion{
            .message = "consider declaring as mutable",
            .fix = "mut " + state.name,
        });
        errors_.push_back(err);
        return;
    }

    if (state.state == OwnershipState::Moved) {
        if (state.move_location) {
            BorrowError err;
            err.code = BorrowErrorCode::UseAfterMove;
            err.message = "cannot assign to moved value: `" + state.name + "`";
            err.span = loc.span;
            err.related_span = state.move_location->span;
            err.related_message = "value moved here";
            errors_.push_back(err);
        } else {
            error("cannot assign to moved value: `" + state.name + "`", loc.span);
        }
        return;
    }

    if (state.state == OwnershipState::Borrowed) {
        // Find the active immutable borrow
        for (const auto& borrow : state.active_borrows) {
            if (!borrow.end && borrow.kind == BorrowKind::Shared) {
                BorrowError err;
                err.code = BorrowErrorCode::AssignWhileBorrowed;
                err.message = "cannot assign to `" + state.name + "` because it is borrowed";
                err.span = loc.span;
                err.related_span = borrow.start.span;
                err.related_message = "immutable borrow occurs here";
                errors_.push_back(err);
                return;
            }
        }
        error("cannot assign to `" + state.name + "` because it is borrowed", loc.span);
        return;
    }

    if (state.state == OwnershipState::MutBorrowed) {
        // Find the active mutable borrow
        for (const auto& borrow : state.active_borrows) {
            if (!borrow.end && borrow.kind == BorrowKind::Mutable) {
                BorrowError err;
                err.code = BorrowErrorCode::AssignWhileBorrowed;
                err.message =
                    "cannot assign to `" + state.name + "` because it is mutably borrowed";
                err.span = loc.span;
                err.related_span = borrow.start.span;
                err.related_message = "mutable borrow occurs here";
                errors_.push_back(err);
                return;
            }
        }
        error("cannot assign to `" + state.name + "` because it is mutably borrowed", loc.span);
        return;
    }
}

void BorrowChecker::check_can_borrow(PlaceId place, BorrowKind kind, Location loc) {
    const auto& state = env_.get_state(place);

    if (state.state == OwnershipState::Moved) {
        BorrowError err;
        err.code = BorrowErrorCode::BorrowAfterMove;
        err.message = "cannot borrow moved value: `" + state.name + "`";
        err.span = loc.span;
        if (state.move_location) {
            err.related_span = state.move_location->span;
            err.related_message = "value moved here";
        }
        errors_.push_back(err);
        return;
    }

    // Check if this is a reborrow (borrowing from a reference)
    // Reborrows are allowed: you can create &T from &mut T, or &mut T from &mut T
    bool is_reborrow = state.borrowed_from.has_value();

    if (kind == BorrowKind::Mutable) {
        if (!state.is_mutable && !is_reborrow) {
            BorrowError err;
            err.code = BorrowErrorCode::MutBorrowNotMutable;
            err.message = "cannot borrow `" + state.name +
                          "` as mutable because it is not declared as mutable";
            err.span = loc.span;
            err.related_span = state.definition.span;
            err.related_message = "`" + state.name + "` is declared here";
            err.suggestions.push_back(BorrowSuggestion{
                .message = "consider declaring as mutable",
                .fix = "mut " + state.name,
            });
            errors_.push_back(err);
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
            // Find the active immutable borrow for related span
            for (const auto& borrow : state.active_borrows) {
                if (!borrow.end && borrow.kind == BorrowKind::Shared) {
                    errors_.push_back(BorrowError::mut_borrow_while_immut(state.name, loc.span,
                                                                          borrow.start.span));
                    return;
                }
            }
            error("cannot borrow `" + state.name +
                      "` as mutable because it is also borrowed as immutable",
                  loc.span);
            return;
        }

        // Allow two-phase borrows: during method calls, we can have a mutable borrow
        // that is temporarily shared while evaluating arguments
        if (state.state == OwnershipState::MutBorrowed && !is_two_phase_borrow_active_) {
            // Find the active mutable borrow for related span
            for (const auto& borrow : state.active_borrows) {
                if (!borrow.end && borrow.kind == BorrowKind::Mutable) {
                    errors_.push_back(
                        BorrowError::double_mut_borrow(state.name, loc.span, borrow.start.span));
                    return;
                }
            }
            error("cannot borrow `" + state.name + "` as mutable more than once at a time",
                  loc.span);
            return;
        }
    } else {
        // Shared borrow
        // Allow shared reborrow from mutable borrow (coercion &mut T -> &T)
        if (state.state == OwnershipState::MutBorrowed && !is_reborrow &&
            !is_two_phase_borrow_active_) {
            // Find the active mutable borrow for related span
            for (const auto& borrow : state.active_borrows) {
                if (!borrow.end && borrow.kind == BorrowKind::Mutable) {
                    errors_.push_back(BorrowError::immut_borrow_while_mut(state.name, loc.span,
                                                                          borrow.start.span));
                    return;
                }
            }
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
