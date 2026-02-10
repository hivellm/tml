//! # Polonius Fact Generation
//!
//! Generates Polonius input facts by traversing the AST. This parallels the
//! existing `BorrowChecker` traversal in `checker_expr.cpp` / `checker_stmt.cpp`
//! but emits facts into a `FactTable` instead of checking rules directly.

#include "borrow/polonius.hpp"
#include "types/env.hpp"

#include <algorithm>
#include <sstream>

namespace tml::borrow::polonius {

// ============================================================================
// FactTable implementation
// ============================================================================

auto FactTable::fresh_origin(const std::string& debug_name, PlaceId ref_place) -> OriginId {
    auto id = next_origin_id_++;
    origins[id] = Origin{id, debug_name, ref_place};
    return id;
}

auto FactTable::fresh_loan(PlaceId place, const Place& full_place, BorrowKind kind, SourceSpan span)
    -> LoanId {
    auto id = next_loan_id_++;
    loans[id] = Loan{id, place, full_place, kind, span};
    return id;
}

auto FactTable::fresh_point(size_t stmt_index, PointPosition pos, SourceSpan span) -> PointId {
    auto id = next_point_id_++;
    points[id] = Point{id, stmt_index, pos, span};
    return id;
}

void FactTable::clear() {
    loan_issued_at.clear();
    loan_invalidated_at.clear();
    cfg_edges.clear();
    subset_constraints.clear();
    origin_live_at.clear();
    origin_contains_loan_at.clear();
    errors.clear();
    origins.clear();
    loans.clear();
    points.clear();
    next_origin_id_ = 0;
    next_loan_id_ = 0;
    next_point_id_ = 0;
}

// ============================================================================
// PoloniusFacts implementation
// ============================================================================

PoloniusFacts::PoloniusFacts(const types::TypeEnv& type_env) : type_env_(&type_env) {}

auto PoloniusFacts::advance_point(SourceSpan span) -> PointId {
    auto pt = facts_.fresh_point(current_stmt_, PointPosition::Start, span);
    if (last_point_ != INVALID_POINT) {
        emit_cfg_edge(last_point_, pt);
    }
    last_point_ = pt;
    return pt;
}

auto PoloniusFacts::create_point(SourceSpan span) -> PointId {
    return facts_.fresh_point(current_stmt_, PointPosition::Start, span);
}

void PoloniusFacts::emit_loan(PlaceId borrowed_place, const Place& full_place, BorrowKind kind,
                              PlaceId ref_place, SourceSpan span) {
    auto loan = facts_.fresh_loan(borrowed_place, full_place, kind, span);
    auto origin = place_origins_.count(ref_place) ? place_origins_[ref_place]
                                                  : facts_.fresh_origin("ref", ref_place);
    place_origins_[ref_place] = origin;

    // Track which loans belong to which place
    place_loans_[borrowed_place].push_back(loan);

    auto pt = (last_point_ != INVALID_POINT) ? last_point_ : advance_point(span);
    facts_.loan_issued_at.push_back({origin, loan, pt});
}

void PoloniusFacts::emit_invalidation(PlaceId place, SourceSpan span) {
    auto pt = (last_point_ != INVALID_POINT) ? last_point_ : advance_point(span);
    auto it = place_loans_.find(place);
    if (it != place_loans_.end()) {
        for (auto loan : it->second) {
            facts_.loan_invalidated_at.push_back({pt, loan});
        }
    }
}

void PoloniusFacts::emit_cfg_edge(PointId from, PointId to) {
    facts_.cfg_edges.push_back({from, to});
}

void PoloniusFacts::emit_subset(OriginId sub, OriginId sup, PointId point) {
    facts_.subset_constraints.push_back({sub, sup, point});
}

auto PoloniusFacts::is_copy_type(const types::TypePtr& type) const -> bool {
    if (!type)
        return true;
    return std::visit(
        [this](const auto& t) -> bool {
            using T = std::decay_t<decltype(t)>;
            if constexpr (std::is_same_v<T, types::PrimitiveType>) {
                return true;
            } else if constexpr (std::is_same_v<T, types::RefType>) {
                return true;
            } else if constexpr (std::is_same_v<T, types::TupleType>) {
                for (const auto& elem : t.elements) {
                    if (!is_copy_type(elem))
                        return false;
                }
                return true;
            } else if constexpr (std::is_same_v<T, types::ArrayType>) {
                return is_copy_type(t.element);
            } else if constexpr (std::is_same_v<T, types::NamedType>) {
                return type_env_ && type_env_->type_implements(t.name, "Copy");
            } else if constexpr (std::is_same_v<T, types::ClassType>) {
                return type_env_ && type_env_->type_implements(t.name, "Copy");
            } else {
                return false;
            }
        },
        type->kind);
}

auto PoloniusFacts::is_ref_type(const types::TypePtr& type) const -> bool {
    if (!type)
        return false;
    return type->is<types::RefType>();
}

auto PoloniusFacts::extract_place(const parser::Expr& expr) -> std::optional<Place> {
    if (expr.is<parser::IdentExpr>()) {
        const auto& ident = expr.as<parser::IdentExpr>();
        auto place_id = env_.lookup(ident.name);
        if (place_id) {
            return Place{*place_id, {}};
        }
    } else if (expr.is<parser::FieldExpr>()) {
        const auto& field = expr.as<parser::FieldExpr>();
        auto base = extract_place(*field.object);
        if (base) {
            base->projections.push_back(
                Projection{ProjectionKind::Field, field.field, std::nullopt});
            return base;
        }
    } else if (expr.is<parser::IndexExpr>()) {
        const auto& idx = expr.as<parser::IndexExpr>();
        auto base = extract_place(*idx.object);
        if (base) {
            base->projections.push_back(Projection{ProjectionKind::Index, "", std::nullopt});
            return base;
        }
    } else if (expr.is<parser::UnaryExpr>()) {
        const auto& unary = expr.as<parser::UnaryExpr>();
        if (unary.op == parser::UnaryOp::Deref) {
            auto base = extract_place(*unary.operand);
            if (base) {
                base->projections.push_back(Projection{ProjectionKind::Deref, "", std::nullopt});
                return base;
            }
        }
    }
    return std::nullopt;
}

// ============================================================================
// AST traversal — Function level
// ============================================================================

void PoloniusFacts::generate_function(const parser::FuncDecl& func) {
    facts_.clear();
    env_ = BorrowEnv{};
    place_origins_.clear();
    place_loans_.clear();
    current_stmt_ = 0;
    last_point_ = INVALID_POINT;
    loop_headers_.clear();
    loop_exits_.clear();

    env_.push_scope();

    // Create entry point
    auto entry_pt = advance_point(func.span);

    // Create exit point (for return edges)
    exit_point_ = create_point(func.span);

    // Register parameters
    for (const auto& param : func.params) {
        std::string name;
        bool is_mut = false;
        bool is_mut_ref = false;

        if (param.pattern->template is<parser::IdentPattern>()) {
            const auto& ident = param.pattern->template as<parser::IdentPattern>();
            is_mut = ident.is_mut;
            name = ident.name;
        } else {
            name = "_param";
        }

        if (param.type && param.type->template is<parser::RefType>()) {
            const auto& ref_type = param.type->template as<parser::RefType>();
            is_mut_ref = ref_type.is_mut;
        }

        auto loc = Location{current_stmt_, func.span};
        auto place_id = env_.define(name, nullptr, is_mut, loc, is_mut_ref);

        // If param is a reference, create an origin for it
        if (is_mut_ref || (param.type && param.type->template is<parser::RefType>())) {
            auto origin = facts_.fresh_origin("param_" + name, place_id);
            place_origins_[place_id] = origin;
        }

        (void)entry_pt; // entry point is used as the initial last_point_
    }

    // Check body
    if (func.body) {
        visit_block(*func.body);
    }

    // Add CFG edge from last point to exit
    if (last_point_ != INVALID_POINT && last_point_ != exit_point_) {
        emit_cfg_edge(last_point_, exit_point_);
    }

    env_.pop_scope();

    // Compute liveness
    compute_liveness();
}

// ============================================================================
// AST traversal — Blocks and statements
// ============================================================================

void PoloniusFacts::visit_block(const parser::BlockExpr& block) {
    env_.push_scope();

    for (const auto& stmt : block.stmts) {
        visit_stmt(*stmt);
    }

    if (block.expr) {
        visit_expr(**block.expr);
    }

    env_.pop_scope();
}

void PoloniusFacts::visit_stmt(const parser::Stmt& stmt) {
    std::visit(
        [this](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, parser::LetStmt>) {
                visit_let(s);
            } else if constexpr (std::is_same_v<T, parser::ExprStmt>) {
                visit_expr_stmt(s);
            } else if constexpr (std::is_same_v<T, parser::DeclPtr>) {
                if (s) {
                    std::visit(
                        [this](const auto& d) {
                            using D = std::decay_t<decltype(d)>;
                            if constexpr (std::is_same_v<D, parser::FuncDecl>) {
                                // Nested functions are checked independently
                            }
                        },
                        s->kind);
                }
            }
        },
        stmt.kind);

