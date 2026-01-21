//! # Borrow Checker Expression Analysis
//!
//! This file implements borrow checking for all expression types in TML.
//! Each expression type has specific rules about ownership and borrowing.
//!
//! ## Expression Categories
//!
//! | Category      | Expressions                      | Borrow Rules                    |
//! |---------------|----------------------------------|--------------------------------|
//! | Values        | Literals, Tuples, Arrays         | No borrows, creates owned value |
//! | Variables     | Identifiers                      | Use requires owned/borrowed     |
//! | Operations    | Binary, Unary                    | Operates on values, ref creates borrow |
//! | Calls         | Call, MethodCall                 | Arguments may be moved/borrowed |
//! | Access        | Field, Index                     | May borrow or move sub-parts   |
//! | Control       | Block, If, When, Loop, For       | Creates scopes for borrows     |
//! | Transfer      | Return, Break                    | Checks for dangling refs       |
//!
//! ## Two-Phase Borrows
//!
//! Method calls use two-phase borrowing to handle cases like `vec.push(vec.len())`:
//!
//! 1. **Reservation phase**: Mutable borrow is "reserved" but not activated
//! 2. **Argument evaluation**: Arguments can borrow the receiver immutably
//! 3. **Activation phase**: Mutable borrow activates when method executes
//!
//! This allows:
//! ```tml
//! let mut v = vec![1, 2, 3]
//! v.push(v.len())  // v.len() borrows immutably during reservation
//! ```

#include "borrow/checker.hpp"

namespace tml::borrow {

/// Dispatches expression checking to the appropriate handler.
///
/// Uses `std::visit` to match on the expression variant and call the
/// corresponding `check_*` method.
void BorrowChecker::check_expr(const parser::Expr& expr) {
    std::visit(
        [this, &expr](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::LiteralExpr>) {
                // Literals don't involve borrowing
            } else if constexpr (std::is_same_v<T, parser::IdentExpr>) {
                check_ident(e, expr.span);
            } else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
                check_binary(e);
            } else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
                check_unary(e);
            } else if constexpr (std::is_same_v<T, parser::CallExpr>) {
                check_call(e);
            } else if constexpr (std::is_same_v<T, parser::MethodCallExpr>) {
                check_method_call(e);
            } else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
                check_field_access(e);
            } else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
                check_index(e);
            } else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
                check_block(e);
            } else if constexpr (std::is_same_v<T, parser::IfExpr>) {
                check_if(e);
            } else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
                check_when(e);
            } else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
                check_loop(e);
            } else if constexpr (std::is_same_v<T, parser::ForExpr>) {
                check_for(e);
            } else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
                check_return(e);
            } else if constexpr (std::is_same_v<T, parser::BreakExpr>) {
                check_break(e);
            } else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
                check_tuple(e);
            } else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
                check_array(e);
            } else if constexpr (std::is_same_v<T, parser::StructExpr>) {
                check_struct_expr(e);
            } else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
                check_closure(e);
            }
            // Other expressions handled as needed
        },
        expr.kind);
}

/// Checks an identifier expression (variable use).
///
/// When a variable is used, we must verify:
/// 1. The variable hasn't been moved (unless it's Copy)
/// 2. The variable isn't mutably borrowed by someone else
/// 3. The variable is initialized
///
/// ## NLL Integration
///
/// Before checking, we apply NLL to release any dead borrows. After checking,
/// we update the `last_use` for NLL tracking.
///
/// ## Example Violations
///
/// ```tml
/// let s = String::new()
/// let t = s           // s is moved
/// println(s)          // ERROR: use of moved value 's'
///
/// let mut x = 42
/// let r = mut ref x   // x is mutably borrowed
/// println(x)          // ERROR: cannot use 'x' while mutably borrowed
/// ```
void BorrowChecker::check_ident(const parser::IdentExpr& ident, SourceSpan span) {
    auto place_id = env_.lookup(ident.name);
    if (!place_id) {
        // Variable not found - might be a function call, let type checker handle it
        return;
    }

    auto loc = current_location(span);

    // NLL: Apply dead borrow release before checking usage
    apply_nll(loc);

    check_can_use(*place_id, loc);
    env_.mark_used(*place_id, loc);

    // NLL: If this place holds a reference, update the borrow's last_use
    if (ref_to_borrowed_.count(*place_id)) {
        env_.mark_ref_used(*place_id, loc);
    }
}

