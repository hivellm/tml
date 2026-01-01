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

// Unique identifier for a place (variable, field access, etc.)
using PlaceId = uint64_t;

// Unique identifier for a lifetime
using LifetimeId = uint64_t;

// Represents a location in the program
struct Location {
    size_t statement_index;
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

// Place projection - represents access path like x.field, x[i], *x
enum class ProjectionKind {
    Field, // .field
    Index, // [i]
    Deref, // *ptr
};

struct Projection {
    ProjectionKind kind;
    std::string field_name; // For Field projection
};

// A place with projections (e.g., x.field.subfield)
struct Place {
    PlaceId base;
    std::vector<Projection> projections;

    // Check if this place is a prefix of another (e.g., x is prefix of x.field)
    auto is_prefix_of(const Place& other) const -> bool;
    // Check if two places overlap (could conflict)
    auto overlaps_with(const Place& other) const -> bool;
    // Get string representation
    auto to_string(const std::string& base_name) const -> std::string;
};

// Lifetime representation for NLL
struct Lifetime {
    LifetimeId id;
    Location start;
    std::optional<Location> end; // None if still live
    PlaceId borrowed_place;      // Which place this lifetime borrows

    auto is_live_at(const Location& loc) const -> bool {
        if (loc < start)
            return false;
        if (!end)
            return true;
        return loc <= *end;
    }
};

// Kind of borrow
enum class BorrowKind {
    Shared,  // ref T - immutable reference
    Mutable, // mut ref T - mutable reference
};

// Represents an active borrow with NLL support
struct Borrow {
    PlaceId place;
    Place full_place; // Full place with projections
    BorrowKind kind;
    Location start;
    std::optional<Location> end;      // None if still active
    std::optional<Location> last_use; // Last use for NLL (borrow ends here, not at scope end)
    size_t scope_depth;               // Scope level where borrow was created
    LifetimeId lifetime;              // Associated lifetime
    PlaceId ref_place;                // The place that holds this reference (for tracking)
};

// Ownership state of a place
enum class OwnershipState {
    Owned,       // Value is owned and valid
    Moved,       // Value has been moved
    Borrowed,    // Value is borrowed (immutably)
    MutBorrowed, // Value is mutably borrowed
    Dropped,     // Value has been dropped
};

// Whether a type is Copy or requires Move
enum class MoveSemantics {
    Copy, // Can be copied implicitly
    Move, // Must be moved (ownership transfer)
};

// Error codes for different borrow error categories
enum class BorrowErrorCode {
    UseAfterMove,           // B001: Use of moved value
    MoveWhileBorrowed,      // B002: Cannot move because value is borrowed
    AssignNotMutable,       // B003: Cannot assign to immutable variable
    AssignWhileBorrowed,    // B004: Cannot assign because value is borrowed
    BorrowAfterMove,        // B005: Cannot borrow moved value
    MutBorrowNotMutable,    // B006: Cannot mutably borrow non-mutable variable
    MutBorrowWhileImmut,    // B007: Cannot mutably borrow while immutably borrowed
    DoubleMutBorrow,        // B008: Cannot borrow mutably more than once
    ImmutBorrowWhileMut,    // B009: Cannot immutably borrow while mutably borrowed
    ReturnLocalRef,         // B010: Cannot return reference to local
    PartialMove,            // B011: Partial move detected
    OverlappingBorrow,      // B012: Overlapping borrows
    UseWhileBorrowed,       // B013: Cannot use while borrowed
    Other,                  // B099: Other borrow errors
};

// Suggestion for fixing borrow errors
struct BorrowSuggestion {
    std::string message;            // Human-readable suggestion
    std::optional<std::string> fix; // Optional code fix (e.g., ".duplicate()")
};

// Borrow checking error with rich diagnostics
struct BorrowError {
    BorrowErrorCode code = BorrowErrorCode::Other;
    std::string message;
    SourceSpan span;
    std::vector<std::string> notes;
    std::optional<SourceSpan> related_span;
    std::optional<std::string> related_message; // Context for related span
    std::vector<BorrowSuggestion> suggestions;  // Fix suggestions

