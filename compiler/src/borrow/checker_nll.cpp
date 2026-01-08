//! # Non-Lexical Lifetimes (NLL) and Advanced Borrow Checking
//!
//! This file implements Non-Lexical Lifetimes (NLL), partial move tracking,
//! projection-aware borrowing, and dangling reference detection.
//!
//! ## Non-Lexical Lifetimes
//!
//! Traditional (lexical) borrow checking ties borrow lifetimes to lexical scopes:
//!
//! ```tml
//! // Lexical lifetimes (restrictive)
//! let mut x = 5
//! let r = ref x       // borrow starts
//! println(r)          // last use of r
//! x = 10              // ERROR: r is still "live" until scope ends
//! }                   // borrow ends here
//! ```
//!
//! NLL instead ends borrows at their last use:
//!
//! ```tml
//! // Non-lexical lifetimes (permissive)
//! let mut x = 5
//! let r = ref x       // borrow starts
//! println(r)          // last use of r - borrow ENDS here
//! x = 10              // OK! borrow has ended
//! ```
//!
//! ## Projection-Aware Borrowing
//!
//! Places can have projections that access sub-parts:
//! - `x.field` - Field projection
//! - `x[i]` - Index projection
//! - `*x` - Deref projection
//!
//! Different fields can be borrowed independently:
//!
//! ```tml
//! let mut s = Struct { a: 1, b: 2 }
//! let ra = ref s.a    // borrows only s.a
//! let rb = ref s.b    // OK! s.b is separate from s.a
//! ```
//!
//! ## Partial Moves
//!
//! Structs can be partially moved, leaving some fields invalid:
//!
//! ```tml
//! let p = Pair { first: String::from("a"), second: String::from("b") }
//! let f = p.first     // moves p.first
//! println(p.second)   // OK: p.second not moved
//! println(p.first)    // ERROR: p.first was moved
//! drop(p)             // ERROR: p is partially moved
//! ```

#include "borrow/checker.hpp"

