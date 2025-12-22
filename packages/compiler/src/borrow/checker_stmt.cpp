#include "tml/borrow/checker.hpp"

namespace tml::borrow {

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


} // namespace tml::borrow
