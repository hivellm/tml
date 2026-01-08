//! # Borrow Checker
//!
//! This module implements the TML borrow checker, which enforces memory safety
//! through ownership and borrowing rules at compile time.
//!
//! ## Overview
//!
//! The borrow checker ensures that:
//! - Each value has exactly one owner at any time
//! - References cannot outlive the values they point to
//! - Mutable references are exclusive (no aliasing)
//! - Immutable references can coexist but not with mutable ones
//!
//! ## Non-Lexical Lifetimes (NLL)
//!
//! This implementation uses Non-Lexical Lifetimes (NLL), which means borrows
//! end at their last use rather than at the end of their lexical scope. This
//! allows for more flexible and ergonomic code while maintaining safety.
//!
//! ## Key Components
//!
//! - [`BorrowChecker`]: The main checker that validates an entire module
//! - [`BorrowEnv`]: Tracks the state of all places (variables) during checking
//! - [`Place`]: Represents a memory location with optional projections (fields, indices)
//! - [`Borrow`]: Represents an active borrow with its lifetime information
//! - [`BorrowError`]: Rich error diagnostics with suggestions for fixes
//!
//! ## Example
//!
//! ```tml
//! func example() {
//!     let mut x = 42
//!     let r = ref x      // Immutable borrow starts
//!     println(r)         // Last use of r - borrow ends here (NLL)
//!     x = 100            // OK! Borrow already ended
//! }
//! ```
//!
//! ## Error Categories
//!
//! | Code | Error |
//! |------|-------|
//! | B001 | Use after move |
//! | B002 | Move while borrowed |
//! | B003 | Assignment to immutable |
//! | B004 | Assignment while borrowed |
//! | B005 | Borrow of moved value |
//! | B006 | Mutable borrow of non-mutable |
//! | B007 | Mutable borrow while immutably borrowed |
//! | B008 | Double mutable borrow |
//! | B009 | Immutable borrow while mutably borrowed |
//! | B010 | Return reference to local |

#ifndef TML_BORROW_CHECKER_HPP
#define TML_BORROW_CHECKER_HPP

#include "common.hpp"
#include "parser/ast.hpp"
#include "types/type.hpp"

#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tml::borrow {

/// Unique identifier for a place (variable, field access, etc.).
///
/// Each variable or memory location in the program is assigned a unique
/// `PlaceId` that is used to track its ownership and borrowing state.
using PlaceId = uint64_t;

/// Unique identifier for a lifetime.
///
/// Lifetimes track how long a borrow is valid. With NLL, lifetimes are
/// computed based on actual usage rather than lexical scope.
using LifetimeId = uint64_t;

/// Represents a location in the program for tracking statement ordering.
///
/// Locations are used to determine the relative ordering of operations,
/// which is essential for NLL analysis. A borrow at location A is only
/// valid for uses at locations >= A and < end location.
struct Location {
    /// The index of the statement in the control flow.
    size_t statement_index;

    /// The source code span for error reporting.
    SourceSpan span;

    auto operator<(const Location& other) const -> bool {
        return statement_index < other.statement_index;
    }
    auto operator<=(const Location& other) const -> bool {
        return statement_index <= other.statement_index;
    }
    auto operator==(const Location& other) const -> bool {
        return statement_index == other.statement_index;
    }
};

/// The kind of projection used to access a sub-part of a place.
///
/// Projections allow tracking access to fields, array elements, and
/// dereferenced pointers as distinct memory locations.
enum class ProjectionKind {
    Field, ///< Field access: `.field`
    Index, ///< Array/slice index: `[i]`
    Deref, ///< Pointer dereference: `*ptr`
};

/// A single projection step in a place path.
///
/// For example, in `x.field[0]`, there are two projections:
/// 1. `Field("field")`
/// 2. `Index`
struct Projection {
    /// The kind of projection.
    ProjectionKind kind;

    /// The field name (only valid when `kind == Field`).
    std::string field_name;
};

/// A place represents a memory location that can be borrowed or moved.
///
/// A place consists of a base variable (identified by `PlaceId`) and zero
/// or more projections. This allows the borrow checker to track borrows
/// of individual fields or array elements separately.
///
/// # Examples
///
/// - `x` - base place with no projections
/// - `x.field` - base `x` with Field projection
/// - `x.field[0]` - base `x` with Field then Index projections
/// - `*ptr` - base `ptr` with Deref projection
struct Place {
    /// The base variable's identifier.
    PlaceId base;

