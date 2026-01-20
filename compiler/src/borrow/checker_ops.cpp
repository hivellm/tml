//! # Borrow Checker Operations
//!
//! This file implements core borrow checking operations: creating/releasing borrows,
//! moving values, checking usage permissions, and error reporting.
//!
//! ## Error Reporting Architecture
//!
//! The borrow checker produces rich diagnostic errors with:
//! - Primary error message and span
//! - Related location (e.g., "value moved here")
//! - Notes explaining why the error occurred
//! - Suggestions for fixing the error
//!
//! Example error output:
//! ```text
//! error[E0382]: use of moved value: `x`
//!   --> src/main.tml:10:5
//!    |
//! 8  |     let y = x;
//!    |             - value moved here
//! 9  |
//! 10 |     println(x);
//!    |             ^ value used here after move
//!    |
//!    = note: move occurs because `x` has type that does not implement `Duplicate`
//!    = help: consider cloning the value before the move
//! ```
//!
//! ## Borrow Operations
//!
//! | Operation       | Effect                                    |
//! |-----------------|-------------------------------------------|
//! | `create_borrow` | Records a new borrow, updates state       |
//! | `release_borrow`| Marks borrow as ended, recomputes state   |
//! | `move_value`    | Transfers ownership, marks source invalid |
//! | `create_reborrow` | Creates borrow from existing reference |
//!
//! ## Two-Phase Borrows
//!
//! During method calls, we need special handling:
//! 1. `begin_two_phase_borrow()` - Enter reservation phase
//! 2. Receiver is "reserved" (can still be borrowed immutably)
//! 3. Arguments are evaluated (may borrow receiver)
//! 4. `end_two_phase_borrow()` - Mutable borrow activates

#include "borrow/checker.hpp"

namespace tml::borrow {

// ============================================================================
// BorrowError Static Helpers - Create rich diagnostics for common error patterns
// ============================================================================

/// Creates a "use after move" error with full context.
///
/// This is one of the most common borrow checker errors. The diagnostic includes:
/// - The location where the moved value is used
/// - The location where the move occurred
/// - Explanation that the type doesn't implement `Duplicate`
/// - Suggestion to use `.duplicate()` if appropriate
///
/// ## Example TML Code
///
/// ```tml
/// let x = String::from("hello")
/// let y = x           // move happens here
/// println(x)          // ERROR: use after move
/// ```
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

/// Creates a "double mutable borrow" error.
///
/// Rust/TML's core invariant: only one mutable reference at a time.
/// This error fires when attempting to create a second mutable borrow
/// while the first is still active.
///
/// ## Example
///
/// ```tml
/// let mut x = 5
/// let r1 = mut ref x   // first mutable borrow
/// let r2 = mut ref x   // ERROR: second mutable borrow
/// ```
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

/// Creates a "mutable borrow while immutably borrowed" error.
///
/// Cannot create a mutable borrow while immutable borrows exist because
/// the mutable borrow could invalidate what the immutable borrows see.
///
/// ## Example
///
/// ```tml
/// let mut x = vec![1, 2, 3]
/// let r = ref x        // immutable borrow
/// let m = mut ref x    // ERROR: cannot borrow mutably
/// println(r)           // r is still in use
/// ```
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
    err.suggestions.push_back(BorrowSuggestion{
        .message = "ensure the immutable borrow is no longer used before creating a mutable borrow",
        .fix = std::nullopt,
    });
    err.suggestions.push_back(BorrowSuggestion{
        .message = "consider using interior mutability types like `Cell[T]` or `Mutex[T]`",
        .fix = std::nullopt,
    });
    return err;
}

/// Creates an "immutable borrow while mutably borrowed" error.
///
/// Cannot create an immutable borrow while a mutable borrow exists because
/// the mutable borrow has exclusive access to the value.
///
/// ## Example
///
/// ```tml
/// let mut x = 5
/// let m = mut ref x    // mutable borrow
/// let r = ref x        // ERROR: cannot borrow immutably
/// *m = 10              // m is still in use
/// ```
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
    err.suggestions.push_back(BorrowSuggestion{
        .message = "ensure the mutable borrow is no longer used before creating an immutable borrow",
        .fix = std::nullopt,
    });
    err.suggestions.push_back(BorrowSuggestion{
        .message = "consider restructuring your code to separate mutable and immutable access",
        .fix = std::nullopt,
    });
    return err;
}

