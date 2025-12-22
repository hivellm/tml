#include "tml/borrow/checker.hpp"
#include <algorithm>

namespace tml::borrow {

// ============================================================================
// BorrowEnv Implementation
// ============================================================================

auto BorrowEnv::define(const std::string& name, types::TypePtr type, bool is_mut, Location loc) -> PlaceId {
    PlaceId id = next_id_++;

    PlaceState state{
        .name = name,
        .type = std::move(type),
        .state = OwnershipState::Owned,
        .is_mutable = is_mut,
        .active_borrows = {},
        .definition = loc,
        .last_use = std::nullopt,
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
                    if (borrow.kind == BorrowKind::Mutable) has_active_mut = true;
                    else has_active_shared = true;
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

// ============================================================================
// BorrowChecker Implementation
// ============================================================================

BorrowChecker::BorrowChecker() = default;

auto BorrowChecker::check_module(const parser::Module& module)
    -> Result<bool, std::vector<BorrowError>> {
    errors_.clear();
    env_ = BorrowEnv{};

    for (const auto& decl : module.decls) {
        std::visit([this](const auto& d) {
            using T = std::decay_t<decltype(d)>;
            if constexpr (std::is_same_v<T, parser::FuncDecl>) {
                check_func_decl(d);
            } else if constexpr (std::is_same_v<T, parser::ImplDecl>) {
                check_impl_decl(d);
            }
            // Other declarations don't need borrow checking
        }, decl->kind);
    }

    if (has_errors()) {
        return errors_;
    }
    return true;
}

auto BorrowChecker::is_copy_type(const types::TypePtr& type) const -> bool {
    if (!type) return true;

    return std::visit([this](const auto& t) -> bool {
        using T = std::decay_t<decltype(t)>;

        if constexpr (std::is_same_v<T, types::PrimitiveType>) {
            // All primitives are Copy
            return true;
        }
        else if constexpr (std::is_same_v<T, types::RefType>) {
            // References are Copy (the reference, not the data)
            return true;
        }
        else if constexpr (std::is_same_v<T, types::TupleType>) {
            // Tuple is Copy if all elements are Copy
            for (const auto& elem : t.elements) {
                if (!is_copy_type(elem)) return false;
            }
            return true;
        }
        else if constexpr (std::is_same_v<T, types::ArrayType>) {
            // Array is Copy if element type is Copy
            return is_copy_type(t.element);
        }
        else {
            // Named types, functions, etc. are not Copy by default
            return false;
        }
    }, type->kind);
}

auto BorrowChecker::get_move_semantics(const types::TypePtr& type) const -> MoveSemantics {
    return is_copy_type(type) ? MoveSemantics::Copy : MoveSemantics::Move;
}

void BorrowChecker::check_func_decl(const parser::FuncDecl& func) {
    env_.push_scope();
    current_stmt_ = 0;

    // Register parameters - FuncParam is a simple struct with pattern, type, span
    for (const auto& param : func.params) {
        bool is_mut = false;
        std::string name;

        if (param.pattern->is<parser::IdentPattern>()) {
            const auto& ident = param.pattern->as<parser::IdentPattern>();
            is_mut = ident.is_mut;
            name = ident.name;
        } else {
            name = "_param";
        }

        auto loc = current_location(func.span);
        // Note: We'd need the resolved type here - using nullptr for now
        env_.define(name, nullptr, is_mut, loc);
    }

    // Check function body - body is std::optional<BlockExpr>
    if (func.body) {
        check_block(*func.body);
    }

    // Drop all places at end of function
    drop_scope_places();
    env_.pop_scope();
}

void BorrowChecker::check_impl_decl(const parser::ImplDecl& impl) {
    for (const auto& method : impl.methods) {
        check_func_decl(method);
    }
}

void BorrowChecker::check_stmt(const parser::Stmt& stmt) {
    std::visit([this](const auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, parser::LetStmt>) {
            check_let(s);
        }
        else if constexpr (std::is_same_v<T, parser::ExprStmt>) {
            check_expr_stmt(s);
        }
        else if constexpr (std::is_same_v<T, parser::DeclPtr>) {
            // Nested declaration - check if it's a function
            if (s) {
                std::visit([this](const auto& d) {
                    using D = std::decay_t<decltype(d)>;
                    if constexpr (std::is_same_v<D, parser::FuncDecl>) {
                        check_func_decl(d);
                    }
                }, s->kind);
            }
        }
    }, stmt.kind);

    current_stmt_++;
}

void BorrowChecker::check_let(const parser::LetStmt& let) {
    // Check initializer first
    if (let.init) {
        check_expr(**let.init);
    }

    // Bind the pattern
    std::visit([this, &let](const auto& p) {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, parser::IdentPattern>) {
            auto loc = current_location(let.span);
            env_.define(p.name, nullptr, p.is_mut, loc);
        }
        else if constexpr (std::is_same_v<T, parser::TuplePattern>) {
            // For tuple patterns, we'd need to destructure
            // For now, just register each sub-pattern
            for (const auto& sub : p.elements) {
                if (sub->is<parser::IdentPattern>()) {
                    const auto& ident = sub->as<parser::IdentPattern>();
                    auto loc = current_location(let.span);
                    env_.define(ident.name, nullptr, ident.is_mut, loc);
                }
            }
        }
        // Other patterns handled similarly
    }, let.pattern->kind);
}

void BorrowChecker::check_expr_stmt(const parser::ExprStmt& expr_stmt) {
    check_expr(*expr_stmt.expr);
}

void BorrowChecker::check_expr(const parser::Expr& expr) {
    std::visit([this, &expr](const auto& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, parser::LiteralExpr>) {
            // Literals don't involve borrowing
        }
        else if constexpr (std::is_same_v<T, parser::IdentExpr>) {
            check_ident(e, expr.span);
        }
        else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
            check_binary(e);
        }
        else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
            check_unary(e);
        }
        else if constexpr (std::is_same_v<T, parser::CallExpr>) {
            check_call(e);
        }
        else if constexpr (std::is_same_v<T, parser::MethodCallExpr>) {
            check_method_call(e);
        }
        else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
            check_field_access(e);
        }
        else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
            check_index(e);
        }
        else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
            check_block(e);
        }
        else if constexpr (std::is_same_v<T, parser::IfExpr>) {
            check_if(e);
        }
        else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
            check_when(e);
        }
        else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
            check_loop(e);
        }
        else if constexpr (std::is_same_v<T, parser::ForExpr>) {
            check_for(e);
        }
        else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
            check_return(e);
        }
        else if constexpr (std::is_same_v<T, parser::BreakExpr>) {
            check_break(e);
        }
        else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
            check_tuple(e);
        }
        else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
            check_array(e);
        }
        else if constexpr (std::is_same_v<T, parser::StructExpr>) {
            check_struct_expr(e);
        }
        else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
            check_closure(e);
        }
        // Other expressions handled as needed
    }, expr.kind);
}