    /// The chain of projections from the base.
    std::vector<Projection> projections;

    /// Checks if this place is a prefix of another.
    ///
    /// A place is a prefix if it represents a "parent" location.
    /// For example, `x` is a prefix of `x.field`.
    auto is_prefix_of(const Place& other) const -> bool;

    /// Checks if two places overlap and could conflict.
    ///
    /// Two places overlap if one is a prefix of the other, or if they
    /// refer to the same location. Overlapping places cannot both have
    /// mutable borrows active simultaneously.
    auto overlaps_with(const Place& other) const -> bool;

    /// Returns a string representation for error messages.
    auto to_string(const std::string& base_name) const -> std::string;
};

/// Lifetime representation for Non-Lexical Lifetimes (NLL).
///
/// A lifetime represents the span during which a borrow is valid. With NLL,
/// lifetimes are computed based on actual data flow rather than lexical scope,
/// allowing more programs to pass borrow checking.
///
/// # NLL Algorithm
///
/// 1. A borrow creates a lifetime starting at the borrow location
/// 2. The lifetime extends to cover all uses of the borrowed reference
/// 3. The lifetime ends at the last use (not at scope end)
struct Lifetime {
    /// Unique identifier for this lifetime.
    LifetimeId id;

    /// Location where the borrow was created.
    Location start;

    /// Location where the lifetime ends, or `nullopt` if still live.
    std::optional<Location> end;

    /// The place that is borrowed by this lifetime.
    PlaceId borrowed_place;

    /// Checks if this lifetime is live at the given location.
    auto is_live_at(const Location& loc) const -> bool {
        if (loc < start)
            return false;
        if (!end)
            return true;
        return loc <= *end;
    }
};

/// The kind of borrow: shared (immutable) or mutable.
///
/// TML's borrowing rules are:
/// - Multiple shared borrows (`ref T`) can coexist
/// - Only one mutable borrow (`mut ref T`) can exist at a time
/// - Shared and mutable borrows cannot coexist
enum class BorrowKind {
    Shared,  ///< Immutable reference: `ref T`
    Mutable, ///< Mutable reference: `mut ref T`
};

/// Represents an active borrow with full NLL tracking information.
///
/// A `Borrow` is created when a reference is taken and tracks:
/// - What place is borrowed
/// - Whether it's a shared or mutable borrow
/// - When the borrow starts and ends (for NLL)
/// - Which scope created the borrow
struct Borrow {
    /// The base place being borrowed.
    PlaceId place;

    /// The full place with projections (e.g., `x.field`).
    Place full_place;

    /// Whether this is a shared or mutable borrow.
    BorrowKind kind;

    /// Location where the borrow was created.
    Location start;

    /// Location where the borrow ends, or `nullopt` if still active.
    std::optional<Location> end;

    /// Last use of this borrow for NLL computation.
    ///
    /// With NLL, the borrow ends at `last_use` rather than at scope end.
    std::optional<Location> last_use;

    /// The scope depth where this borrow was created.
    size_t scope_depth;

    /// The associated lifetime identifier.
    LifetimeId lifetime;

    /// The place that holds this reference (for tracking reference chains).
    PlaceId ref_place;
};

/// The ownership state of a place (variable or memory location).
///
/// The borrow checker tracks the state of each place to ensure memory safety.
/// State transitions follow strict rules based on operations performed.
enum class OwnershipState {
    Owned,       ///< Value is owned and valid - can be used, moved, or borrowed
    Moved,       ///< Value has been moved - cannot be used until reassigned
    Borrowed,    ///< Value is immutably borrowed - can be read but not modified
    MutBorrowed, ///< Value is mutably borrowed - cannot be accessed at all
    Dropped,     ///< Value has been dropped - cannot be accessed
};

/// Whether a type uses copy or move semantics.
///
/// Copy types are implicitly duplicated when assigned or passed to functions.
/// Move types transfer ownership, making the source invalid after the operation.
///
/// # Copy Types
///
/// Primitive types like `I32`, `Bool`, `F64` are Copy. Composite types are Copy
/// only if all their fields are Copy.
///
/// # Move Types
///
/// Types with resources (heap allocations, file handles) use move semantics.
/// This includes `String`, `Vec[T]`, and most user-defined types.
enum class MoveSemantics {
    Copy, ///< Type can be implicitly copied
    Move, ///< Type must be explicitly moved (ownership transfer)
};