/// Creates a "return reference to local" error.
///
/// This is a critical memory safety error. Returning a reference to a local
/// would create a dangling pointer when the function returns and the local
/// is deallocated.
///
/// ## Example
///
/// ```tml
/// func bad() -> ref I32 {
///     let x = 42
///     return ref x    // ERROR: returns reference to local
/// }                   // x is dropped here
/// ```
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

auto BorrowError::closure_captures_moved(const std::string& name, SourceSpan capture_span,
                                         SourceSpan move_span) -> BorrowError {
    BorrowError err;
    err.code = BorrowErrorCode::ClosureCapturesMoved;
    err.message = "closure captures moved value `" + name + "`";
    err.span = capture_span;
    err.related_span = move_span;
    err.related_message = "`" + name + "` was moved here";
    err.notes.push_back("value moved before closure is defined");
    err.suggestions.push_back(BorrowSuggestion{
        .message = "consider using `.duplicate()` before the move to keep a copy",
        .fix = std::nullopt,
    });
    return err;
}

auto BorrowError::closure_capture_conflict(const std::string& name, CaptureKind capture_kind,
                                           SourceSpan capture_span, SourceSpan borrow_span)
    -> BorrowError {
    BorrowError err;
    err.code = BorrowErrorCode::ClosureCaptureConflict;

    std::string capture_desc;
    switch (capture_kind) {
    case CaptureKind::ByMutRef:
        capture_desc = "mutably";
        break;
    case CaptureKind::ByMove:
        capture_desc = "by move";
        break;
    default:
        capture_desc = "by reference";
        break;
    }

    err.message = "closure captures `" + name + "` " + capture_desc +
                  " while it is already borrowed";
    err.span = capture_span;
    err.related_span = borrow_span;
    err.related_message = "`" + name + "` is borrowed here";
    err.notes.push_back("closure would invalidate the existing borrow");
    err.suggestions.push_back(BorrowSuggestion{
        .message = "consider restructuring code to avoid overlapping borrows",
        .fix = std::nullopt,
    });
    return err;
}

auto BorrowError::partially_moved_value(const std::string& name, const std::string& moved_field,
                                        SourceSpan use_span, SourceSpan move_span) -> BorrowError {
    BorrowError err;
    err.code = BorrowErrorCode::PartiallyMovedValue;
    err.message = "use of partially moved value `" + name + "`";
    err.span = use_span;
    err.related_span = move_span;
    err.related_message = "field `" + moved_field + "` was moved here";
    err.notes.push_back("partial move occurs because `" + name + "." + moved_field +
                        "` has type that does not implement `Duplicate`");
    err.suggestions.push_back(BorrowSuggestion{
        .message = "consider using a reference instead: `ref " + name + "." + moved_field + "`",
        .fix = std::nullopt,
    });
    return err;
}

/// Creates a "reborrow outlives original borrow" error (B017).
///
/// This error fires when a reborrowed reference has a longer lifetime than
/// the original reference it was derived from.
///
/// ## Example
///
/// ```tml
/// func bad() -> ref I32 {
///     let x = 42
///     let r1 = ref x
///     let r2 = ref *r1   // r2 is a reborrow from r1
///     return r2          // ERROR: r2 outlives r1
/// }
/// ```
auto BorrowError::reborrow_outlives_origin(const std::string& reborrow_name,
                                           const std::string& origin_name,
                                           SourceSpan reborrow_span,
                                           SourceSpan origin_span) -> BorrowError {
    BorrowError err;
    err.code = BorrowErrorCode::ReborrowOutlivesOrigin;
    err.message = "reborrow `" + reborrow_name + "` outlives the original borrow `" + origin_name + "`";
    err.span = reborrow_span;
    err.related_span = origin_span;
    err.related_message = "original borrow created here";
    err.notes.push_back("the reborrowed reference cannot outlive the reference it derives from");
    err.suggestions.push_back(BorrowSuggestion{
        .message = "consider borrowing directly from the owned value instead of reborrowing",
        .fix = std::nullopt,
    });
    return err;
}