    current_stmt_++;
}

void PoloniusFacts::visit_let(const parser::LetStmt& let) {
    // Check initializer first
    if (let.init) {
        visit_expr(**let.init);
    }

    auto pt = advance_point(let.span);

    bool is_mut_ref = false;
    if (let.type_annotation && (*let.type_annotation)->is<parser::RefType>()) {
        is_mut_ref = (*let.type_annotation)->as<parser::RefType>().is_mut;
    }

    bool is_initialized = let.init.has_value();
    auto loc = Location{current_stmt_, let.span};

    // Bind the pattern
    std::visit(
        [this, &let, &loc, is_mut_ref, is_initialized, pt](const auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, parser::IdentPattern>) {
                auto place_id =
                    env_.define(p.name, nullptr, p.is_mut, loc, is_mut_ref, is_initialized);

                // If this let binds a reference, check if it's a reborrow
                if (let.init && is_initialized) {
                    const auto& init = **let.init;
                    if (init.is<parser::UnaryExpr>()) {
                        const auto& unary = init.as<parser::UnaryExpr>();
                        if (unary.op == parser::UnaryOp::Ref ||
                            unary.op == parser::UnaryOp::RefMut) {
                            auto full_place = extract_place(*unary.operand);
                            if (full_place) {
                                auto kind = (unary.op == parser::UnaryOp::RefMut)
                                                ? BorrowKind::Mutable
                                                : BorrowKind::Shared;
                                emit_loan(full_place->base, *full_place, kind, place_id, let.span);

                                // Check for reborrow: ref *r (deref projection)
                                if (!full_place->projections.empty() &&
                                    full_place->projections.back().kind == ProjectionKind::Deref) {
                                    auto source_place = full_place->base;
                                    if (place_origins_.count(source_place)) {
                                        auto source_origin = place_origins_[source_place];
                                        auto new_origin = place_origins_.count(place_id)
                                                              ? place_origins_[place_id]
                                                              : facts_.fresh_origin(
                                                                    "reborrow_" + p.name, place_id);
                                        place_origins_[place_id] = new_origin;
                                        emit_subset(new_origin, source_origin, pt);
                                    }
                                }
                            }
                        }
                    } else if (init.is<parser::IdentExpr>()) {
                        // Assignment from another reference: let r2 = r1
                        const auto& ident = init.as<parser::IdentExpr>();
                        auto src_place = env_.lookup(ident.name);
                        if (src_place && place_origins_.count(*src_place)) {
                            auto src_origin = place_origins_[*src_place];
                            auto new_origin = facts_.fresh_origin("assign_" + p.name, place_id);
                            place_origins_[place_id] = new_origin;
                            emit_subset(new_origin, src_origin, pt);
                        }
                    }
                }
            } else if constexpr (std::is_same_v<T, parser::TuplePattern>) {
                for (const auto& sub : p.elements) {
                    if (sub->template is<parser::IdentPattern>()) {
                        const auto& ident = sub->template as<parser::IdentPattern>();
                        env_.define(ident.name, nullptr, ident.is_mut, loc, is_mut_ref,
                                    is_initialized);
                    }
                }
            }
        },
        let.pattern->kind);
}