/// Error codes for categorizing borrow checker errors.
///
/// Each error code corresponds to a specific violation of borrowing rules.
/// Error codes are prefixed with 'B' in diagnostics (e.g., B001).
enum class BorrowErrorCode {
    UseAfterMove,        ///< B001: Use of moved value
    MoveWhileBorrowed,   ///< B002: Cannot move because value is borrowed
    AssignNotMutable,    ///< B003: Cannot assign to immutable variable
    AssignWhileBorrowed, ///< B004: Cannot assign because value is borrowed
    BorrowAfterMove,     ///< B005: Cannot borrow moved value
    MutBorrowNotMutable, ///< B006: Cannot mutably borrow non-mutable variable
    MutBorrowWhileImmut, ///< B007: Cannot mutably borrow while immutably borrowed
    DoubleMutBorrow,     ///< B008: Cannot borrow mutably more than once
    ImmutBorrowWhileMut, ///< B009: Cannot immutably borrow while mutably borrowed
    ReturnLocalRef,      ///< B010: Cannot return reference to local
    PartialMove,         ///< B011: Partial move detected
    OverlappingBorrow,   ///< B012: Overlapping borrows conflict
    UseWhileBorrowed,    ///< B013: Cannot use value while borrowed
    Other,               ///< B099: Other borrow errors
};

/// A suggestion for fixing a borrow error.
///
/// Suggestions help users understand how to resolve borrow checker errors
/// by providing human-readable explanations and optional code fixes.
struct BorrowSuggestion {
    /// Human-readable description of the suggested fix.
    std::string message;

    /// Optional code snippet to apply (e.g., `.duplicate()`).
    std::optional<std::string> fix;
};

/// A borrow checking error with rich diagnostic information.
///
/// `BorrowError` provides detailed information about borrow violations including:
/// - The error category and message
/// - Source location of the error
/// - Related locations (e.g., where a value was moved)
/// - Suggestions for fixing the error
///
/// # Example Error
///
/// ```text
/// error[B001]: use of moved value `x`
///  --> src/main.tml:10:5
///   |
/// 8 |     let y = x;  // value moved here
///   |             - value moved here
/// 10|     println(x); // error: use after move
///   |     ^^^^^^^^^ value used after move
///   |
/// help: consider using `.duplicate()` to create a copy
/// ```
struct BorrowError {
    /// The error category code.
    BorrowErrorCode code = BorrowErrorCode::Other;

    /// The primary error message.
    std::string message;

    /// Source location of the error.
    SourceSpan span;

    /// Additional notes explaining the error.
    std::vector<std::string> notes;

    /// Related source location (e.g., where value was moved/borrowed).
    std::optional<SourceSpan> related_span;

    /// Message describing the related span.
    std::optional<std::string> related_message;

    /// Suggestions for fixing the error.
    std::vector<BorrowSuggestion> suggestions;

    /// Creates a "use after move" error (B001).
    static auto use_after_move(const std::string& name, SourceSpan use_span, SourceSpan move_span)
        -> BorrowError;

    /// Creates a "double mutable borrow" error (B008).
    static auto double_mut_borrow(const std::string& name, SourceSpan second_span,
                                  SourceSpan first_span) -> BorrowError;

    /// Creates a "mutable borrow while immutably borrowed" error (B007).
    static auto mut_borrow_while_immut(const std::string& name, SourceSpan mut_span,
                                       SourceSpan immut_span) -> BorrowError;

    /// Creates an "immutable borrow while mutably borrowed" error (B009).
    static auto immut_borrow_while_mut(const std::string& name, SourceSpan immut_span,
                                       SourceSpan mut_span) -> BorrowError;

    /// Creates a "return reference to local" error (B010).
    static auto return_local_ref(const std::string& name, SourceSpan return_span,
                                 SourceSpan def_span) -> BorrowError;
};

/// Tracks the complete state of a single place (variable or memory location).
///
/// `PlaceState` contains all information needed to check borrowing rules for
/// a specific variable, including its type, mutability, active borrows, and
/// move status.
struct PlaceState {
    /// The variable's name in source code.
    std::string name;