/// Creates an "ambiguous return lifetime" error (E031).
///
/// This error fires when a function returns a reference but has multiple input
/// reference parameters and no `this` parameter, so the compiler cannot
/// determine which input lifetime the return should use.
///
/// ## TML Lifetime Elision Rules
///
/// 1. Each `ref T` parameter gets a separate lifetime
/// 2. If there's a `this`/`mut this`, return has same lifetime as `this`
/// 3. If there's exactly one ref parameter, return uses that lifetime
///
/// ## Example
///
/// ```tml
/// // ERROR: ambiguous lifetime - which input does output reference?
/// func longest(a: ref String, b: ref String) -> ref String {
///     if a.len() > b.len() then a else b
/// }
///
/// // FIX: return owned value instead
/// func longest(a: ref String, b: ref String) -> String {
///     if a.len() > b.len() then a.duplicate() else b.duplicate()
/// }
/// ```
auto BorrowError::ambiguous_return_lifetime(const std::string& func_name,
                                            const std::vector<std::string>& ref_params,
                                            SourceSpan func_span) -> BorrowError {
    BorrowError err;
    err.code = BorrowErrorCode::AmbiguousReturnLifetime;

    // Build parameter list for the message
    std::string params_list;
    for (size_t i = 0; i < ref_params.size(); ++i) {
        if (i > 0) params_list += ", ";
        params_list += "`" + ref_params[i] + "`";
    }

    err.message = "cannot determine lifetime of return reference in function `" + func_name + "`";
    err.span = func_span;
    err.notes.push_back("function has multiple reference parameters: " + params_list);
    err.notes.push_back("without a `this` parameter, the compiler cannot infer which parameter's "
                        "lifetime the return should use");
    err.suggestions.push_back(BorrowSuggestion{
        .message = "consider returning an owned value instead of a reference",
        .fix = std::nullopt,
    });
    err.suggestions.push_back(BorrowSuggestion{
        .message = "if this is a method, add a `this` parameter to disambiguate",
        .fix = std::nullopt,
    });
    return err;
}

/// Creates a new borrow on a place.
///
/// This is the core operation for `ref x` and `mut ref x` expressions.
/// Updates the place's state to reflect the new borrow.
///
/// ## Parameters
///
/// - `place`: The PlaceId being borrowed
/// - `kind`: `Shared` for `ref`, `Mutable` for `mut ref`
/// - `loc`: Location for error reporting and lifetime tracking
///
/// ## State Transitions
///
/// | Initial State | Borrow Kind | New State     |
/// |---------------|-------------|---------------|
/// | `Owned`       | `Shared`    | `Borrowed`    |
/// | `Owned`       | `Mutable`   | `MutBorrowed` |
/// | `Borrowed`    | `Shared`    | `Borrowed`    |
/// | `Borrowed`    | `Mutable`   | (error)       |
/// | `MutBorrowed` | Any         | (error/2phase)|
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
        .reborrow_origin = std::nullopt,
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

/// Releases a borrow on a place.
///
/// Called when a reference goes out of scope or is explicitly dropped.
/// After release, recomputes the ownership state based on remaining borrows.
///
/// ## State Recomputation
///
/// After releasing, the state is determined by remaining borrows:
/// - Any active mutable borrow → `MutBorrowed`
/// - Only active shared borrows → `Borrowed`
/// - No active borrows → `Owned`
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

/// Moves a value out of a place, transferring ownership.
///
/// After a move, the source place is invalid and cannot be used. This
/// method checks various error conditions before allowing the move.
///
/// ## Error Conditions
///
/// | Condition           | Error                              |
/// |---------------------|------------------------------------|
/// | Already moved       | "use of moved value"               |
/// | Currently borrowed  | "cannot move while borrowed"       |
///
/// ## State After Move
///
/// The place transitions to `OwnershipState::Moved` and records the
/// move location for future error messages.
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
                err.suggestions.push_back(BorrowSuggestion{
                    .message = "consider cloning the value instead of moving it",
                    .fix = state.name + ".duplicate()",
                });
                err.suggestions.push_back(BorrowSuggestion{
                    .message = "ensure the borrow ends before moving the value",
                    .fix = std::nullopt,
                });
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

/// Checks if a place can be used (read from).
///
/// A place can be used if it hasn't been moved or dropped. This check
/// is performed before any read of a variable.
///
/// Note: Being borrowed does NOT prevent use! You can read a borrowed
/// value; you just can't move or mutably borrow it.
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