void PoloniusFacts::visit_expr_stmt(const parser::ExprStmt& expr_stmt) {
    visit_expr(*expr_stmt.expr);
}

// ============================================================================
// AST traversal — Expressions
// ============================================================================

void PoloniusFacts::visit_expr(const parser::Expr& expr) {
    std::visit(
        [this, &expr](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::LiteralExpr>) {
                // Literals don't involve borrowing
            } else if constexpr (std::is_same_v<T, parser::IdentExpr>) {
                visit_ident(e, expr.span);
            } else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
                visit_binary(e);
            } else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
                visit_unary(e);
            } else if constexpr (std::is_same_v<T, parser::CallExpr>) {
                visit_call(e);
            } else if constexpr (std::is_same_v<T, parser::MethodCallExpr>) {
                visit_method_call(e);
            } else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
                visit_field_access(e);
            } else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
                visit_index(e);
            } else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
                visit_block(e);
            } else if constexpr (std::is_same_v<T, parser::IfExpr>) {
                visit_if(e);
            } else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
                visit_when(e);
            } else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
                visit_loop(e);
            } else if constexpr (std::is_same_v<T, parser::ForExpr>) {
                visit_for(e);
            } else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
                visit_return(e);
            } else if constexpr (std::is_same_v<T, parser::BreakExpr>) {
                visit_break(e);
            } else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
                visit_tuple(e);
            } else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
                visit_array(e);
            } else if constexpr (std::is_same_v<T, parser::StructExpr>) {
                visit_struct_expr(e);
            } else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
                visit_closure(e);
            }
            // Other expression types: no borrow-relevant facts
        },
        expr.kind);
}