    /// The type of the place.
    types::TypePtr type;

    /// Current ownership state.
    OwnershipState state = OwnershipState::Owned;

    /// Whether the place was declared as mutable (`let mut`).
    bool is_mutable = false;

    /// List of currently active borrows of this place.
    std::vector<Borrow> active_borrows;

    /// Location where this place was defined.
    Location definition;

    /// Location of the last use (for NLL).
    std::optional<Location> last_use;

    /// If this place holds a reference, tracks what it borrowed from.
    ///
    /// The pair contains the borrowed place's ID and the kind of borrow.
    std::optional<std::pair<PlaceId, BorrowKind>> borrowed_from;

    /// Set of field names that have been moved out (for partial move detection).
    std::set<std::string> moved_fields;

    /// Whether this place has been initialized.
    bool is_initialized = true;

    /// Location where a move occurred (for error reporting).
    std::optional<Location> move_location;
};

/// The move state of a place with respect to partial moves.
///
/// Partial moves occur when individual fields of a struct are moved while
/// the struct itself is not. The borrow checker must track this to prevent
/// using partially-moved values.
enum class MoveState {
    FullyOwned,     ///< All parts are owned - value can be used normally
    PartiallyMoved, ///< Some fields have been moved out
    FullyMoved,     ///< The entire value has been moved
};

/// Environment for tracking place states during borrow checking.
///
/// `BorrowEnv` maintains the state of all variables and their borrows during
/// the checking of a function. It supports:
///
/// - Variable definition and lookup
/// - Scope management (push/pop)
/// - Borrow tracking with NLL support
/// - Partial move detection
///
/// # Scope Handling
///
/// The environment maintains a stack of scopes. When a scope is pushed,
/// new variables are tracked in that scope. When popped, those variables
/// are dropped and their borrows released.
///
/// # NLL Integration
///
/// The environment tracks `last_use` for each place and borrow, enabling
/// Non-Lexical Lifetimes. Borrows can be released before scope end when
/// their last use is determined.
class BorrowEnv {
public:
    BorrowEnv() = default;

    /// Defines a new variable in the current scope.
    ///
    /// Returns the unique `PlaceId` assigned to this variable.
    auto define(const std::string& name, types::TypePtr type, bool is_mut, Location loc) -> PlaceId;

    /// Looks up a place by name in the current and enclosing scopes.
    ///
    /// Returns `nullopt` if the name is not found.
    auto lookup(const std::string& name) const -> std::optional<PlaceId>;

    /// Gets the state of a place (read-only).
    auto get_state(PlaceId id) const -> const PlaceState&;

    /// Gets the state of a place (mutable).
    auto get_state_mut(PlaceId id) -> PlaceState&;

    /// Marks a place as used at the given location.
    ///
    /// Updates `last_use` for NLL tracking and propagates to active borrows.
    void mark_used(PlaceId id, Location loc);

    /// Marks a reference place as used.
    ///
    /// This updates the underlying borrow's `last_use` for NLL.
    void mark_ref_used(PlaceId ref_place, Location loc);

    /// Pushes a new scope onto the scope stack.
    void push_scope();

    /// Pops the current scope, dropping all variables defined in it.
    void pop_scope();

    /// Returns the places defined in the current scope.
    auto current_scope_places() const -> const std::vector<PlaceId>&;

    /// Returns the current scope nesting depth.
    auto scope_depth() const -> size_t {
        return scopes_.size();
    }

    /// Releases all borrows created at the given scope depth.
    void release_borrows_at_depth(size_t depth, Location loc);

    /// Releases borrows that are no longer used at the given location (NLL).
    void release_dead_borrows(Location loc);

    /// Checks if a borrow is still live at the given location (NLL).
    auto is_borrow_live(const Borrow& borrow, Location loc) const -> bool;

    /// Returns a mutable reference to all places.
    auto all_places() -> std::unordered_map<PlaceId, PlaceState>& {
        return places_;
    }

    /// Returns a const reference to all places.
    auto all_places() const -> const std::unordered_map<PlaceId, PlaceState>& {
        return places_;
    }

    /// Allocates a new unique lifetime ID.
    auto next_lifetime_id() -> LifetimeId {
        return next_lifetime_id_++;
    }

    /// Marks a field as moved for partial move tracking.
    void mark_field_moved(PlaceId id, const std::string& field);