/// Checks if a place can be mutated (assigned to).
///
/// Mutation requires:
/// 1. The variable must be declared `mut`
/// 2. The variable must not have been moved
/// 3. The variable must not be currently borrowed
///
/// Exception: Interior mutable types (Cell, Mutex, Shared, Sync) can be
/// mutated through shared references, with a W001 warning.
///
/// ## Example
///
/// ```tml
/// let x = 5
/// x = 10       // ERROR: x is not mutable
///
/// let mut y = 5
/// y = 10       // OK
///
/// let mut z = 5
/// let r = ref z
/// z = 10       // ERROR: z is borrowed
///
/// let c: Cell[I32] = Cell::new(5)
/// let r: ref Cell[I32] = ref c
/// r.set(10)    // OK (interior mutability) - W001 warning
/// ```
void BorrowChecker::check_can_mutate(PlaceId place, Location loc) {
    const auto& state = env_.get_state(place);

    // Check for interior mutability - allows mutation through shared references
    // This is used by types like Cell, Mutex, Shared, Sync
    bool is_interior_mut = is_interior_mutable(state.type);

    if (!state.is_mutable) {
        // Allow assignment through mutable references (mut ref T)
        // Even if the variable itself isn't mutable, we can assign through it
        if (!state.is_mut_ref) {
            // Also allow if the type has interior mutability
            if (is_interior_mut) {
                // Emit W001 warning: interior mutability bypasses borrow checking
                BorrowError warning;
                warning.code = BorrowErrorCode::InteriorMutWarning;
                warning.message = "mutation through shared reference to interior mutable type `" +
                                  state.name + "`";
                warning.span = loc.span;
                warning.related_span = state.definition.span;
                warning.related_message = "interior mutable type declared here";
                warning.suggestions.push_back(BorrowSuggestion{
                    .message = "interior mutability bypasses normal borrow checking rules",
                    .fix = "",
                });
                warnings_.push_back(warning);
                return; // Allow the mutation
            }

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
                err.suggestions.push_back(BorrowSuggestion{
                    .message = "ensure the borrow is no longer used before assigning",
                    .fix = std::nullopt,
                });
                err.suggestions.push_back(BorrowSuggestion{
                    .message = "consider using `Cell[T]` or `Mutex[T]` for interior mutability",
                    .fix = std::nullopt,
                });
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
                err.suggestions.push_back(BorrowSuggestion{
                    .message = "ensure the mutable borrow is no longer used before assigning",
                    .fix = std::nullopt,
                });
                err.suggestions.push_back(BorrowSuggestion{
                    .message = "consider performing the assignment through the mutable reference",
                    .fix = std::nullopt,
                });
                errors_.push_back(err);
                return;
            }
        }
        error("cannot assign to `" + state.name + "` because it is mutably borrowed", loc.span);
        return;
    }
}