namespace tml::borrow {

// ============================================================================
// Place Implementation
// ============================================================================

/// Checks if this place is a prefix of another place.
///
/// A place P1 is a prefix of P2 if accessing P2 requires first accessing P1.
/// For example:
/// - `x` is a prefix of `x.field` (must access x to get x.field)
/// - `x.a` is NOT a prefix of `x.b` (different fields)
///
/// ## Algorithm
///
/// 1. Check base variables match
/// 2. Check this place's projections are a prefix of other's projections
/// 3. For Field projections, field names must match
auto Place::is_prefix_of(const Place& other) const -> bool {
    // A place is a prefix if it has the same base and its projections
    // are a prefix of the other's projections
    if (base != other.base)
        return false;
    if (projections.size() > other.projections.size())
        return false;

    for (size_t i = 0; i < projections.size(); ++i) {
        if (projections[i].kind != other.projections[i].kind)
            return false;
        if (projections[i].kind == ProjectionKind::Field &&
            projections[i].field_name != other.projections[i].field_name) {
            return false;
        }
    }
    return true;
}

/// Checks if two places overlap (could conflict in borrowing).
///
/// Two places overlap if borrowing one would affect the other. This happens
/// when one is a prefix of the other:
///
/// | Place 1   | Place 2   | Overlap? | Why                           |
/// |-----------|-----------|----------|-------------------------------|
/// | `x`       | `x.field` | Yes      | x contains x.field            |
/// | `x.a`     | `x.b`     | No       | Different fields              |
/// | `x[0]`    | `x[1]`    | Yes*     | Can't distinguish at compile time |
/// | `x.a.b`   | `x.a`     | Yes      | x.a contains x.a.b            |
///
/// *Array indices are conservatively treated as overlapping because the
/// borrow checker doesn't track concrete index values.
auto Place::overlaps_with(const Place& other) const -> bool {
    // Two places overlap if one is a prefix of the other
    // Examples:
    //   x overlaps with x.field (x is prefix of x.field)
    //   x.field overlaps with x (x.field has x as prefix)
    //   x.a does NOT overlap with x.b (different fields)
    return is_prefix_of(other) || other.is_prefix_of(*this);
}

/// Converts a place to a human-readable string for error messages.
///
/// ## Examples
///
/// - `x` → "x"
/// - `x.field` → "x.field"
/// - `x[...]` → "x[...]" (index shown as [...])
/// - `*x` → "*x"
/// - `*x.field` → "*x.field"
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

/// Updates `last_use` for borrows associated with a reference variable.
///
/// When a reference variable is used, we need to update the `last_use` of
/// the underlying borrow. This enables NLL to end the borrow at the right time.
///
/// ## Example
///
/// ```tml
/// let r = ref x       // Creates borrow, ref_place = r's PlaceId
/// println(r)          // mark_ref_used updates borrow's last_use
/// // ... later, NLL can end the borrow at this point
/// ```
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

/// Releases borrows that are no longer needed (NLL core algorithm).
///
/// For each active borrow, if we have recorded a `last_use` and the current
/// location is past that use, the borrow is ended.
///
/// ## Algorithm
///
/// ```text
/// for each place:
///     for each active borrow:
///         if borrow.last_use < current_location:
///             borrow.end = borrow.last_use
///     recompute ownership state based on remaining borrows
/// ```
///
/// ## State Recomputation
///
/// After releasing borrows, ownership state is updated:
/// - Has active mutable borrow → `MutBorrowed`
/// - Has active shared borrows → `Borrowed`
/// - No active borrows → `Owned` (if was previously borrowed)
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
                if (borrow.kind == BorrowKind::Mutable)
                    has_active_mut = true;
                else
                    has_active_shared = true;
            }
        }

        if (state.state == OwnershipState::Borrowed || state.state == OwnershipState::MutBorrowed) {
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

/// Checks if a borrow is still live at a given location.
///
/// A borrow is live if:
/// 1. It hasn't been explicitly ended (borrow.end is nullopt)
/// 2. AND (no last_use recorded OR current_location <= last_use)
///
/// If no last_use is recorded, we conservatively assume the borrow is live.
auto BorrowEnv::is_borrow_live(const Borrow& borrow, Location loc) const -> bool {
    // A borrow is live if:
    // 1. It hasn't ended (end is nullopt)
    // 2. AND either has no last_use OR current loc <= last_use
    if (borrow.end)
        return false;

    if (borrow.last_use) {
        return loc <= *borrow.last_use;
    }

    // No last_use recorded yet - conservatively assume live
    return true;
}

// ============================================================================
// Partial Move Tracking
// ============================================================================

/// Marks a field as moved out of a struct.
///
/// After moving a field, the struct is in a "partially moved" state:
/// - The moved field cannot be used
/// - Other fields can still be used
/// - The whole struct cannot be moved or borrowed
///
/// ## Example
///
/// ```tml
/// let p = Pair { a: String::from("x"), b: String::from("y") }
/// let s = p.a         // mark_field_moved(p, "a")
/// println(p.b)        // OK
/// println(p.a)        // ERROR
/// drop(p)             // ERROR: partially moved
/// ```
void BorrowEnv::mark_field_moved(PlaceId id, const std::string& field) {
    auto& state = get_state_mut(id);
    state.moved_fields.insert(field);

    // If any field is moved, mark as partially moved
    // We don't change to FullyMoved because other fields are still valid
}

/// Returns the move state of a place.
///
/// | State           | Description                              |
/// |-----------------|------------------------------------------|
/// | `FullyOwned`    | All parts owned, can use/move/borrow     |
/// | `PartiallyMoved`| Some fields moved, limited usage         |
/// | `FullyMoved`    | Entire value moved, cannot use           |
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

/// Checks if a specific field has been moved out.
auto BorrowEnv::is_field_moved(PlaceId id, const std::string& field) const -> bool {
    const auto& state = get_state(id);
    return state.moved_fields.count(field) > 0;
}

// ============================================================================
// BorrowChecker NLL Methods
// ============================================================================

/// Applies NLL by releasing dead borrows at the current location.
///
/// This should be called before checking operations that might conflict
/// with borrows, giving NLL a chance to release borrows that are no longer
/// needed.
void BorrowChecker::apply_nll(Location loc) {
    // Release borrows that are no longer needed at this location
    env_.release_dead_borrows(loc);
}

/// Creates a borrow with full projection information.
///
/// This method handles sophisticated borrowing scenarios including:
/// - Field-level borrows (`ref x.field`)
/// - Index borrows (`ref x[i]`)
/// - Nested borrows (`ref x.field.subfield`)
///
/// ## Parameters
///
/// - `place`: The base PlaceId being borrowed
/// - `full_place`: The complete place with all projections
/// - `kind`: Shared or Mutable borrow
/// - `loc`: Location for lifetime tracking
/// - `ref_place`: The PlaceId of the reference variable that will hold this borrow
///
/// ## State Updates
///
/// - Creates a new `Borrow` record with projection info
/// - Updates ownership state to `Borrowed` or `MutBorrowed`
/// - Records the `ref_place → borrowed_place` mapping for NLL
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
        .last_use = std::nullopt, // Will be updated by NLL tracking
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

/// Checks if a place with projections can be borrowed.
///
/// This performs sophisticated conflict checking:
/// 1. Check if the value has been moved (fully or partially)
/// 2. Check for conflicts with existing borrows (projection-aware)
/// 3. Check mutability requirements
///
/// ## Projection-Aware Conflict Detection
///
/// Two borrows conflict only if their places overlap. Non-overlapping
/// fields can be borrowed independently:
///
/// ```tml
/// let mut s = S { a: 1, b: 2 }
/// let ra = mut ref s.a   // OK
/// let rb = mut ref s.b   // OK: s.a and s.b don't overlap
/// let rs = ref s         // ERROR: s overlaps with s.a and s.b
/// ```
///
/// ## Two-Phase Borrow Handling
///
/// During two-phase borrow (method call evaluation), some conflicts are
/// temporarily allowed to support patterns like `v.push(v.len())`.
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
                error("cannot borrow `" + state.name + "." + field + "` because it has been moved",
                      loc.span);
                return;
            }
        }
    } else {
        // Borrowing the whole value - check if any part is moved
        if (env_.get_move_state(place) == MoveState::PartiallyMoved) {
            error("cannot borrow `" + state.name + "` because part of it has been moved", loc.span);
            return;
        }
    }

    // Check for conflicting borrows with projection awareness
    bool is_reborrow = state.borrowed_from.has_value();

    for (const auto& existing_borrow : state.active_borrows) {
        if (existing_borrow.end)
            continue; // Borrow has ended

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
                              "` as mutable more than once at a time",
                          loc.span);
                    return;
                } else {
                    error("cannot borrow `" + get_place_name(full_place) +
                              "` as mutable because it is also borrowed as immutable",
                          loc.span);
                    return;
                }
            }
        } else {
            // Shared borrow conflicts with mutable borrow
            if (existing_borrow.kind == BorrowKind::Mutable && !is_two_phase_borrow_active_) {
                error("cannot borrow `" + get_place_name(full_place) +
                          "` as immutable because it is also borrowed as mutable",
                      loc.span);
                return;
            }
        }
    }

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
    }
}

