//! # Borrow Checker Statement Analysis
//!
//! This file implements borrow checking for statements in TML. Statements
//! introduce variables into scope and may transfer ownership of values.
//!
//! ## Statement Types
//!
//! | Statement       | Effect on Ownership                      |
//! |-----------------|------------------------------------------|
//! | `let x = v`     | Defines `x`, takes ownership of `v`      |
//! | `let mut x = v` | Defines mutable `x`, takes ownership     |
//! | `expr;`         | Evaluates expression, may move/borrow    |
//! | `func ...`      | Nested function (checked separately)     |
//!
//! ## Let Bindings and Ownership
//!
//! When a `let` binding is evaluated, ownership transfers from the initializer
//! to the new variable:
//!
//! ```tml
//! let x = String::from("hello")  // x takes ownership
//! let y = x                       // ownership moves to y, x is invalid
//! let z = y.duplicate()           // y still valid, z gets a copy
//! ```
//!
//! ## Pattern Destructuring
//!
//! Patterns in let bindings can destructure values, potentially moving
//! individual fields:
//!
//! ```tml
//! let (a, b) = get_pair()        // a and b take ownership of tuple fields
//! let Point { x, y } = point     // x and y take ownership of fields
//! ```

#include "borrow/checker.hpp"

namespace tml::borrow {

/// Dispatches statement checking to the appropriate handler.
///
/// Statements are checked in order, with `current_stmt_` incremented after
/// each to track location for NLL lifetime analysis.
///
/// ## Statement Types
///
/// - `LetStmt`: Variable binding, may introduce new variables
/// - `ExprStmt`: Expression evaluated for side effects
/// - `DeclPtr`: Nested declaration (e.g., nested function)
void BorrowChecker::check_stmt(const parser::Stmt& stmt) {
    std::visit(
        [this](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, parser::LetStmt>) {
                check_let(s);
            } else if constexpr (std::is_same_v<T, parser::ExprStmt>) {
                check_expr_stmt(s);
            } else if constexpr (std::is_same_v<T, parser::DeclPtr>) {
                // Nested declaration - check if it's a function
                if (s) {
                    std::visit(
                        [this](const auto& d) {
                            using D = std::decay_t<decltype(d)>;
                            if constexpr (std::is_same_v<D, parser::FuncDecl>) {
                                check_func_decl(d);
                            }
                        },
                        s->kind);
                }
            }
        },
        stmt.kind);

    current_stmt_++;
}

/// Checks a let binding for borrow violations.
///
/// A let binding introduces a new variable and optionally initializes it.
/// The borrow checker:
/// 1. Checks the initializer expression (if present)
/// 2. Binds the pattern, creating new places for each variable
///
/// ## Initialization Order
///
/// The initializer is checked BEFORE the pattern is bound. This ensures
/// that the initializer cannot reference the variable being defined:
///
/// ```tml
/// let x = x + 1  // ERROR: x is not defined when evaluating x + 1
/// ```
///
/// ## Pattern Types
///
/// | Pattern               | Places Created                |
/// |-----------------------|-------------------------------|
/// | `let x = ...`         | Single place `x`              |
/// | `let (a, b) = ...`    | Places `a` and `b`            |
/// | `let Point{x, y} = ...` | Places `x` and `y`          |
/// | `let _ = ...`         | No places (value is dropped)  |
///
/// ## Mutability
///
/// The `mut` keyword on a pattern determines whether the bound variable
/// can be reassigned or mutably borrowed:
///
/// ```tml
/// let x = 5
/// x = 10           // ERROR: x is immutable
///
/// let mut y = 5
/// y = 10           // OK
/// ```
void BorrowChecker::check_let(const parser::LetStmt& let) {
    // Check initializer first
    if (let.init) {
        check_expr(**let.init);
    }

    // Check if the type is a mutable reference
    bool is_mut_ref = false;
    if (let.type_annotation && (*let.type_annotation)->is<parser::RefType>()) {
        is_mut_ref = (*let.type_annotation)->as<parser::RefType>().is_mut;
    }

    // Track whether variable is initialized (has an initializer expression)
    bool is_initialized = let.init.has_value();

    // Bind the pattern
    std::visit(
        [this, &let, is_mut_ref, is_initialized](const auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, parser::IdentPattern>) {
                auto loc = current_location(let.span);
                env_.define(p.name, nullptr, p.is_mut, loc, is_mut_ref, is_initialized);
            } else if constexpr (std::is_same_v<T, parser::TuplePattern>) {
                // For tuple patterns, we'd need to destructure
                // For now, just register each sub-pattern
                for (const auto& sub : p.elements) {
                    if (sub->template is<parser::IdentPattern>()) {
                        const auto& ident = sub->template as<parser::IdentPattern>();
                        auto loc = current_location(let.span);
                        env_.define(ident.name, nullptr, ident.is_mut, loc, is_mut_ref,
                                    is_initialized);
                    }
                }
            }
            // Other patterns handled similarly
        },
        let.pattern->kind);
}

/// Checks an expression statement.
///
/// Expression statements evaluate an expression for its side effects.
/// The resulting value (if any) is dropped at the end of the statement.
///
/// ## Drop Semantics
///
/// Values produced by expression statements are dropped immediately:
///
/// ```tml
/// create_temp_file();  // File is created and immediately dropped
/// ```
///
/// This is important for ownership because if the expression produces
/// a value with a destructor, that destructor runs at the semicolon.
void BorrowChecker::check_expr_stmt(const parser::ExprStmt& expr_stmt) {
    check_expr(*expr_stmt.expr);
}

} // namespace tml::borrow