    // Helpers for common error patterns
    static auto use_after_move(const std::string& name, SourceSpan use_span,
                               SourceSpan move_span) -> BorrowError;
    static auto double_mut_borrow(const std::string& name, SourceSpan second_span,
                                  SourceSpan first_span) -> BorrowError;
    static auto mut_borrow_while_immut(const std::string& name, SourceSpan mut_span,
                                       SourceSpan immut_span) -> BorrowError;
    static auto immut_borrow_while_mut(const std::string& name, SourceSpan immut_span,
                                       SourceSpan mut_span) -> BorrowError;
    static auto return_local_ref(const std::string& name, SourceSpan return_span,
                                 SourceSpan def_span) -> BorrowError;
};

// Tracks the state of a single place (variable)
struct PlaceState {
    std::string name;
    types::TypePtr type;
    OwnershipState state = OwnershipState::Owned;
    bool is_mutable = false;
    std::vector<Borrow> active_borrows;
    Location definition;
    std::optional<Location> last_use;
    // If this place is a borrow, tracks what it borrowed from and the kind
    std::optional<std::pair<PlaceId, BorrowKind>> borrowed_from;
    // Track which fields have been partially moved (for partial move detection)
    std::set<std::string> moved_fields;
    // Is this place initialized?
    bool is_initialized = true;
    // Track where a move occurred (for use-after-move errors)
    std::optional<Location> move_location;
};

// Move state for partial moves
enum class MoveState {
    FullyOwned,     // All parts are owned
    PartiallyMoved, // Some fields moved
    FullyMoved,     // Entire value moved
};

// Environment for borrow checking with NLL support
class BorrowEnv {
public:
    BorrowEnv() = default;

    // Define a new variable
    auto define(const std::string& name, types::TypePtr type, bool is_mut, Location loc) -> PlaceId;

    // Look up a place by name
    auto lookup(const std::string& name) const -> std::optional<PlaceId>;

    // Get state of a place
    auto get_state(PlaceId id) const -> const PlaceState&;
    auto get_state_mut(PlaceId id) -> PlaceState&;

    // Mark a place as used (for NLL) - updates last_use and borrow tracking
    void mark_used(PlaceId id, Location loc);

    // Mark a reference place as used (updates the borrow's last_use for NLL)
    void mark_ref_used(PlaceId ref_place, Location loc);

    // Enter/exit scope
    void push_scope();
    void pop_scope();

    // Get all places defined in current scope
    auto current_scope_places() const -> const std::vector<PlaceId>&;

    // Get current scope depth
    auto scope_depth() const -> size_t {
        return scopes_.size();
    }

    // Release all borrows created at the given scope depth across all places
    void release_borrows_at_depth(size_t depth, Location loc);

    // NLL: Release borrows that are no longer used at given location
    void release_dead_borrows(Location loc);

    // NLL: Check if a borrow is still live at given location
    auto is_borrow_live(const Borrow& borrow, Location loc) const -> bool;

    // Get all places (for releasing borrows)
    auto all_places() -> std::unordered_map<PlaceId, PlaceState>& {
        return places_;
    }
    auto all_places() const -> const std::unordered_map<PlaceId, PlaceState>& {
        return places_;
    }

    // Lifetime management
    auto next_lifetime_id() -> LifetimeId {
        return next_lifetime_id_++;
    }

    // Partial move tracking
    void mark_field_moved(PlaceId id, const std::string& field);
    auto get_move_state(PlaceId id) const -> MoveState;
    auto is_field_moved(PlaceId id, const std::string& field) const -> bool;

private:
    std::unordered_map<std::string, std::vector<PlaceId>> name_to_place_;
    std::unordered_map<PlaceId, PlaceState> places_;
    std::vector<std::vector<PlaceId>> scopes_;
    PlaceId next_id_ = 0;
    LifetimeId next_lifetime_id_ = 0;
};

// Main borrow checker
class BorrowChecker {
public:
    BorrowChecker();

    // Check an entire module
    // Returns true on success, vector of errors on failure
    [[nodiscard]] auto check_module(const parser::Module& module)
        -> Result<bool, std::vector<BorrowError>>;