/// Moves a single field out of a struct (partial move).
///
/// This checks all the conditions that must hold for a partial move:
/// 1. The struct itself hasn't been fully moved
/// 2. This specific field hasn't already been moved
/// 3. No active borrows prevent the move
///
/// ## Borrow Interaction
///
/// - Borrow of whole struct → cannot move any field
/// - Borrow of this field → cannot move this field
/// - Borrow of other field → CAN move this field
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
    if (state.state == OwnershipState::Borrowed || state.state == OwnershipState::MutBorrowed) {
        // Check if the borrow is on this specific field or the whole thing
        for (const auto& borrow : state.active_borrows) {
            if (borrow.end)
                continue;

            // If borrowing the whole value, can't move any field
            if (borrow.full_place.projections.empty()) {
                error("cannot move out of `" + state.name + "." + field + "` because `" +
                          state.name + "` is borrowed",
                      loc.span);
                return;
            }

            // If borrowing this specific field, can't move it
            if (!borrow.full_place.projections.empty() &&
                borrow.full_place.projections[0].kind == ProjectionKind::Field &&
                borrow.full_place.projections[0].field_name == field) {
                error("cannot move out of `" + state.name + "." + field +
                          "` because it is borrowed",
                      loc.span);
                return;
            }
        }
    }

    // Mark field as moved
    env_.mark_field_moved(place, field);
}