void PoloniusFacts::visit_ident(const parser::IdentExpr& ident, SourceSpan span) {
    auto place_id = env_.lookup(ident.name);
    if (!place_id)
        return;

    // Mark used for liveness tracking
    auto loc = Location{current_stmt_, span};
    env_.mark_used(*place_id, loc);
}

void PoloniusFacts::visit_binary(const parser::BinaryExpr& binary) {
    if (binary.op == parser::BinaryOp::Assign) {
        // Don't visit LHS as a use for simple identifiers
        if (!binary.left->template is<parser::IdentExpr>()) {
            visit_expr(*binary.left);
        }
        visit_expr(*binary.right);

        // Assignment invalidates loans on LHS
        if (binary.left->template is<parser::IdentExpr>()) {
            const auto& ident = binary.left->template as<parser::IdentExpr>();
            auto place_id = env_.lookup(ident.name);
            if (place_id) {
                emit_invalidation(*place_id, binary.span);
            }
        }
    } else if (binary.op == parser::BinaryOp::AddAssign ||
               binary.op == parser::BinaryOp::SubAssign ||
               binary.op == parser::BinaryOp::MulAssign ||
               binary.op == parser::BinaryOp::DivAssign ||
               binary.op == parser::BinaryOp::ModAssign) {
        visit_expr(*binary.left);
        visit_expr(*binary.right);

        // Compound assignment also invalidates loans
        if (binary.left->template is<parser::IdentExpr>()) {
            const auto& ident = binary.left->template as<parser::IdentExpr>();
            auto place_id = env_.lookup(ident.name);
            if (place_id) {
                emit_invalidation(*place_id, binary.span);
            }
        }
    } else {
        visit_expr(*binary.left);
        visit_expr(*binary.right);
    }
}

void PoloniusFacts::visit_unary(const parser::UnaryExpr& unary) {
    visit_expr(*unary.operand);

    // ref/mut ref creates a borrow — loans are emitted in visit_let when bound
    // Standalone ref expressions (not bound to a let) still need tracking
    if (unary.op == parser::UnaryOp::Ref || unary.op == parser::UnaryOp::RefMut) {
        // The loan emission happens in visit_let for `let r = ref x`
        // For standalone `ref x` (e.g., in function arguments), we emit here
        auto full_place = extract_place(*unary.operand);
        if (full_place) {
            // Note: For standalone refs (not bound to a variable), we use place 0
            // as a temporary ref_place. The let-binding case handles real tracking.
        }
    }
}

void PoloniusFacts::visit_call(const parser::CallExpr& call) {
    visit_expr(*call.callee);
    for (const auto& arg : call.args) {
        visit_expr(*arg);
    }
}