    // Get accumulated errors
    [[nodiscard]] auto errors() const -> const std::vector<BorrowError>& {
        return errors_;
    }
    [[nodiscard]] auto has_errors() const -> bool {
        return !errors_.empty();
    }

private:
    BorrowEnv env_;
    std::vector<BorrowError> errors_;
    size_t current_stmt_ = 0;
    int loop_depth_ = 0;
    bool is_two_phase_borrow_active_ = false; // For two-phase borrow support

    // Determine if a type is Copy
    auto is_copy_type(const types::TypePtr& type) const -> bool;

    // Get move semantics for a type
    auto get_move_semantics(const types::TypePtr& type) const -> MoveSemantics;

    // Check declarations
    void check_func_decl(const parser::FuncDecl& func);
    void check_impl_decl(const parser::ImplDecl& impl);

    // Check statements
    void check_stmt(const parser::Stmt& stmt);
    void check_let(const parser::LetStmt& let);
    void check_expr_stmt(const parser::ExprStmt& expr_stmt);

    // Check expressions - returns if the expression produces a moved value
    void check_expr(const parser::Expr& expr);
    void check_ident(const parser::IdentExpr& ident, SourceSpan span);
    void check_binary(const parser::BinaryExpr& binary);
    void check_unary(const parser::UnaryExpr& unary);
    void check_call(const parser::CallExpr& call);
    void check_method_call(const parser::MethodCallExpr& call);
    void check_field_access(const parser::FieldExpr& field);
    void check_index(const parser::IndexExpr& idx);
    void check_block(const parser::BlockExpr& block);
    void check_if(const parser::IfExpr& if_expr);
    void check_when(const parser::WhenExpr& when);
    void check_loop(const parser::LoopExpr& loop);
    void check_for(const parser::ForExpr& for_expr);
    void check_return(const parser::ReturnExpr& ret);
    void check_break(const parser::BreakExpr& brk);
    void check_tuple(const parser::TupleExpr& tuple);
    void check_array(const parser::ArrayExpr& array);
    void check_struct_expr(const parser::StructExpr& struct_expr);
    void check_closure(const parser::ClosureExpr& closure);

    // Borrow operations
    void create_borrow(PlaceId place, BorrowKind kind, Location loc);
    void create_borrow_with_projection(PlaceId place, const Place& full_place, BorrowKind kind,
                                       Location loc, PlaceId ref_place);
    void release_borrow(PlaceId place, BorrowKind kind, Location loc);
    void move_value(PlaceId place, Location loc);
    void move_field(PlaceId place, const std::string& field, Location loc);
    void check_can_use(PlaceId place, Location loc);
    void check_can_use_field(PlaceId place, const std::string& field, Location loc);
    void check_can_mutate(PlaceId place, Location loc);
    void check_can_borrow(PlaceId place, BorrowKind kind, Location loc);
    void check_can_borrow_with_projection(PlaceId place, const Place& full_place, BorrowKind kind,
                                          Location loc);

    // Reborrow handling: create a borrow from an existing reference
    void create_reborrow(PlaceId source, PlaceId target, BorrowKind kind, Location loc);

    // Two-phase borrow support for method calls
    void begin_two_phase_borrow();
    void end_two_phase_borrow();

    // Drop places at end of scope
    void drop_scope_places();

    // NLL: Check for dangling references before return
    void check_return_borrows(const parser::ReturnExpr& ret);

    // NLL: Release dead borrows at current location
    void apply_nll(Location loc);

    // Error reporting
    void error(const std::string& message, SourceSpan span);
    void error_with_note(const std::string& message, SourceSpan span, const std::string& note,
                         SourceSpan note_span);

    // Helper to get current location
    auto current_location(SourceSpan span) const -> Location;

    // Place projection helpers
    auto extract_place(const parser::Expr& expr) -> std::optional<Place>;
    auto get_place_name(const Place& place) const -> std::string;

    // Track which places hold references (for NLL tracking)
    std::unordered_map<PlaceId, PlaceId> ref_to_borrowed_; // ref_place -> borrowed_place
};

} // namespace tml::borrow

#endif // TML_BORROW_CHECKER_HPP