/// Checks if a specific field can be used (not moved or dropped).
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

/// Checks for dangling references in return expressions.
///
/// A function cannot return a reference to a local variable because the
/// local will be dropped when the function returns, leaving a dangling
/// reference.
///
/// ## Cases Detected
///
/// 1. **Direct reference to local**: `return ref x`
/// 2. **Reference variable borrowing local**: `let r = ref x; return r`
///
/// ## Safe Returns
///
/// - Returning a reference from a parameter is allowed (caller owns the value)
/// - Returning owned values is always allowed
/// - Returning 'static references is allowed
void BorrowChecker::check_return_borrows(const parser::ReturnExpr& ret) {
    if (!ret.value)
        return;

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
                              "` as it will be dropped when the function returns",
                          loc.span);
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
                bool is_local = true; // All variables in current scope are locals

                // Check scope depth - if borrowed value is in current function scope
                // and not a parameter, it will be dropped
                // (Parameters have scope_depth 1 in a function)
                if (is_local && borrowed_state.definition.statement_index > 0) {
                    // Local variable defined after function start - will be dropped
                    error("cannot return reference that borrows from local variable `" +
                              borrowed_state.name + "`",
                          loc.span);
                }
            }
        }
    }
}

/// Extracts a Place from an expression, if the expression represents a place.
///
/// A "place" is a memory location that can be borrowed or moved. Not all
/// expressions are places - for example, `1 + 2` is not a place.
///
/// ## Supported Expressions
///
/// | Expression Type | Place                          |
/// |-----------------|--------------------------------|
/// | `x`             | `Place { base: x_id, [] }`     |
/// | `x.field`       | `Place { base: x_id, [Field("field")] }` |
/// | `x[i]`          | `Place { base: x_id, [Index] }` |
/// | `*x`            | `Place { base: x_id, [Deref] }` |
/// | `1 + 2`         | None (not a place)             |
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
            base_place->projections.push_back(Projection{ProjectionKind::Field, field_expr.field});
            return base_place;
        }
        return std::nullopt;
    }

    if (expr.is<parser::IndexExpr>()) {
        const auto& index = expr.as<parser::IndexExpr>();
        auto base_place = extract_place(*index.object);
        if (base_place) {
            base_place->projections.push_back(Projection{ProjectionKind::Index, ""});
            return base_place;
        }
        return std::nullopt;
    }

    if (expr.is<parser::UnaryExpr>()) {
        const auto& unary = expr.as<parser::UnaryExpr>();
        if (unary.op == parser::UnaryOp::Deref) {
            auto base_place = extract_place(*unary.operand);
            if (base_place) {
                base_place->projections.push_back(Projection{ProjectionKind::Deref, ""});
                return base_place;
            }
        }
        return std::nullopt;
    }

    return std::nullopt;
}

/// Gets a human-readable name for a place (used in error messages).
auto BorrowChecker::get_place_name(const Place& place) const -> std::string {
    auto it = env_.all_places().find(place.base);
    if (it == env_.all_places().end()) {
        return "<unknown>";
    }
    return place.to_string(it->second.name);
}

} // namespace tml::borrow