void PoloniusFacts::visit_method_call(const parser::MethodCallExpr& call) {
    visit_expr(*call.receiver);
    for (const auto& arg : call.args) {
        visit_expr(*arg);
    }
}

void PoloniusFacts::visit_field_access(const parser::FieldExpr& field_expr) {
    visit_expr(*field_expr.object);
}

void PoloniusFacts::visit_index(const parser::IndexExpr& idx) {
    visit_expr(*idx.object);
    visit_expr(*idx.index);
}

// ============================================================================
// Control flow — emits CFG edges
// ============================================================================

void PoloniusFacts::visit_if(const parser::IfExpr& if_expr) {
    visit_expr(*if_expr.condition);

    auto cond_point = last_point_;

    // Then branch
    auto then_start = create_point(if_expr.then_branch->span);
    emit_cfg_edge(cond_point, then_start);
    last_point_ = then_start;
    visit_expr(*if_expr.then_branch);
    auto then_end = last_point_;

    if (if_expr.else_branch) {
        // Else branch
        auto else_start = create_point((*if_expr.else_branch)->span);
        emit_cfg_edge(cond_point, else_start);
        last_point_ = else_start;
        visit_expr(**if_expr.else_branch);
        auto else_end = last_point_;

        // Merge point
        auto merge = create_point(if_expr.then_branch->span);
        emit_cfg_edge(then_end, merge);
        emit_cfg_edge(else_end, merge);
        last_point_ = merge;
    } else {
        // No else: flow can skip the then branch
        auto merge = create_point(if_expr.then_branch->span);
        emit_cfg_edge(then_end, merge);
        emit_cfg_edge(cond_point, merge);
        last_point_ = merge;
    }
}

void PoloniusFacts::visit_when(const parser::WhenExpr& when) {
    visit_expr(*when.scrutinee);

    auto pre_when_point = last_point_;
    std::vector<PointId> arm_ends;

    for (const auto& arm : when.arms) {
        auto arm_start = create_point(arm.body->span);
        emit_cfg_edge(pre_when_point, arm_start);
        last_point_ = arm_start;

        env_.push_scope();
        if (arm.pattern->template is<parser::IdentPattern>()) {
            const auto& ident = arm.pattern->template as<parser::IdentPattern>();
            auto loc = Location{current_stmt_, arm.pattern->span};
            env_.define(ident.name, nullptr, ident.is_mut, loc);
        }

        if (arm.guard) {
            visit_expr(**arm.guard);
        }
        visit_expr(*arm.body);
        env_.pop_scope();

        arm_ends.push_back(last_point_);
    }

    // Merge all arms
    auto merge = create_point(when.scrutinee->span);
    for (auto arm_end : arm_ends) {
        emit_cfg_edge(arm_end, merge);
    }
    last_point_ = merge;
}

void PoloniusFacts::visit_loop(const parser::LoopExpr& loop) {
    auto loop_header = create_point(loop.body->span);
    if (last_point_ != INVALID_POINT) {
        emit_cfg_edge(last_point_, loop_header);
    }
    last_point_ = loop_header;

    auto loop_exit = create_point(loop.body->span);

    loop_headers_.push_back(loop_header);
    loop_exits_.push_back(loop_exit);

    env_.push_scope();
    visit_expr(*loop.body);
    env_.pop_scope();

    // Back edge: loop body end → loop header
    if (last_point_ != INVALID_POINT) {
        emit_cfg_edge(last_point_, loop_header);
    }

    loop_headers_.pop_back();
    loop_exits_.pop_back();

    last_point_ = loop_exit;
}