    /// Gets the move state of a place.
    auto get_move_state(PlaceId id) const -> MoveState;

    /// Checks if a specific field has been moved.
    auto is_field_moved(PlaceId id, const std::string& field) const -> bool;

private:
    /// Maps variable names to their PlaceIds (supports shadowing via vector).
    std::unordered_map<std::string, std::vector<PlaceId>> name_to_place_;

    /// Maps PlaceIds to their state.
    std::unordered_map<PlaceId, PlaceState> places_;

    /// Stack of scopes, each containing PlaceIds defined in that scope.
    std::vector<std::vector<PlaceId>> scopes_;

    /// Next PlaceId to allocate.
    PlaceId next_id_ = 0;

    /// Next LifetimeId to allocate.
    LifetimeId next_lifetime_id_ = 0;
};

/// The main borrow checker that validates ownership and borrowing rules.
///
/// `BorrowChecker` analyzes a TML module to ensure memory safety without
/// runtime garbage collection. It enforces:
///
/// - **Ownership**: Each value has exactly one owner
/// - **Borrowing**: References must not outlive their referents
/// - **Exclusivity**: Mutable references are exclusive
/// - **Initialization**: Values must be initialized before use
///
/// # Usage
///
/// ```cpp
/// BorrowChecker checker;
/// auto result = checker.check_module(module);
/// if (result.is_err()) {
///     for (const auto& error : result.error()) {
///         report_error(error);
///     }
/// }
/// ```
///
/// # Non-Lexical Lifetimes
///
/// The checker implements NLL, meaning borrows end at their last use rather
/// than at scope boundaries. This is more permissive and matches programmer
/// intuition better than lexical lifetimes.
///
/// # Two-Phase Borrows
///
/// For method calls like `x.push(x.len())`, the checker supports two-phase
/// borrows where a mutable borrow is created but only "activated" when the
/// mutation actually occurs.
class BorrowChecker {
public:
    BorrowChecker();

    /// Checks an entire module for borrow violations.
    ///
    /// Returns `Ok(true)` if the module passes borrow checking, or
    /// `Err(errors)` with a list of all violations found.
    [[nodiscard]] auto check_module(const parser::Module& module)
        -> Result<bool, std::vector<BorrowError>>;

    /// Returns all accumulated errors.
    [[nodiscard]] auto errors() const -> const std::vector<BorrowError>& {
        return errors_;
    }

    /// Returns true if any errors were found.
    [[nodiscard]] auto has_errors() const -> bool {
        return !errors_.empty();
    }

private:
    /// The borrow checking environment.
    BorrowEnv env_;

    /// Accumulated errors.
    std::vector<BorrowError> errors_;

    /// Current statement index for location tracking.
    size_t current_stmt_ = 0;

    /// Current loop nesting depth (for break/continue analysis).
    int loop_depth_ = 0;

    /// Whether a two-phase borrow is currently active.
    bool is_two_phase_borrow_active_ = false;

    // ========================================================================
    // Type Analysis
    // ========================================================================

    /// Determines if a type implements Copy semantics.
    auto is_copy_type(const types::TypePtr& type) const -> bool;

    /// Gets the move semantics for a type.
    auto get_move_semantics(const types::TypePtr& type) const -> MoveSemantics;

    // ========================================================================
    // Declaration Checking
    // ========================================================================

    /// Checks a function declaration.
    void check_func_decl(const parser::FuncDecl& func);

    /// Checks an impl block.
    void check_impl_decl(const parser::ImplDecl& impl);

    // ========================================================================
    // Statement Checking
    // ========================================================================

    /// Dispatches to the appropriate statement checker.
    void check_stmt(const parser::Stmt& stmt);

    /// Checks a let binding.
    void check_let(const parser::LetStmt& let);

    /// Checks an expression statement.
    void check_expr_stmt(const parser::ExprStmt& expr_stmt);

    // ========================================================================
    // Expression Checking
    // ========================================================================

    /// Dispatches to the appropriate expression checker.
    void check_expr(const parser::Expr& expr);

    /// Checks an identifier expression (variable use).
    void check_ident(const parser::IdentExpr& ident, SourceSpan span);

    /// Checks a binary expression.
    void check_binary(const parser::BinaryExpr& binary);

