#include "tml/borrow/checker.hpp"

namespace tml::borrow {

// ============================================================================
// Place Implementation
// ============================================================================

auto Place::is_prefix_of(const Place& other) const -> bool {
    // A place is a prefix if it has the same base and its projections
    // are a prefix of the other's projections
    if (base != other.base) return false;
    if (projections.size() > other.projections.size()) return false;

    for (size_t i = 0; i < projections.size(); ++i) {
        if (projections[i].kind != other.projections[i].kind) return false;
        if (projections[i].kind == ProjectionKind::Field &&
            projections[i].field_name != other.projections[i].field_name) {
            return false;
        }
    }
    return true;
}

auto Place::overlaps_with(const Place& other) const -> bool {
    // Two places overlap if one is a prefix of the other
    // Examples:
    //   x overlaps with x.field (x is prefix of x.field)
    //   x.field overlaps with x (x.field has x as prefix)
    //   x.a does NOT overlap with x.b (different fields)
    return is_prefix_of(other) || other.is_prefix_of(*this);
}

auto Place::to_string(const std::string& base_name) const -> std::string {
    std::string result = base_name;
    for (const auto& proj : projections) {
        switch (proj.kind) {
            case ProjectionKind::Field:
                result += "." + proj.field_name;
                break;
            case ProjectionKind::Index:
                result += "[...]";
                break;
            case ProjectionKind::Deref:
                result = "*" + result;
                break;
        }
    }
    return result;
}

// ============================================================================
// BorrowEnv NLL Methods
// ============================================================================

void BorrowEnv::mark_ref_used(PlaceId ref_place, Location loc) {
    // Find all borrows that were created with this ref_place and update their last_use
    for (auto& [id, state] : places_) {
        for (auto& borrow : state.active_borrows) {
            if (borrow.ref_place == ref_place && !borrow.end) {
                borrow.last_use = loc;
            }
        }
    }
}

void BorrowEnv::release_dead_borrows(Location loc) {
    // NLL: Release borrows whose last_use is before the current location
    for (auto& [id, state] : places_) {
        for (auto& borrow : state.active_borrows) {
            if (!borrow.end && borrow.last_use) {
                // If we have a last_use and current location is past it, end the borrow
                if (*borrow.last_use < loc) {
                    borrow.end = *borrow.last_use;
                }
            }
        }

        // Recompute ownership state based on remaining active borrows
        bool has_active_mut = false;
        bool has_active_shared = false;
        for (const auto& borrow : state.active_borrows) {
            if (!borrow.end) {
                if (borrow.kind == BorrowKind::Mutable) has_active_mut = true;
                else has_active_shared = true;
            }
        }

        if (state.state == OwnershipState::Borrowed ||
            state.state == OwnershipState::MutBorrowed) {
            if (has_active_mut) {
                state.state = OwnershipState::MutBorrowed;
            } else if (has_active_shared) {
                state.state = OwnershipState::Borrowed;
            } else {
                state.state = OwnershipState::Owned;
            }
        }
    }
}

auto BorrowEnv::is_borrow_live(const Borrow& borrow, Location loc) const -> bool {
    // A borrow is live if:
    // 1. It hasn't ended (end is nullopt)
    // 2. AND either has no last_use OR current loc <= last_use
    if (borrow.end) return false;

    if (borrow.last_use) {
        return loc <= *borrow.last_use;
    }

    // No last_use recorded yet - conservatively assume live
    return true;
}

// ============================================================================
// Partial Move Tracking
// ============================================================================

void BorrowEnv::mark_field_moved(PlaceId id, const std::string& field) {
    auto& state = get_state_mut(id);
    state.moved_fields.insert(field);

    // If any field is moved, mark as partially moved
    // We don't change to FullyMoved because other fields are still valid
}

auto BorrowEnv::get_move_state(PlaceId id) const -> MoveState {
    const auto& state = get_state(id);

    if (state.state == OwnershipState::Moved) {
        return MoveState::FullyMoved;
    }

    if (!state.moved_fields.empty()) {
        return MoveState::PartiallyMoved;
    }

    return MoveState::FullyOwned;
}

auto BorrowEnv::is_field_moved(PlaceId id, const std::string& field) const -> bool {
    const auto& state = get_state(id);
    return state.moved_fields.count(field) > 0;
}

// ============================================================================
// BorrowChecker NLL Methods
// ============================================================================

void BorrowChecker::apply_nll(Location loc) {
    // Release borrows that are no longer needed at this location
    env_.release_dead_borrows(loc);
}

void BorrowChecker::create_borrow_with_projection(PlaceId place, const Place& full_place,
                                                   BorrowKind kind, Location loc,
                                                   PlaceId ref_place) {
    auto& state = env_.get_state_mut(place);

    Borrow borrow{
        .place = place,
        .full_place = full_place,
        .kind = kind,
        .start = loc,
        .end = std::nullopt,
        .last_use = std::nullopt,  // Will be updated by NLL tracking
        .scope_depth = env_.scope_depth(),
        .lifetime = env_.next_lifetime_id(),
        .ref_place = ref_place,
    };

    state.active_borrows.push_back(borrow);

    // Track the reference relationship
    ref_to_borrowed_[ref_place] = place;

    if (kind == BorrowKind::Mutable) {
        state.state = OwnershipState::MutBorrowed;
    } else {
        if (state.state == OwnershipState::Owned) {
            state.state = OwnershipState::Borrowed;
        }
    }
}

void BorrowChecker::check_can_borrow_with_projection(PlaceId place, const Place& full_place,
                                                      BorrowKind kind, Location loc) {
    const auto& state = env_.get_state(place);

    if (state.state == OwnershipState::Moved) {
        error("cannot borrow moved value: `" + state.name + "`", loc.span);
        return;
    }

    // Check for partial moves
    if (!full_place.projections.empty()) {
        // Borrowing a field - check if that specific field was moved
        if (full_place.projections[0].kind == ProjectionKind::Field) {
            const auto& field = full_place.projections[0].field_name;
            if (env_.is_field_moved(place, field)) {
                error("cannot borrow `" + state.name + "." + field +
                      "` because it has been moved", loc.span);
                return;
            }
        }
    } else {
        // Borrowing the whole value - check if any part is moved
        if (env_.get_move_state(place) == MoveState::PartiallyMoved) {
            error("cannot borrow `" + state.name +
                  "` because part of it has been moved", loc.span);
            return;
        }
    }

    // Check for conflicting borrows with projection awareness
    bool is_reborrow = state.borrowed_from.has_value();

    for (const auto& existing_borrow : state.active_borrows) {
        if (existing_borrow.end) continue;  // Borrow has ended

        // Check if the places overlap
        if (!existing_borrow.full_place.overlaps_with(full_place)) {
            // Different fields - no conflict (e.g., x.a and x.b)
            continue;
        }

        // Overlapping places - check for conflicts
        if (kind == BorrowKind::Mutable) {
            // Mutable borrow conflicts with any existing borrow on overlapping place
            if (!is_two_phase_borrow_active_) {
                if (existing_borrow.kind == BorrowKind::Mutable) {
                    error("cannot borrow `" + get_place_name(full_place) +
                          "` as mutable more than once at a time", loc.span);
                    return;
                } else {
                    error("cannot borrow `" + get_place_name(full_place) +
                          "` as mutable because it is also borrowed as immutable", loc.span);
                    return;
                }
            }
        } else {
            // Shared borrow conflicts with mutable borrow
            if (existing_borrow.kind == BorrowKind::Mutable && !is_two_phase_borrow_active_) {
                error("cannot borrow `" + get_place_name(full_place) +
                      "` as immutable because it is also borrowed as mutable", loc.span);
                return;
            }
        }
    }

    if (kind == BorrowKind::Mutable) {
        if (!state.is_mutable && !is_reborrow) {
            error("cannot borrow `" + state.name +
                  "` as mutable because it is not declared as mutable", loc.span);
            return;
        }

        // For reborrows from mutable references, allow creating new mutable borrows
        if (is_reborrow && state.borrowed_from->second == BorrowKind::Shared) {
            error("cannot reborrow `" + state.name +
                  "` as mutable because it was borrowed as immutable", loc.span);
            return;
        }
    }
}

void BorrowChecker::move_field(PlaceId place, const std::string& field, Location loc) {
    auto& state = env_.get_state_mut(place);

    // Check if already moved
    if (state.state == OwnershipState::Moved) {
        error("use of moved value: `" + state.name + "`", loc.span);
        return;
    }

    // Check if this specific field was already moved
    if (env_.is_field_moved(place, field)) {
        error("use of moved value: `" + state.name + "." + field + "`", loc.span);
        return;
    }

    // Check if borrowed
    if (state.state == OwnershipState::Borrowed ||
        state.state == OwnershipState::MutBorrowed) {
        // Check if the borrow is on this specific field or the whole thing
        for (const auto& borrow : state.active_borrows) {
            if (borrow.end) continue;

            // If borrowing the whole value, can't move any field
            if (borrow.full_place.projections.empty()) {
                error("cannot move out of `" + state.name + "." + field +
                      "` because `" + state.name + "` is borrowed", loc.span);
                return;
            }

            // If borrowing this specific field, can't move it
            if (!borrow.full_place.projections.empty() &&
                borrow.full_place.projections[0].kind == ProjectionKind::Field &&
                borrow.full_place.projections[0].field_name == field) {
                error("cannot move out of `" + state.name + "." + field +
                      "` because it is borrowed", loc.span);
                return;
            }
        }
    }

    // Mark field as moved
    env_.mark_field_moved(place, field);
}

void BorrowChecker::check_can_use_field(PlaceId place, const std::string& field, Location loc) {
    const auto& state = env_.get_state(place);

    if (state.state == OwnershipState::Moved) {
        error("use of moved value: `" + state.name + "`", loc.span);
        return;
    }

    if (env_.is_field_moved(place, field)) {
        error("use of moved value: `" + state.name + "." + field + "`", loc.span);
        return;
    }

    if (state.state == OwnershipState::Dropped) {
        error("use of dropped value: `" + state.name + "`", loc.span);
    }
}

void BorrowChecker::check_return_borrows(const parser::ReturnExpr& ret) {
    if (!ret.value) return;

    auto loc = current_location(ret.span);

    // Check if we're returning a reference to a local variable
    // This would be a dangling reference

    // Extract the expression being returned
    const auto& value = *ret.value;

    // If it's a reference expression, check what it references
    if (value->is<parser::UnaryExpr>()) {
        const auto& unary = value->as<parser::UnaryExpr>();
        if (unary.op == parser::UnaryOp::Ref || unary.op == parser::UnaryOp::RefMut) {
            // Check if the referenced value is a local variable
            if (unary.operand->is<parser::IdentExpr>()) {
                const auto& ident = unary.operand->as<parser::IdentExpr>();
                auto place_id = env_.lookup(ident.name);
                if (place_id) {
                    // Check if this variable will be dropped when function returns
                    // All local variables will be dropped, so this is always an error
                    const auto& state = env_.get_state(*place_id);
                    error("cannot return reference to local variable `" + state.name +
                          "` as it will be dropped when the function returns", loc.span);
                }
            }
        }
    }

    // If returning an identifier that is itself a reference, check its borrowed_from
    if (value->is<parser::IdentExpr>()) {
        const auto& ident = value->as<parser::IdentExpr>();
        auto place_id = env_.lookup(ident.name);
        if (place_id) {
            const auto& state = env_.get_state(*place_id);
            if (state.borrowed_from) {
                // This is a reference - check if what it borrows from will be valid
                auto borrowed_id = state.borrowed_from->first;
                const auto& borrowed_state = env_.get_state(borrowed_id);

                // Check if the borrowed value is a local that will be dropped
                // (This is a simplified check - full lifetime analysis would be more complex)
                // For now, we only error if it's obviously a local
                bool is_local = true;  // All variables in current scope are locals

                // Check scope depth - if borrowed value is in current function scope
                // and not a parameter, it will be dropped
                // (Parameters have scope_depth 1 in a function)
                if (is_local && borrowed_state.definition.statement_index > 0) {
                    // Local variable defined after function start - will be dropped
                    error("cannot return reference that borrows from local variable `" +
                          borrowed_state.name + "`", loc.span);
                }
            }
        }
    }
}

auto BorrowChecker::extract_place(const parser::Expr& expr) -> std::optional<Place> {
    if (expr.is<parser::IdentExpr>()) {
        const auto& ident = expr.as<parser::IdentExpr>();
        auto place_id = env_.lookup(ident.name);
        if (place_id) {
            return Place{*place_id, {}};
        }
        return std::nullopt;
    }

    if (expr.is<parser::FieldExpr>()) {
        const auto& field_expr = expr.as<parser::FieldExpr>();
        auto base_place = extract_place(*field_expr.object);
        if (base_place) {
            base_place->projections.push_back(Projection{
                ProjectionKind::Field,
                field_expr.field
            });
            return base_place;
        }
        return std::nullopt;
    }

    if (expr.is<parser::IndexExpr>()) {
        const auto& index = expr.as<parser::IndexExpr>();
        auto base_place = extract_place(*index.object);
        if (base_place) {
            base_place->projections.push_back(Projection{
                ProjectionKind::Index,
                ""
            });
            return base_place;
        }
        return std::nullopt;
    }

    if (expr.is<parser::UnaryExpr>()) {
        const auto& unary = expr.as<parser::UnaryExpr>();
        if (unary.op == parser::UnaryOp::Deref) {
            auto base_place = extract_place(*unary.operand);
            if (base_place) {
                base_place->projections.push_back(Projection{
                    ProjectionKind::Deref,
                    ""
                });
                return base_place;
            }
        }
        return std::nullopt;
    }

    return std::nullopt;
}

auto BorrowChecker::get_place_name(const Place& place) const -> std::string {
    auto it = env_.all_places().find(place.base);
    if (it == env_.all_places().end()) {
        return "<unknown>";
    }
    return place.to_string(it->second.name);
}

} // namespace tml::borrow