void PoloniusFacts::visit_for(const parser::ForExpr& for_expr) {
    visit_expr(*for_expr.iter);

    auto loop_header = create_point(for_expr.body->span);
    if (last_point_ != INVALID_POINT) {
        emit_cfg_edge(last_point_, loop_header);
    }
    last_point_ = loop_header;

    auto loop_exit = create_point(for_expr.body->span);

    loop_headers_.push_back(loop_header);
    loop_exits_.push_back(loop_exit);

    env_.push_scope();

    // Bind the loop variable
    if (for_expr.pattern->template is<parser::IdentPattern>()) {
        const auto& ident = for_expr.pattern->template as<parser::IdentPattern>();
        auto loc = Location{current_stmt_, for_expr.pattern->span};
        env_.define(ident.name, nullptr, ident.is_mut, loc);
    }

    visit_expr(*for_expr.body);
    env_.pop_scope();

    // Back edge
    if (last_point_ != INVALID_POINT) {
        emit_cfg_edge(last_point_, loop_header);
    }
    // Exit edge (loop may not execute)
    emit_cfg_edge(loop_header, loop_exit);

    loop_headers_.pop_back();
    loop_exits_.pop_back();

    last_point_ = loop_exit;
}

void PoloniusFacts::visit_return(const parser::ReturnExpr& ret) {
    if (ret.value) {
        visit_expr(**ret.value);
    }
    // Return jumps to exit point
    if (last_point_ != INVALID_POINT) {
        emit_cfg_edge(last_point_, exit_point_);
    }
    // After a return, subsequent code is unreachable (no last_point_)
    last_point_ = INVALID_POINT;
}

void PoloniusFacts::visit_break(const parser::BreakExpr& brk) {
    if (brk.value) {
        visit_expr(**brk.value);
    }
    // Break jumps to loop exit
    if (!loop_exits_.empty() && last_point_ != INVALID_POINT) {
        emit_cfg_edge(last_point_, loop_exits_.back());
    }
    last_point_ = INVALID_POINT;
}

void PoloniusFacts::visit_closure(const parser::ClosureExpr& closure) {
    // Closures are checked separately — just note captures
    if (closure.body) {
        // Don't traverse into the closure body for the parent function's facts
        // Closure captures create subset constraints but the body is independent
    }
}

void PoloniusFacts::visit_struct_expr(const parser::StructExpr& struct_expr) {
    for (const auto& field : struct_expr.fields) {
        visit_expr(*field.second);
    }
}

void PoloniusFacts::visit_tuple(const parser::TupleExpr& tuple) {
    for (const auto& elem : tuple.elements) {
        visit_expr(*elem);
    }
}

void PoloniusFacts::visit_array(const parser::ArrayExpr& array) {
    std::visit(
        [this](const auto& k) {
            using T = std::decay_t<decltype(k)>;
            if constexpr (std::is_same_v<T, std::vector<parser::ExprPtr>>) {
                for (const auto& elem : k) {
                    visit_expr(*elem);
                }
            } else if constexpr (std::is_same_v<T, std::pair<parser::ExprPtr, parser::ExprPtr>>) {
                visit_expr(*k.first);
                visit_expr(*k.second);
            }
        },
        array.kind);
}

// ============================================================================
// Liveness computation
// ============================================================================

void PoloniusFacts::compute_liveness() {
    // For each origin, compute the set of points where it is live.
    // An origin is live at a point if the reference holding it might still
    // be used at or after that point.
    //
    // Algorithm: backward dataflow analysis
    // 1. For each origin, find all points where the reference is used
    // 2. Propagate liveness backward through CFG edges
    // 3. An origin is live from its definition through its last use

    // Build reverse CFG
    std::unordered_map<PointId, std::vector<PointId>> cfg_preds;
    for (const auto& edge : facts_.cfg_edges) {
        cfg_preds[edge.to].push_back(edge.from);
    }

    // For each origin, compute use points from the place's last_use
    for (const auto& [place_id, origin_id] : place_origins_) {
        auto it = facts_.origins.find(origin_id);
        if (it == facts_.origins.end())
            continue;

        // Find the definition point and last use point for this place
        const auto& state = env_.get_state(place_id);
        size_t def_stmt = state.definition.statement_index;
        size_t last_use_stmt = state.last_use ? state.last_use->statement_index : def_stmt;

        // Mark origin as live at all points between definition and last use
        for (const auto& [point_id, point] : facts_.points) {
            if (point.stmt_index >= def_stmt && point.stmt_index <= last_use_stmt) {
                facts_.origin_live_at.push_back({origin_id, point_id});
            }
        }
    }
}

} // namespace tml::borrow::polonius
