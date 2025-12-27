#ifndef TML_BORROW_CHECKER_HPP
#define TML_BORROW_CHECKER_HPP

#include "tml/common.hpp"
#include "tml/parser/ast.hpp"
#include "tml/types/type.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>

namespace tml::borrow {

// Unique identifier for a place (variable, field access, etc.)
using PlaceId = uint64_t;

// Represents a location in the program
struct Location {
    size_t statement_index;
    SourceSpan span;
};

// Kind of borrow
enum class BorrowKind {
    Shared,     // ref T - immutable reference
    Mutable,    // mut ref T - mutable reference
};

// Represents an active borrow
struct Borrow {
    PlaceId place;
    BorrowKind kind;
    Location start;
    std::optional<Location> end;  // None if still active
    size_t scope_depth;  // Scope level where borrow was created
};

// Ownership state of a place
enum class OwnershipState {
    Owned,      // Value is owned and valid
    Moved,      // Value has been moved
    Borrowed,   // Value is borrowed (immutably)
    MutBorrowed,// Value is mutably borrowed
    Dropped,    // Value has been dropped
};

// Whether a type is Copy or requires Move
enum class MoveSemantics {
    Copy,   // Can be copied implicitly
    Move,   // Must be moved (ownership transfer)
};

// Borrow checking error
struct BorrowError {
    std::string message;
    SourceSpan span;
    std::vector<std::string> notes;
    std::optional<SourceSpan> related_span;
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
};

// Environment for borrow checking
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

    // Mark a place as used (for NLL)
    void mark_used(PlaceId id, Location loc);

    // Enter/exit scope
    void push_scope();
    void pop_scope();

    // Get all places defined in current scope
    auto current_scope_places() const -> const std::vector<PlaceId>&;

    // Get current scope depth
    auto scope_depth() const -> size_t { return scopes_.size(); }

    // Release all borrows created at the given scope depth across all places
    void release_borrows_at_depth(size_t depth, Location loc);

    // Get all places (for releasing borrows)
    auto all_places() -> std::unordered_map<PlaceId, PlaceState>& { return places_; }

private:
    std::unordered_map<std::string, std::vector<PlaceId>> name_to_place_;
    std::unordered_map<PlaceId, PlaceState> places_;
    std::vector<std::vector<PlaceId>> scopes_;
    PlaceId next_id_ = 0;
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
    [[nodiscard]] auto errors() const -> const std::vector<BorrowError>& { return errors_; }
    [[nodiscard]] auto has_errors() const -> bool { return !errors_.empty(); }

private:
    BorrowEnv env_;
    std::vector<BorrowError> errors_;
    size_t current_stmt_ = 0;
    int loop_depth_ = 0;
    bool is_two_phase_borrow_active_ = false;  // For two-phase borrow support

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
    void release_borrow(PlaceId place, BorrowKind kind, Location loc);
    void move_value(PlaceId place, Location loc);
    void check_can_use(PlaceId place, Location loc);
    void check_can_mutate(PlaceId place, Location loc);
    void check_can_borrow(PlaceId place, BorrowKind kind, Location loc);

    // Reborrow handling: create a borrow from an existing reference
    void create_reborrow(PlaceId source, PlaceId target, BorrowKind kind, Location loc);

    // Two-phase borrow support for method calls
    void begin_two_phase_borrow();
    void end_two_phase_borrow();

    // Drop places at end of scope
    void drop_scope_places();

    // Error reporting
    void error(const std::string& message, SourceSpan span);
    void error_with_note(const std::string& message, SourceSpan span,
                         const std::string& note, SourceSpan note_span);

    // Helper to get current location
    auto current_location(SourceSpan span) const -> Location;
};

} // namespace tml::borrow

#endif // TML_BORROW_CHECKER_HPP
