#include "tml/borrow/checker.hpp"

namespace tml::borrow {

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
        if (binary.left->template is<parser::IdentExpr>()) {
            const auto& ident = binary.left->template as<parser::IdentExpr>();
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
        if (unary.operand->template is<parser::IdentExpr>()) {
            const auto& ident = unary.operand->template as<parser::IdentExpr>();
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
    // Two-phase borrow: When calling a method that takes &mut self,
    // we need to allow the receiver to be borrowed while evaluating args.
    // Example: vec.push(vec.len()) - vec is borrowed for push, then for len
    begin_two_phase_borrow();
    check_expr(*call.receiver);

    for (const auto& arg : call.args) {
        check_expr(*arg);
    }
    end_two_phase_borrow();
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
        if (arm.pattern->template is<parser::IdentPattern>()) {
            const auto& ident = arm.pattern->template as<parser::IdentPattern>();
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
    if (for_expr.pattern->template is<parser::IdentPattern>()) {
        const auto& ident = for_expr.pattern->template as<parser::IdentPattern>();
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

        if (pattern->template is<parser::IdentPattern>()) {
            const auto& ident = pattern->template as<parser::IdentPattern>();
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


} // namespace tml::borrow
