TML_MODULE("compiler")

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
#include "types/env.hpp"

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
            } else if constexpr (std::is_same_v<T, parser::LetElseStmt>) {
                check_let_else(s);
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
    // phase26e 1.2: clear any stale interior-ref record from a prior statement so
    // only THIS let's initializer can leave one behind (an initializer that is not
    // a ref-returning method call must not inherit a previous one).
    pending_ref_return_.reset();

    // Check initializer first
    if (let.init) {
        check_expr(**let.init);
        // phase26f 1.1: record the move performed by the initializer BEFORE the
        // pattern is bound, so that identifiers in the initializer still resolve
        // to the outer scope (correct for shadowing: `let x = x` moves the outer
        // `x`, not the freshly-bound one). `let y = x` moves x when x has move
        // semantics; `let leaf = base.field` partially moves base.field. Borrows
        // (`ref x`), method-call results (`x.duplicate()`) and copies are no-ops.
        move_if_owned_ident(**let.init);
        move_if_owned_projection(**let.init);
    }

    // Check if the type is a mutable reference
    bool is_mut_ref = false;
    if (let.type_annotation && (*let.type_annotation)->is<parser::RefType>()) {
        is_mut_ref = (*let.type_annotation)->as<parser::RefType>().is_mut;
    }

    // Track whether variable is initialized (has an initializer expression)
    bool is_initialized = let.init.has_value();

    // Bind the pattern. phase26f: thread each binding's resolved type into the
    // place so move-vs-copy classification works. The type checker recorded it
    // against the IdentPattern node in bind_pattern (see types/checker/stmt.cpp);
    // read it back via get_expr_type keyed on the same cached AST node.
    std::visit(
        [this, &let, is_mut_ref, is_initialized](const auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, parser::IdentPattern>) {
                auto loc = current_location(let.span);
                types::TypePtr bound_type =
                    type_env_ ? type_env_->get_expr_type(&p) : nullptr;
                env_.define(p.name, bound_type, p.is_mut, loc, is_mut_ref, is_initialized);
            } else if constexpr (std::is_same_v<T, parser::TuplePattern>) {
                // For tuple patterns, we'd need to destructure
                // For now, just register each sub-pattern
                for (const auto& sub : p.elements) {
                    if (sub->template is<parser::IdentPattern>()) {
                        const auto& ident = sub->template as<parser::IdentPattern>();
                        auto loc = current_location(let.span);
                        types::TypePtr bound_type =
                            type_env_ ? type_env_->get_expr_type(&ident) : nullptr;
                        env_.define(ident.name, bound_type, ident.is_mut, loc, is_mut_ref,
                                    is_initialized);
                    }
                }
            }
            // Other patterns handled similarly
        },
        let.pattern->kind);

    // phase26e 1.2: if the initializer was a CONFIDENTLY-resolved ref-returning
    // method call over a place (e.g. `let r = c.get_ref(i)`), bind a borrow of
    // the receiver to this LHS ref. NLL then keeps the receiver borrow live until
    // the ref's last use, so a mutating call on the receiver while `r` is still
    // live (`c.push(x); use(*r)`) is a B009 conflict. Only simple `let r = ...`
    // ident bindings participate — destructuring an interior ref is not a shape
    // we model, and skipping it is the safe (under-borrow) direction.
    // phase26g 1.4: bind the interior-ref borrow to the LHS ONLY when the
    // initializer is DIRECTLY the ref-returning method call (`let r =
    // c.get_ref(i)`). If the ref is consumed by a wrapping expression before
    // binding — e.g. `let first: I64 = *c.get_ref(i)` (deref to a value) — the
    // ref does not escape into `first`, so no borrow of the receiver survives.
    // Without this guard the stale `pending_ref_return_` (set by the inner
    // get_ref call, never cleared by the deref) was bound to the value LHS,
    // keeping a phantom borrow live and producing a false B009 on a later
    // mutation. Under-binding on exotic shapes (block/if-expr yielding a ref)
    // is the safe (under-borrow) direction already adopted by this wiring.
    bool init_is_direct_call =
        let.init && (*let.init)->is<parser::MethodCallExpr>();
    if (pending_ref_return_ && init_is_direct_call &&
        let.pattern->is<parser::IdentPattern>()) {
        const auto& ident = let.pattern->as<parser::IdentPattern>();
        auto lhs_place = env_.lookup(ident.name);
        if (lhs_place) {
            auto loc = current_location(let.span);
            create_borrow_with_projection(pending_ref_return_->place,
                                          pending_ref_return_->full_place,
                                          pending_ref_return_->kind, loc, *lhs_place);
        }
    }
    pending_ref_return_.reset();
}

/// Checks a `let Pattern = init else { diverge }` binding.
///
/// phase26g 1.2: check_stmt previously did NOT visit LetElseStmt at all, so its
/// initializer, else block, and pattern were entirely unchecked. This handler
/// checks the initializer and else block, and — when the initializer is directly
/// a method call returning an interior reference (`ref T` or `Maybe[ref V]` from
/// get_ref/get_mut over a place receiver) matched by `Just(r)` — ties the
/// receiver borrow to the payload `r`, so a mutation of the receiver while `r` is
/// still live is a B009 conflict. Minimal by design: no move recording is added
/// for let-else (staying at the prior no-op status quo for that), and only the
/// confident interior-ref shape creates a borrow.
void BorrowChecker::check_let_else(const parser::LetElseStmt& let_else) {
    pending_ref_return_.reset();

    // Initializer: also lets check_method_call record an interior-ref receiver
    // borrow for a `Maybe[ref V]` / `ref T` accessor result.
    check_expr(*let_else.init);
    std::optional<PendingRefReturn> init_ref;
    if (let_else.init->is<parser::MethodCallExpr>() && pending_ref_return_) {
        init_ref = pending_ref_return_;
    }
    pending_ref_return_.reset();

    // The else block diverges (return/panic/break/continue); its bindings never
    // escape, so check it in a throwaway scope.
    env_.push_scope();
    check_expr(*let_else.else_block);
    drop_scope_places();
    env_.pop_scope();

    // Bind the interior-ref payload into the CONTINUATION scope and link the
    // receiver borrow. Non-ref results are left to existing machinery (the prior
    // behaviour bound nothing here — under-borrow safe).
    if (init_ref) {
        bind_interior_ref_payload(*let_else.pattern, *init_ref);
    }
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