/// Checks if a place can be borrowed with the given kind.
///
/// This implements the core borrowing rules:
///
/// ## Shared Borrow (`ref x`)
///
/// - Cannot borrow moved value
/// - Cannot borrow while mutably borrowed (unless two-phase borrow active)
/// - Can borrow while already borrowed immutably
///
/// ## Mutable Borrow (`mut ref x`)
///
/// - Value must be declared `mut`
/// - Cannot borrow moved value
/// - Cannot borrow while borrowed (mutable or immutable)
/// - Exception: reborrows from mutable references are allowed
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
        err.suggestions.push_back(BorrowSuggestion{
            .message = "consider borrowing before the move instead",
            .fix = std::nullopt,
        });
        err.suggestions.push_back(BorrowSuggestion{
            .message = "or clone the value if you need both ownership and a borrow",
            .fix = state.name + ".duplicate()",
        });
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
        // Only allow if we're in Reserved state (borrow not yet active)
        bool in_reservation = two_phase_info_.state == TwoPhaseState::Reserved;
        if (state.state == OwnershipState::MutBorrowed && !in_reservation) {
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
        // Also allow during two-phase borrow reservation
        bool in_reservation = two_phase_info_.state == TwoPhaseState::Reserved;
        if (state.state == OwnershipState::MutBorrowed && !is_reborrow &&
            !in_reservation) {
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

/// Creates a reborrow from an existing reference.
///
/// Reborrows allow:
/// - Creating `ref T` from `mut ref T` (downgrade)
/// - Creating `mut ref T` from `mut ref T` (if inner supports it)
/// - NOT creating `mut ref T` from `ref T` (upgrade)
///
/// ## Example
///
/// ```tml
/// func use_ref(r: mut ref I32) {
///     let shared: ref I32 = r      // reborrow: mut ref -> ref
///     println(*shared)
/// }
/// ```
void BorrowChecker::create_reborrow(PlaceId source, PlaceId target, BorrowKind kind, Location loc) {
    auto& target_state = env_.get_state_mut(target);
    target_state.borrowed_from = std::make_pair(source, kind);

    // Create a borrow on the source
    create_borrow(source, kind, loc);

    // Track the reborrow chain for lifetime validation
    const auto& source_state = env_.get_state(source);

    // Find the index of the origin borrow (the most recent active borrow on source)
    size_t origin_borrow_index = 0;
    if (!source_state.active_borrows.empty()) {
        // Use the most recent borrow as the origin
        origin_borrow_index = source_state.active_borrows.size() - 1;
    }

    // Register this reborrow for lifetime tracking
    env_.create_reborrow(target, origin_borrow_index, kind, loc);
}

/// Begins a two-phase borrow context.
///
/// Two-phase borrowing is needed for method calls where the receiver
/// is mutably borrowed but arguments might need to read from it.
///
/// This enters the Reserved state where shared borrows are temporarily allowed.
void BorrowChecker::begin_two_phase_borrow() {
    two_phase_info_.state = TwoPhaseState::Reserved;
}

/// Ends a two-phase borrow context.
///
/// After this, normal borrow checking rules apply again.
void BorrowChecker::end_two_phase_borrow() {
    two_phase_info_.state = TwoPhaseState::None;
    two_phase_info_.place = 0;
    two_phase_info_.borrow_index = 0;
}

/// Reserves a two-phase borrow on a place.
///
/// Creates a mutable borrow in Reserved state. While reserved, shared
/// borrows of the same place are allowed.
void BorrowChecker::reserve_two_phase_borrow(PlaceId place, BorrowKind kind, Location loc) {
    two_phase_info_.place = place;
    two_phase_info_.state = TwoPhaseState::Reserved;
    two_phase_info_.kind = kind;
    two_phase_info_.start = loc;

    // Create the actual borrow but mark it as reserved
    // The borrow will be in the active_borrows but conflicts are suppressed
    create_borrow(place, kind, loc);

    // Store the borrow index (last added borrow)
    const auto& state = env_.get_state(place);
    if (!state.active_borrows.empty()) {
        two_phase_info_.borrow_index = state.active_borrows.size() - 1;
    }
}

/// Activates a reserved two-phase borrow.
///
/// Transitions from Reserved to Active state. After activation, normal
/// borrow rules apply (no other borrows allowed).
void BorrowChecker::activate_two_phase_borrow() {
    if (two_phase_info_.state == TwoPhaseState::Reserved) {
        two_phase_info_.state = TwoPhaseState::Active;
    }
}

/// Checks if a place has a reserved (not active) two-phase borrow.
auto BorrowChecker::is_reserved_borrow(PlaceId place) const -> bool {
    return two_phase_info_.state == TwoPhaseState::Reserved &&
           two_phase_info_.place == place;
}

/// Gets the current two-phase state.
auto BorrowChecker::get_two_phase_state() const -> TwoPhaseState {
    return two_phase_info_.state;
}

/// Drops all places in the current scope.
///
/// Called when a scope ends (block, function, etc.). This method:
/// 1. Releases all borrows created at the current scope depth
/// 2. Marks all places defined in the scope as `Dropped`
///
/// ## Drop Order
///
/// Places are dropped in reverse declaration order (LIFO), matching
/// Rust/TML's drop semantics.
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

/// Reports a simple error without related locations.
void BorrowChecker::error(const std::string& message, SourceSpan span) {
    errors_.push_back(BorrowError{
        .message = message,
        .span = span,
        .notes = {},
        .related_span = std::nullopt,
        .related_message = std::nullopt,
        .suggestions = {},
    });
}

/// Reports an error with a note at a related location.
void BorrowChecker::error_with_note(const std::string& message, SourceSpan span,
                                    const std::string& note, SourceSpan note_span) {
    errors_.push_back(BorrowError{
        .message = message,
        .span = span,
        .notes = {note},
        .related_span = note_span,
        .related_message = std::nullopt,
        .suggestions = {},
    });
}

/// Returns a Location struct for the current statement and span.
auto BorrowChecker::current_location(SourceSpan span) const -> Location {
    return Location{current_stmt_, span};
}

} // namespace tml::borrow