    /// Checks a unary expression.
    void check_unary(const parser::UnaryExpr& unary);

    /// Checks a function call.
    void check_call(const parser::CallExpr& call);

    /// Checks a method call.
    void check_method_call(const parser::MethodCallExpr& call);

    /// Checks a field access expression.
    void check_field_access(const parser::FieldExpr& field);

    /// Checks an index expression.
    void check_index(const parser::IndexExpr& idx);

    /// Checks a block expression.
    void check_block(const parser::BlockExpr& block);

    /// Checks an if expression.
    void check_if(const parser::IfExpr& if_expr);

    /// Checks a when (match) expression.
    void check_when(const parser::WhenExpr& when);

    /// Checks a loop expression.
    void check_loop(const parser::LoopExpr& loop);

    /// Checks a for expression.
    void check_for(const parser::ForExpr& for_expr);

    /// Checks a return expression.
    void check_return(const parser::ReturnExpr& ret);

    /// Checks a break expression.
    void check_break(const parser::BreakExpr& brk);

    /// Checks a tuple expression.
    void check_tuple(const parser::TupleExpr& tuple);

    /// Checks an array expression.
    void check_array(const parser::ArrayExpr& array);

    /// Checks a struct instantiation expression.
    void check_struct_expr(const parser::StructExpr& struct_expr);

    /// Checks a closure expression.
    void check_closure(const parser::ClosureExpr& closure);

    // ========================================================================
    // Borrow Operations
    // ========================================================================

    /// Creates a new borrow of a place.
    void create_borrow(PlaceId place, BorrowKind kind, Location loc);

    /// Creates a new borrow with projection information.
    void create_borrow_with_projection(PlaceId place, const Place& full_place, BorrowKind kind,
                                       Location loc, PlaceId ref_place);

    /// Releases a borrow of a place.
    void release_borrow(PlaceId place, BorrowKind kind, Location loc);

    /// Moves a value out of a place.
    void move_value(PlaceId place, Location loc);

    /// Moves a single field out of a place (partial move).
    void move_field(PlaceId place, const std::string& field, Location loc);

    /// Checks if a place can be used (read).
    void check_can_use(PlaceId place, Location loc);

    /// Checks if a field can be used.
    void check_can_use_field(PlaceId place, const std::string& field, Location loc);

    /// Checks if a place can be mutated.
    void check_can_mutate(PlaceId place, Location loc);

    /// Checks if a place can be borrowed.
    void check_can_borrow(PlaceId place, BorrowKind kind, Location loc);

    /// Checks if a place with projections can be borrowed.
    void check_can_borrow_with_projection(PlaceId place, const Place& full_place, BorrowKind kind,
                                          Location loc);

    /// Creates a reborrow from an existing reference.
    void create_reborrow(PlaceId source, PlaceId target, BorrowKind kind, Location loc);

    // ========================================================================
    // Two-Phase Borrows
    // ========================================================================

    /// Begins a two-phase borrow region.
    void begin_two_phase_borrow();

    /// Ends a two-phase borrow region.
    void end_two_phase_borrow();

    // ========================================================================
    // Scope and Lifetime Management
    // ========================================================================

    /// Drops all places at the end of the current scope.
    void drop_scope_places();

    /// Checks for dangling references in a return expression.
    void check_return_borrows(const parser::ReturnExpr& ret);

    /// Applies NLL: releases dead borrows at the current location.
    void apply_nll(Location loc);

    // ========================================================================
    // Error Reporting
    // ========================================================================

    /// Reports a simple error.
    void error(const std::string& message, SourceSpan span);

    /// Reports an error with a related note.
    void error_with_note(const std::string& message, SourceSpan span, const std::string& note,
                         SourceSpan note_span);

    // ========================================================================
    // Helpers
    // ========================================================================

    /// Creates a Location from the current statement index and span.
    auto current_location(SourceSpan span) const -> Location;

    /// Extracts a Place from an expression (if possible).
    auto extract_place(const parser::Expr& expr) -> std::optional<Place>;

    /// Gets the human-readable name for a place.
    auto get_place_name(const Place& place) const -> std::string;

    /// Maps reference places to the places they borrow from.
    std::unordered_map<PlaceId, PlaceId> ref_to_borrowed_;
};

} // namespace tml::borrow

#endif // TML_BORROW_CHECKER_HPP