void BorrowChecker::check_ident(const parser::IdentExpr& ident, SourceSpan span) {
    auto place_id = env_.lookup(ident.name);
    if (!place_id) {
        // Variable not found - might be a function call, let type checker handle it
        return;
    }

    auto loc = current_location(span);
    check_can_use(*place_id, loc);
    env_.mark_used(*place_id, loc);
}

void BorrowChecker::check_binary(const parser::BinaryExpr& binary) {
    check_expr(*binary.left);
    check_expr(*binary.right);

    // Assignment operators require mutable access to LHS
    if (binary.op == parser::BinaryOp::Assign ||
        binary.op == parser::BinaryOp::AddAssign ||
        binary.op == parser::BinaryOp::SubAssign ||
        binary.op == parser::BinaryOp::MulAssign ||
        binary.op == parser::BinaryOp::DivAssign ||
        binary.op == parser::BinaryOp::ModAssign) {

        // Check if LHS is a mutable place
        if (binary.left->is<parser::IdentExpr>()) {
            const auto& ident = binary.left->as<parser::IdentExpr>();
            auto place_id = env_.lookup(ident.name);
            if (place_id) {
                auto loc = current_location(binary.span);
                check_can_mutate(*place_id, loc);
            }
        }
    }
}

void BorrowChecker::check_unary(const parser::UnaryExpr& unary) {
    check_expr(*unary.operand);

    // Ref and RefMut create borrows
    if (unary.op == parser::UnaryOp::Ref || unary.op == parser::UnaryOp::RefMut) {
        if (unary.operand->is<parser::IdentExpr>()) {
            const auto& ident = unary.operand->as<parser::IdentExpr>();
            auto place_id = env_.lookup(ident.name);
            if (place_id) {
                auto loc = current_location(unary.span);
                auto kind = (unary.op == parser::UnaryOp::RefMut)
                    ? BorrowKind::Mutable
                    : BorrowKind::Shared;
                check_can_borrow(*place_id, kind, loc);
                create_borrow(*place_id, kind, loc);
            }
        }
    }
}

void BorrowChecker::check_call(const parser::CallExpr& call) {
    check_expr(*call.callee);
    for (const auto& arg : call.args) {
        check_expr(*arg);
    }
}

void BorrowChecker::check_method_call(const parser::MethodCallExpr& call) {
    check_expr(*call.receiver);
    for (const auto& arg : call.args) {
        check_expr(*arg);
    }
}

void BorrowChecker::check_field_access(const parser::FieldExpr& field) {
    check_expr(*field.object);
}

void BorrowChecker::check_index(const parser::IndexExpr& idx) {
    check_expr(*idx.object);
    check_expr(*idx.index);
}

void BorrowChecker::check_block(const parser::BlockExpr& block) {
    env_.push_scope();

    for (const auto& stmt : block.stmts) {
        check_stmt(*stmt);
    }

    if (block.expr) {
        check_expr(**block.expr);
    }

    drop_scope_places();
    env_.pop_scope();
}

void BorrowChecker::check_if(const parser::IfExpr& if_expr) {
    check_expr(*if_expr.condition);
    check_expr(*if_expr.then_branch);

    if (if_expr.else_branch) {
        check_expr(**if_expr.else_branch);
    }
}