/// Checks a binary expression for borrow violations.
///
/// Binary expressions evaluate both operands and may perform assignment.
/// Assignment operators (`=`, `+=`, etc.) require the LHS to be mutable.
///
/// ## Assignment Rules
///
/// - LHS must be a mutable place (`let mut x`)
/// - LHS must not be currently borrowed
/// - RHS is evaluated before assignment
///
/// ## Example
///
/// ```tml
/// let x = 5
/// x = 10        // ERROR: cannot assign to 'x' (not mutable)
///
/// let mut y = 5
/// let r = ref y
/// y = 10        // ERROR: cannot assign to 'y' while borrowed
/// ```
void BorrowChecker::check_binary(const parser::BinaryExpr& binary) {
    check_expr(*binary.left);
    check_expr(*binary.right);

    // Assignment operators require mutable access to LHS
    if (binary.op == parser::BinaryOp::Assign || binary.op == parser::BinaryOp::AddAssign ||
        binary.op == parser::BinaryOp::SubAssign || binary.op == parser::BinaryOp::MulAssign ||
        binary.op == parser::BinaryOp::DivAssign || binary.op == parser::BinaryOp::ModAssign) {

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

/// Checks a unary expression for borrow violations.
///
/// The key unary operators for borrowing are:
/// - `ref` (`&`): Creates an immutable (shared) borrow
/// - `mut ref` (`&mut`): Creates a mutable (exclusive) borrow
/// - `*` (deref): Accesses through a reference
///
/// ## Borrow Creation Rules
///
/// | Operation     | Requirement                              |
/// |---------------|------------------------------------------|
/// | `ref x`       | x must be usable (not moved/mutably borrowed) |
/// | `mut ref x`   | x must be mutable and not borrowed       |
///
/// ## Projection-Aware Borrowing
///
/// When borrowing a field (e.g., `ref x.field`), only that field is borrowed.
/// Other fields of `x` can still be borrowed:
///
/// ```tml
/// let mut p = Point { x: 1, y: 2 }
/// let rx = ref p.x
/// let ry = ref p.y   // OK! Different fields
/// ```
void BorrowChecker::check_unary(const parser::UnaryExpr& unary) {
    check_expr(*unary.operand);

    // Ref and RefMut create borrows
    if (unary.op == parser::UnaryOp::Ref || unary.op == parser::UnaryOp::RefMut) {
        auto loc = current_location(unary.span);

        // NLL: Apply dead borrow release before creating new borrow
        apply_nll(loc);

        // Extract full place with projections
        auto full_place = extract_place(*unary.operand);

        if (full_place) {
            auto kind =
                (unary.op == parser::UnaryOp::RefMut) ? BorrowKind::Mutable : BorrowKind::Shared;

            // Check if this is a reborrow pattern: ref *r or mut ref *r
            // where r is a reference - the place will have a Deref projection
            bool is_reborrow = false;
            [[maybe_unused]] PlaceId source_ref_place = 0;

            if (!full_place->projections.empty() &&
                full_place->projections.back().kind == ProjectionKind::Deref) {
                // This is a reborrow: we're taking a reference to a dereferenced reference
                // e.g., let r2 = ref *r1
                // The source reference is the base place before the deref
                const auto& base_state = env_.get_state(full_place->base);
                if (base_state.borrowed_from.has_value() ||
                    base_state.type) { // Check if base is a reference type
                    is_reborrow = true;
                    source_ref_place = full_place->base;
                }
            }

            // Use projection-aware borrow checking
            check_can_borrow_with_projection(full_place->base, *full_place, kind, loc);

            if (is_reborrow) {
                // This is a reborrow - use specialized tracking
                // Note: create_reborrow will be called when the let binding assigns to a variable
                // For now, just create the borrow with reborrow_origin tracking
                create_borrow_with_projection(full_place->base, *full_place, kind, loc, 0);
            } else {
                create_borrow_with_projection(full_place->base, *full_place, kind, loc, 0);
            }
        } else if (unary.operand->template is<parser::IdentExpr>()) {
            // Fallback for simple identifiers
            const auto& ident = unary.operand->template as<parser::IdentExpr>();
            auto place_id = env_.lookup(ident.name);
            if (place_id) {
                auto kind = (unary.op == parser::UnaryOp::RefMut) ? BorrowKind::Mutable
                                                                  : BorrowKind::Shared;
                check_can_borrow(*place_id, kind, loc);
                create_borrow(*place_id, kind, loc);
            }
        }
    }
}

/// Checks a function call for borrow violations.
///
/// Arguments are checked in order. Each argument may move or borrow a value.
/// The callee is also checked (it might be a variable holding a function).
void BorrowChecker::check_call(const parser::CallExpr& call) {
    check_expr(*call.callee);
    for (const auto& arg : call.args) {
        check_expr(*arg);
    }
}

/// Checks a method call with two-phase borrow support.
///
/// Method calls are special because the receiver might be mutably borrowed
/// for the method, but arguments might also need to borrow the receiver.
///
/// ## Two-Phase Borrow
///
/// Consider: `vec.push(vec.len())`
///
/// Without two-phase borrowing:
/// 1. `vec` is mutably borrowed for `push`
/// 2. `vec.len()` tries to borrow `vec` immutably → ERROR
///
/// With two-phase borrowing:
/// 1. Mutable borrow is "reserved" (not yet active)
/// 2. `vec.len()` borrows immutably → OK
/// 3. Mutable borrow activates for `push` → OK
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

/// Checks field access for partial move violations.
///
/// When accessing a field, we check if that specific field has been moved.
/// Other fields of the struct may still be accessible.
///
/// ## Example
///
/// ```tml
/// type Pair { a: String, b: String }
///
/// let p = Pair { a: "hello", b: "world" }
/// let s = p.a      // p.a is moved
/// println(p.a)     // ERROR: p.a was moved
/// println(p.b)     // OK: p.b is still valid
/// ```
void BorrowChecker::check_field_access(const parser::FieldExpr& field_expr) {
    check_expr(*field_expr.object);

    // Check if accessing a partially moved field
    auto base_place = extract_place(*field_expr.object);
    if (base_place && base_place->projections.empty()) {
        // Simple base (e.g., x.field where x is a variable)
        auto loc = current_location(field_expr.span);
        check_can_use_field(base_place->base, field_expr.field, loc);
    }
}

/// Checks index expression (array/slice access).
///
/// Both the object and index expressions are checked for borrow violations.
void BorrowChecker::check_index(const parser::IndexExpr& idx) {
    check_expr(*idx.object);
    check_expr(*idx.index);
}

/// Checks a block expression, creating a new scope.
///
/// Blocks introduce a new scope for variables. All variables defined in the
/// block are dropped when the block ends, releasing any borrows.
///
/// ## Scope Lifecycle
///
/// 1. `push_scope()`: Create new scope
/// 2. Check statements: Variables defined here belong to this scope
/// 3. Check final expression: May use scope's variables
/// 4. `drop_scope_places()`: Release borrows, run destructors
/// 5. `pop_scope()`: Remove scope
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

/// Checks an if expression.
///
/// Both branches are checked independently. Variables defined in one branch
/// are not visible in the other.
void BorrowChecker::check_if(const parser::IfExpr& if_expr) {
    check_expr(*if_expr.condition);
    check_expr(*if_expr.then_branch);

    if (if_expr.else_branch) {
        check_expr(**if_expr.else_branch);
    }
}

/// Checks a when (match) expression.
///
/// Each arm creates a new scope for pattern bindings. The scrutinee may be
/// moved or borrowed depending on the pattern.
///
/// ## Pattern Binding
///
/// ```tml
/// when value {
///     Just(x) => use(x),   // x is bound in this arm's scope
///     Nothing => {},
/// }
/// ```
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

/// Checks a loop expression.
///
/// Loop bodies create a scope that is entered repeatedly. The loop depth
/// is tracked for break/continue analysis.
void BorrowChecker::check_loop(const parser::LoopExpr& loop) {
    loop_depth_++;
    env_.push_scope();

    check_expr(*loop.body);

    drop_scope_places();
    env_.pop_scope();
    loop_depth_--;
}

/// Checks a for expression.
///
/// The iterator expression is checked first, then the loop body is checked
/// with the loop variable bound.
///
/// ## Loop Variable
///
/// The loop variable is defined fresh for each iteration:
///
/// ```tml
/// for item in items {
///     // 'item' is valid here, owned for this iteration
/// }
/// // 'item' is out of scope
/// ```
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

/// Checks a return expression for dangling references.
///
/// Returning a reference to a local variable is an error because the local
/// will be dropped when the function returns, leaving a dangling reference.
///
/// ## Example Violation
///
/// ```tml
/// func bad() -> ref I32 {
///     let x = 42
///     return ref x    // ERROR: returns reference to local 'x'
/// }
/// ```
void BorrowChecker::check_return(const parser::ReturnExpr& ret) {
    if (ret.value) {
        check_expr(**ret.value);
    }

    // NLL: Check for dangling references
    check_return_borrows(ret);
}

/// Checks a break expression.
///
/// If the break has a value, that value is checked for borrow violations.
void BorrowChecker::check_break(const parser::BreakExpr& brk) {
    if (brk.value) {
        check_expr(**brk.value);
    }
}

/// Checks a tuple expression.
///
/// Each element is checked in order.
void BorrowChecker::check_tuple(const parser::TupleExpr& tuple) {
    for (const auto& elem : tuple.elements) {
        check_expr(*elem);
    }
}

/// Checks an array expression.
///
/// Arrays can be either:
/// - Literal: `[1, 2, 3]` - each element checked
/// - Repeat: `[0; 100]` - element and count checked
void BorrowChecker::check_array(const parser::ArrayExpr& array) {
    std::visit(
        [this](const auto& a) {
            using T = std::decay_t<decltype(a)>;
            if constexpr (std::is_same_v<T, std::vector<parser::ExprPtr>>) {
                // Array literal: [1, 2, 3]
                for (const auto& elem : a) {
                    check_expr(*elem);
                }
            } else if constexpr (std::is_same_v<T, std::pair<parser::ExprPtr, parser::ExprPtr>>) {
                // Array repeat: [expr; count]
                check_expr(*a.first);
                check_expr(*a.second);
            }
        },
        array.kind);
}

/// Checks a struct instantiation expression.
///
/// Each field value is checked. If a base struct is provided for update syntax,
/// it is also checked.
///
/// ## Struct Update Syntax
///
/// ```tml
/// let p2 = Point { x: 10, ..p1 }
/// // p1.y is moved to p2.y (if not Copy)
/// ```
void BorrowChecker::check_struct_expr(const parser::StructExpr& struct_expr) {
    for (const auto& [name, value] : struct_expr.fields) {
        check_expr(*value);
    }

    if (struct_expr.base) {
        check_expr(**struct_expr.base);
    }
}

/// Checks a closure expression.
///
/// Closures create a new scope for their parameters. The body is checked
/// within this scope.
///
/// ## Closure Captures
///
/// Closures may capture variables from their environment. The borrow checker
/// determines whether captures are by reference or by move based on usage.
///
/// ```tml
/// let x = String::new()
/// let f = do() { println(x) }  // x is borrowed
/// let g = do() { drop(x) }     // x is moved into g
/// ```
void BorrowChecker::check_closure(const parser::ClosureExpr& closure) {
    // Analyze captures before entering closure scope
    auto captures = analyze_captures(closure);

    // Validate captures against current borrow state
    validate_captures(captures, closure);

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

// ============================================================================
// Closure Capture Analysis
// ============================================================================

auto BorrowChecker::analyze_captures(const parser::ClosureExpr& closure)
    -> std::vector<CaptureInfo> {
    std::vector<CaptureInfo> result;

    // Collect parameter names as local variables
    std::unordered_set<std::string> local_vars;
    for (const auto& [pattern, type] : closure.params) {
        if (pattern->template is<parser::IdentPattern>()) {
            const auto& ident = pattern->template as<parser::IdentPattern>();
            local_vars.insert(ident.name);
        }
    }

    // Collect free variables and their usage from the closure body
    std::unordered_map<std::string, CaptureKind> captures;
    std::unordered_map<std::string, SourceSpan> capture_spans;
    collect_free_variables(*closure.body, local_vars, captures, capture_spans);

    // Build CaptureInfo for each captured variable
    for (const auto& [var_name, kind] : captures) {
        auto place_id_opt = env_.lookup(var_name);
        if (!place_id_opt) {
            continue; // Variable not found in outer scope - might be global or function
        }

        CaptureKind final_kind = kind;

        // If this is a move closure, force all captures to be by move
        if (closure.is_move && kind != CaptureKind::ByCopy) {
            final_kind = CaptureKind::ByMove;
        }

        result.push_back(CaptureInfo{
            .name = var_name,
            .place_id = *place_id_opt,
            .kind = final_kind,
            .capture_span = capture_spans[var_name],
            .forced_move = closure.is_move,
        });
    }

    return result;
}

/// Helper to extract all variable bindings from a pattern.
/// Used to track pattern-bound variables as locals in closure capture analysis.
static void collect_pattern_bindings(const parser::Pattern& pattern,
                                     std::unordered_set<std::string>& bindings) {
    std::visit(
        [&bindings](const auto& p) {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, parser::IdentPattern>) {
                bindings.insert(p.name);
            } else if constexpr (std::is_same_v<T, parser::TuplePattern>) {
                for (const auto& elem : p.elements) {
                    collect_pattern_bindings(*elem, bindings);
                }
            } else if constexpr (std::is_same_v<T, parser::StructPattern>) {
                for (const auto& [name, sub_pattern] : p.fields) {
                    collect_pattern_bindings(*sub_pattern, bindings);
                }
            } else if constexpr (std::is_same_v<T, parser::EnumPattern>) {
                if (p.payload) {
                    for (const auto& sub_pattern : *p.payload) {
                        collect_pattern_bindings(*sub_pattern, bindings);
                    }
                }
            } else if constexpr (std::is_same_v<T, parser::OrPattern>) {
                // Or-patterns must bind the same variables in each alternative
                // Just collect from the first one (they should all be the same)
                if (!p.patterns.empty()) {
                    collect_pattern_bindings(*p.patterns[0], bindings);
                }
            } else if constexpr (std::is_same_v<T, parser::ArrayPattern>) {
                for (const auto& elem : p.elements) {
                    collect_pattern_bindings(*elem, bindings);
                }
                if (p.rest) {
                    collect_pattern_bindings(**p.rest, bindings);
                }
            }
            // WildcardPattern, LiteralPattern, RangePattern don't bind variables
        },
        pattern.kind);
}

void BorrowChecker::collect_free_variables(
    const parser::Expr& expr, const std::unordered_set<std::string>& local_vars,
    std::unordered_map<std::string, CaptureKind>& captures,
    std::unordered_map<std::string, SourceSpan>& capture_spans) {

    // Helper to update capture kind (most restrictive wins)
    auto update_capture = [&](const std::string& name, CaptureKind kind, SourceSpan span) {
        if (local_vars.count(name) > 0) {
            return; // Not a capture - it's a local variable
        }

        auto it = captures.find(name);
        if (it == captures.end()) {
            captures[name] = kind;
            capture_spans[name] = span;
        } else {
            // Most restrictive kind wins: ByMove > ByMutRef > ByRef > ByCopy
            if (kind == CaptureKind::ByMove ||
                (kind == CaptureKind::ByMutRef && it->second != CaptureKind::ByMove)) {
                it->second = kind;
            }
        }
    };

    // Visit expression recursively
    std::visit(
        [&](const auto& e) {
            using T = std::decay_t<decltype(e)>;

            if constexpr (std::is_same_v<T, parser::IdentExpr>) {
                // Simple identifier - captured by reference
                update_capture(e.name, CaptureKind::ByRef, e.span);
            } else if constexpr (std::is_same_v<T, parser::BinaryExpr>) {
                // Check for assignment operators
                if (e.op == parser::BinaryOp::Assign || e.op == parser::BinaryOp::AddAssign ||
                    e.op == parser::BinaryOp::SubAssign || e.op == parser::BinaryOp::MulAssign ||
                    e.op == parser::BinaryOp::DivAssign || e.op == parser::BinaryOp::ModAssign ||
                    e.op == parser::BinaryOp::BitAndAssign ||
                    e.op == parser::BinaryOp::BitOrAssign ||
                    e.op == parser::BinaryOp::BitXorAssign || e.op == parser::BinaryOp::ShlAssign ||
                    e.op == parser::BinaryOp::ShrAssign) {
                    // Assignment target needs mutable capture
                    if (e.left->template is<parser::IdentExpr>()) {
                        const auto& ident = e.left->template as<parser::IdentExpr>();
                        update_capture(ident.name, CaptureKind::ByMutRef, ident.span);
                    }
                }
                collect_free_variables(*e.left, local_vars, captures, capture_spans);
                collect_free_variables(*e.right, local_vars, captures, capture_spans);
            } else if constexpr (std::is_same_v<T, parser::UnaryExpr>) {
                // Check for mutable reference
                if (e.op == parser::UnaryOp::RefMut &&
                    e.operand->template is<parser::IdentExpr>()) {
                    const auto& ident = e.operand->template as<parser::IdentExpr>();
                    update_capture(ident.name, CaptureKind::ByMutRef, ident.span);
                }
                collect_free_variables(*e.operand, local_vars, captures, capture_spans);
            } else if constexpr (std::is_same_v<T, parser::CallExpr>) {
                collect_free_variables(*e.callee, local_vars, captures, capture_spans);
                for (const auto& arg : e.args) {
                    collect_free_variables(*arg, local_vars, captures, capture_spans);
                }
            } else if constexpr (std::is_same_v<T, parser::MethodCallExpr>) {
                collect_free_variables(*e.receiver, local_vars, captures, capture_spans);
                for (const auto& arg : e.args) {
                    collect_free_variables(*arg, local_vars, captures, capture_spans);
                }
            } else if constexpr (std::is_same_v<T, parser::FieldExpr>) {
                collect_free_variables(*e.object, local_vars, captures, capture_spans);
            } else if constexpr (std::is_same_v<T, parser::IndexExpr>) {
                collect_free_variables(*e.object, local_vars, captures, capture_spans);
                collect_free_variables(*e.index, local_vars, captures, capture_spans);
            } else if constexpr (std::is_same_v<T, parser::BlockExpr>) {
                // Track new locals introduced in block
                std::unordered_set<std::string> block_locals = local_vars;
                for (const auto& stmt : e.stmts) {
                    if (stmt->template is<parser::LetStmt>()) {
                        const auto& let_stmt = stmt->template as<parser::LetStmt>();
                        if (let_stmt.pattern->template is<parser::IdentPattern>()) {
                            const auto& ident =
                                let_stmt.pattern->template as<parser::IdentPattern>();
                            block_locals.insert(ident.name);
                        }
                    }
                    // Check statement expression
                    if (stmt->template is<parser::ExprStmt>()) {
                        const auto& expr_stmt = stmt->template as<parser::ExprStmt>();
                        collect_free_variables(*expr_stmt.expr, block_locals, captures,
                                               capture_spans);
                    } else if (stmt->template is<parser::LetStmt>()) {
                        const auto& let_stmt = stmt->template as<parser::LetStmt>();
                        if (let_stmt.init) {
                            collect_free_variables(**let_stmt.init, block_locals, captures,
                                                   capture_spans);
                        }
                    }
                }
                if (e.expr) {
                    collect_free_variables(**e.expr, block_locals, captures, capture_spans);
                }
            } else if constexpr (std::is_same_v<T, parser::IfExpr>) {
                collect_free_variables(*e.condition, local_vars, captures, capture_spans);
                collect_free_variables(*e.then_branch, local_vars, captures, capture_spans);
                if (e.else_branch) {
                    collect_free_variables(**e.else_branch, local_vars, captures, capture_spans);
                }
            } else if constexpr (std::is_same_v<T, parser::LoopExpr>) {
                collect_free_variables(*e.body, local_vars, captures, capture_spans);
            } else if constexpr (std::is_same_v<T, parser::ForExpr>) {
                std::unordered_set<std::string> for_locals = local_vars;
                if (e.pattern->template is<parser::IdentPattern>()) {
                    const auto& ident = e.pattern->template as<parser::IdentPattern>();
                    for_locals.insert(ident.name);
                }
                collect_free_variables(*e.iter, local_vars, captures, capture_spans);
                collect_free_variables(*e.body, for_locals, captures, capture_spans);
            } else if constexpr (std::is_same_v<T, parser::ClosureExpr>) {
                // Nested closure - don't descend (it will have its own capture analysis)
            } else if constexpr (std::is_same_v<T, parser::TupleExpr>) {
                for (const auto& elem : e.elements) {
                    collect_free_variables(*elem, local_vars, captures, capture_spans);
                }
            } else if constexpr (std::is_same_v<T, parser::ArrayExpr>) {
                std::visit(
                    [&](auto&& arr_kind) {
                        using K = std::decay_t<decltype(arr_kind)>;
                        if constexpr (std::is_same_v<K, std::vector<parser::ExprPtr>>) {
                            for (const auto& elem : arr_kind) {
                                collect_free_variables(*elem, local_vars, captures, capture_spans);
                            }
                        } else if constexpr (std::is_same_v<
                                                 K, std::pair<parser::ExprPtr, parser::ExprPtr>>) {
                            collect_free_variables(*arr_kind.first, local_vars, captures,
                                                   capture_spans);
                            collect_free_variables(*arr_kind.second, local_vars, captures,
                                                   capture_spans);
                        }
                    },
                    e.kind);
            } else if constexpr (std::is_same_v<T, parser::StructExpr>) {
                for (const auto& field : e.fields) {
                    collect_free_variables(*field.second, local_vars, captures, capture_spans);
                }
            } else if constexpr (std::is_same_v<T, parser::ReturnExpr>) {
                if (e.value) {
                    collect_free_variables(**e.value, local_vars, captures, capture_spans);
                }
            } else if constexpr (std::is_same_v<T, parser::WhenExpr>) {
                collect_free_variables(*e.scrutinee, local_vars, captures, capture_spans);
                for (const auto& arm : e.arms) {
                    // Track pattern bindings as locals for this arm's scope
                    std::unordered_set<std::string> arm_locals = local_vars;
                    collect_pattern_bindings(*arm.pattern, arm_locals);
                    collect_free_variables(*arm.body, arm_locals, captures, capture_spans);
                }
            } else if constexpr (std::is_same_v<T, parser::CastExpr>) {
                collect_free_variables(*e.expr, local_vars, captures, capture_spans);
            }
            // Literals and other expressions don't capture anything
        },
        expr.kind);
}

auto BorrowChecker::determine_capture_kind([[maybe_unused]] const std::string& var_name,
                                           PlaceId place_id, bool is_mutated, bool is_moved)
    -> CaptureKind {
    // Check if the type is Copy
    const auto& state = env_.get_state(place_id);
    if (state.type && is_copy_type(state.type)) {
        return CaptureKind::ByCopy;
    }

    if (is_moved) {
        return CaptureKind::ByMove;
    }

    if (is_mutated) {
        return CaptureKind::ByMutRef;
    }

    return CaptureKind::ByRef;
}

void BorrowChecker::validate_captures(const std::vector<CaptureInfo>& captures,
                                      const parser::ClosureExpr& closure) {
    for (const auto& capture : captures) {
        const auto& state = env_.get_state(capture.place_id);

        // B014: Cannot capture a moved value
        if (state.state == OwnershipState::Moved) {
            SourceSpan move_span = state.move_location ? state.move_location->span : closure.span;
            errors_.push_back(
                BorrowError::closure_captures_moved(capture.name, capture.capture_span, move_span));
            continue;
        }

        // B015: Cannot capture by mut ref or move while value is borrowed
        if (capture.kind == CaptureKind::ByMutRef || capture.kind == CaptureKind::ByMove) {
            // Check for existing borrows
            if (!state.active_borrows.empty()) {
                const auto& first_borrow = state.active_borrows.front();
                errors_.push_back(BorrowError::closure_capture_conflict(
                    capture.name, capture.kind, capture.capture_span, first_borrow.start.span));
                continue;
            }
        }

        // B015: Cannot capture by ref while value is mutably borrowed
        if (capture.kind == CaptureKind::ByRef) {
            for (const auto& borrow : state.active_borrows) {
                if (borrow.kind == BorrowKind::Mutable) {
                    errors_.push_back(BorrowError::closure_capture_conflict(
                        capture.name, capture.kind, capture.capture_span, borrow.start.span));
                    break;
                }
            }
        }
    }
}

} // namespace tml::borrow