void BorrowChecker::check_when(const parser::WhenExpr& when) {
    check_expr(*when.scrutinee);

    for (const auto& arm : when.arms) {
        // Each arm creates a new scope for pattern bindings
        env_.push_scope();

        // Bind pattern variables
        // For now, simplified handling
        if (arm.pattern->is<parser::IdentPattern>()) {
            const auto& ident = arm.pattern->as<parser::IdentPattern>();
            auto loc = current_location(arm.pattern->span);
            env_.define(ident.name, nullptr, ident.is_mut, loc);
        }

        // Check guard if present
        if (arm.guard) {
            check_expr(**arm.guard);
        }

        // Check body
        check_expr(*arm.body);

        drop_scope_places();
        env_.pop_scope();
    }
}

void BorrowChecker::check_loop(const parser::LoopExpr& loop) {
    loop_depth_++;
    env_.push_scope();

    check_expr(*loop.body);

    drop_scope_places();
    env_.pop_scope();
    loop_depth_--;
}

void BorrowChecker::check_for(const parser::ForExpr& for_expr) {
    // Check iterator expression
    check_expr(*for_expr.iter);

    loop_depth_++;
    env_.push_scope();

    // Bind loop variable
    if (for_expr.pattern->is<parser::IdentPattern>()) {
        const auto& ident = for_expr.pattern->as<parser::IdentPattern>();
        auto loc = current_location(for_expr.span);
        env_.define(ident.name, nullptr, ident.is_mut, loc);
    }

    check_expr(*for_expr.body);

    drop_scope_places();
    env_.pop_scope();
    loop_depth_--;
}

void BorrowChecker::check_return(const parser::ReturnExpr& ret) {
    if (ret.value) {
        check_expr(**ret.value);
    }
}

void BorrowChecker::check_break(const parser::BreakExpr& brk) {
    if (brk.value) {
        check_expr(**brk.value);
    }
}

void BorrowChecker::check_tuple(const parser::TupleExpr& tuple) {
    for (const auto& elem : tuple.elements) {
        check_expr(*elem);
    }
}

void BorrowChecker::check_array(const parser::ArrayExpr& array) {
    std::visit([this](const auto& a) {
        using T = std::decay_t<decltype(a)>;
        if constexpr (std::is_same_v<T, std::vector<parser::ExprPtr>>) {
            // Array literal: [1, 2, 3]
            for (const auto& elem : a) {
                check_expr(*elem);
            }
        }
        else if constexpr (std::is_same_v<T, std::pair<parser::ExprPtr, parser::ExprPtr>>) {
            // Array repeat: [expr; count]
            check_expr(*a.first);
            check_expr(*a.second);
        }
    }, array.kind);
}

void BorrowChecker::check_struct_expr(const parser::StructExpr& struct_expr) {
    for (const auto& [name, value] : struct_expr.fields) {
        check_expr(*value);
    }

    if (struct_expr.base) {
        check_expr(**struct_expr.base);
    }
}

void BorrowChecker::check_closure(const parser::ClosureExpr& closure) {
    env_.push_scope();

    // Register closure parameters - params is vector<pair<PatternPtr, optional<TypePtr>>>
    for (const auto& [pattern, type] : closure.params) {
        bool is_mut = false;
        std::string name;

        if (pattern->is<parser::IdentPattern>()) {
            const auto& ident = pattern->as<parser::IdentPattern>();
            is_mut = ident.is_mut;
            name = ident.name;
        } else {
            name = "_param";
        }

        auto loc = current_location(closure.span);
        env_.define(name, nullptr, is_mut, loc);
    }

    check_expr(*closure.body);

    drop_scope_places();
    env_.pop_scope();
}

void BorrowChecker::create_borrow(PlaceId place, BorrowKind kind, Location loc) {
    auto& state = env_.get_state_mut(place);

    Borrow borrow{
        .place = place,
        .kind = kind,
        .start = loc,
        .end = std::nullopt,
        .scope_depth = env_.scope_depth(),  // Track scope level
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
            if (borrow.kind == BorrowKind::Mutable) has_active_mut = true;
            else has_active_shared = true;
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

    if (state.state == OwnershipState::Borrowed ||
        state.state == OwnershipState::MutBorrowed) {
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

    if (kind == BorrowKind::Mutable) {
        if (!state.is_mutable) {
            error("cannot borrow `" + state.name + "` as mutable because it is not declared as mutable", loc.span);
            return;
        }

        if (state.state == OwnershipState::Borrowed) {
            error("cannot borrow `" + state.name + "` as mutable because it is also borrowed as immutable", loc.span);
            return;
        }

        if (state.state == OwnershipState::MutBorrowed) {
            error("cannot borrow `" + state.name + "` as mutable more than once at a time", loc.span);
            return;
        }
    } else {
        // Shared borrow
        if (state.state == OwnershipState::MutBorrowed) {
            error("cannot borrow `" + state.name + "` as immutable because it is also borrowed as mutable", loc.span);
            return;
        }
    }
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
