TML_MODULE("compiler")

//! # HIR Builder - Statement Lowering
//!
//! This file implements statement lowering from AST to HIR.
//!
//! ## Overview
//!
//! Statement lowering transforms parser AST statements into HIR statements.
//! TML has a small set of statement types since most constructs are expressions.
//!
//! ## Statement Types
//!
//! | AST Statement | HIR Statement | Description                           |
//! |---------------|---------------|---------------------------------------|
//! | `LetStmt`     | `HirLetStmt`  | Immutable binding: `let x = 1`        |
//! | `VarStmt`     | `HirLetStmt`  | Mutable binding: `var x = 1` (sugar)  |
//! | `ExprStmt`    | `HirExprStmt` | Expression evaluated for side-effects |
//! | `DeclPtr`     | (placeholder) | Nested declarations in blocks         |
//!
//! ## Key Transformations
//!
//! - `var x = value` desugars to `let mut x = value`
//! - Pattern bindings add variables to the current scope
//! - Type inference from initializers when no annotation provided
//!
//! ## Scope Management
//!
//! Each `let`/`var` binding adds names to the current scope for variable
//! resolution. Block expressions create new scopes (handled in hir_builder_expr.cpp).
//!
//! ## See Also
//!
//! - `hir_builder.cpp` - Main builder and scope infrastructure
//! - `hir_builder_expr.cpp` - Expression lowering
//! - `hir_builder_pattern.cpp` - Pattern lowering for let bindings

#include "hir/hir_builder.hpp"

namespace tml::hir {

// ============================================================================
// Statement Lowering Dispatch
// ============================================================================
//
// Main entry point for statement lowering. Uses std::visit to dispatch
// to type-specific lowering functions based on the statement variant.
// Nested declarations (const, type aliases) become placeholder statements
// since they're already registered in the module's symbol tables.

auto HirBuilder::lower_stmt(const parser::Stmt& stmt) -> HirStmtPtr {
    return std::visit(
        [&](const auto& s) -> HirStmtPtr {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, parser::LetStmt>) {
                return lower_let(s);
            } else if constexpr (std::is_same_v<T, parser::VarStmt>) {
                return lower_var(s);
            } else if constexpr (std::is_same_v<T, parser::LetElseStmt>) {
                return lower_let_else(s);
            } else if constexpr (std::is_same_v<T, parser::ExprStmt>) {
                return lower_expr_stmt(s);
            } else if constexpr (std::is_same_v<T, parser::DeclPtr>) {
                // Handle nested declarations (const, func, type, etc.)
                return lower_nested_decl(*s, stmt.span);
            } else {
                // Fallback
                auto placeholder =
                    make_hir_literal(fresh_id(), int64_t(0), types::make_unit(), stmt.span);
                return make_hir_expr_stmt(fresh_id(), std::move(placeholder), stmt.span);
            }
        },
        stmt.kind);
}

// ============================================================================
// Let Statement
// ============================================================================
//
// `let` creates an immutable binding. The type is determined by:
// 1. Explicit type annotation: `let x: I32 = ...`
// 2. Initializer expression type: `let x = 42` infers I32
// 3. Default to unit if neither present (rare edge case)
//
// Pattern bindings extract names and register them in the current scope
// for later variable resolution in expressions.

auto HirBuilder::lower_let(const parser::LetStmt& let_stmt) -> HirStmtPtr {
    // Resolve type
    HirType type;
    if (let_stmt.type_annotation) {
        type = resolve_type(**let_stmt.type_annotation);
    } else if (let_stmt.init) {
        type = get_expr_type(**let_stmt.init);
    } else {
        type = types::make_unit();
    }

    // Lower pattern
    auto pattern = lower_pattern(*let_stmt.pattern, type);

    // Add bindings to current scope
    if (let_stmt.pattern->is<parser::IdentPattern>()) {
        const auto& ident = let_stmt.pattern->as<parser::IdentPattern>();
        if (!scopes_.empty()) {
            scopes_.back().insert(ident.name);
        }
        // Also define in type environment scope so we can look up the type later
        if (auto scope = type_env_.current_scope()) {
            scope->define(ident.name, type, ident.is_mut, let_stmt.span);
        }
    }

    // Lower initializer
    std::optional<HirExprPtr> init;
    if (let_stmt.init) {
        init = lower_expr(**let_stmt.init);
    }

    return make_hir_let(fresh_id(), std::move(pattern), type, std::move(init), let_stmt.span,
                        let_stmt.is_volatile);
}

// ============================================================================
// Var Statement (desugars to let mut)
// ============================================================================
//
// TML's `var x = value` is syntactic sugar for `let mut x = value`.
// This provides a familiar, more concise syntax for mutable variables.
//
// Unlike `let`, `var` always requires an initializer and uses simple
// identifier binding (not full pattern syntax).

auto HirBuilder::lower_var(const parser::VarStmt& var_stmt) -> HirStmtPtr {
    // Resolve type
    HirType type;
    if (var_stmt.type_annotation) {
        type = resolve_type(**var_stmt.type_annotation);
    } else {
        type = get_expr_type(*var_stmt.init);
    }

    // Create mutable binding pattern
    auto pattern = make_hir_binding_pattern(fresh_id(), var_stmt.name, true, type, var_stmt.span);

    // Add to current scope
    if (!scopes_.empty()) {
        scopes_.back().insert(var_stmt.name);
    }
    // Also define in type environment scope so we can look up the type later
    if (auto scope = type_env_.current_scope()) {
        scope->define(var_stmt.name, type, true, var_stmt.span);
    }

    // Lower initializer
    auto init = lower_expr(*var_stmt.init);

    return make_hir_let(fresh_id(), std::move(pattern), type, std::move(init), var_stmt.span,
                        var_stmt.is_volatile);
}

// ============================================================================
// Let-Else Statement (desugars to let + when)
// ============================================================================
//
// `let Pattern = expr else { block }` desugars to a when expression:
//
//   let Ok(x): Outcome[I32, Str] = result else { return Err("e") }
//
// becomes:
//
//   let x: I32 = when result {
//       Ok(__val) => __val,
//       _ => { return Err("e") }
//   }
//
// The else block must diverge (return, panic, break, continue).

auto HirBuilder::lower_let_else(const parser::LetElseStmt& let_else) -> HirStmtPtr {
    // Resolve the scrutinee type
    HirType scrutinee_type;
    if (let_else.type_annotation) {
        scrutinee_type = resolve_type(**let_else.type_annotation);
    } else {
        scrutinee_type = get_expr_type(*let_else.init);
    }

    // Extract the bound variable name and type from the pattern
    // For enum patterns like Ok(x) or Just(x), we need the inner binding
    std::string bound_name;
    HirType bound_type = types::make_unit();

    if (let_else.pattern->is<parser::EnumPattern>()) {
        const auto& enum_pat = let_else.pattern->as<parser::EnumPattern>();
        if (enum_pat.payload && !enum_pat.payload->empty()) {
            const auto& first_payload = (*enum_pat.payload)[0];
            if (first_payload->is<parser::IdentPattern>()) {
                bound_name = first_payload->as<parser::IdentPattern>().name;
                // For Outcome[T, E], Ok has type T; for Maybe[T], Just has type T
                if (scrutinee_type->is<types::NamedType>()) {
                    const auto& named = scrutinee_type->as<types::NamedType>();
                    if (!named.type_args.empty()) {
                        bound_type = named.type_args[0];
                    }
                }
            }
        }
    } else if (let_else.pattern->is<parser::IdentPattern>()) {
        bound_name = let_else.pattern->as<parser::IdentPattern>().name;
        bound_type = scrutinee_type;
    }

    // Add the binding to the outer scope FIRST, before lowering anything else
    // This ensures the variable is visible for the rest of the function
    if (!bound_name.empty() && !scopes_.empty()) {
        scopes_.back().insert(bound_name);
    }
    if (!bound_name.empty()) {
        if (auto scope = type_env_.current_scope()) {
            scope->define(bound_name, bound_type, false, let_else.span);
        }
    }

    // Lower the scrutinee expression
    auto scrutinee = lower_expr(*let_else.init);

    // Build the when expression with two arms:
    // 1. The pattern match arm (extracts the value)
    // 2. Wildcard arm (else block, must diverge)
    std::vector<HirWhenArm> arms;

    // First arm: the pattern match
    HirWhenArm match_arm;
    match_arm.pattern = lower_pattern(*let_else.pattern, scrutinee_type);
    // The arm body references the pattern-bound variable
    match_arm.body = make_hir_var(fresh_id(), bound_name, bound_type, let_else.span);
    match_arm.span = let_else.pattern->span;
    arms.push_back(std::move(match_arm));

    // Second arm: wildcard for else block
    HirWhenArm else_arm;
    else_arm.pattern = make_hir_wildcard_pattern(fresh_id(), let_else.span);
    else_arm.body = lower_expr(*let_else.else_block);
    else_arm.span = let_else.else_block->span;
    arms.push_back(std::move(else_arm));

    // Create the when expression
    auto when_expr = std::make_unique<HirExpr>();
    when_expr->kind =
        HirWhenExpr{fresh_id(), std::move(scrutinee), std::move(arms), bound_type, let_else.span};

    // Create a binding pattern for the let statement
    auto binding_pattern =
        make_hir_binding_pattern(fresh_id(), bound_name, false, bound_type, let_else.span);

    // Create the let statement with the when expression as initializer
    return make_hir_let(fresh_id(), std::move(binding_pattern), bound_type, std::move(when_expr),
                        let_else.span);
}

// ============================================================================
// Expression Statement
// ============================================================================
//
// Expression statements evaluate an expression for its side effects,
// discarding the result. Common examples:
// - Function calls: `print("hello")`
// - Assignments: `x = 42`
// - Method calls: `list.push(item)`

auto HirBuilder::lower_expr_stmt(const parser::ExprStmt& expr_stmt) -> HirStmtPtr {
    auto expr = lower_expr(*expr_stmt.expr);
    return make_hir_expr_stmt(fresh_id(), std::move(expr), expr_stmt.span);
}

// ============================================================================
// Nested Declaration
// ============================================================================
//
// Handles declarations that appear inside blocks. The parser allows:
// - `const` - lowered to immutable let binding
// - `func`, `type`, `behavior`, `impl` - parsed but limited support
//
// Current support status:
// - ConstDecl: Fully supported. Lowered to `let NAME: TYPE = value`.
// - FuncDecl: Parser allows, but type checker/codegen don't fully support
//   nested functions yet. Returns placeholder.
// - TypeAliasDecl: Handled in type environment. Returns placeholder.
// - Other declarations: Return placeholder (rare in nested contexts).
//
// Placeholders generate a unit-typed expression statement that has no
// runtime effect but maintains valid HIR structure.

auto HirBuilder::lower_nested_decl(const parser::Decl& decl, SourceSpan span) -> HirStmtPtr {
    return std::visit(
        [&](const auto& d) -> HirStmtPtr {
            using T = std::decay_t<decltype(d)>;

            if constexpr (std::is_same_v<T, parser::ConstDecl>) {
                // const NAME: TYPE = value becomes let NAME: TYPE = value
                HirType type = resolve_type(*d.type);

                // Create immutable binding pattern
                auto pattern = make_hir_binding_pattern(fresh_id(), d.name, false, type, d.span);

                // Add to scope
                if (!scopes_.empty()) {
                    scopes_.back().insert(d.name);
                }
                if (auto scope = type_env_.current_scope()) {
                    scope->define(d.name, type, false, d.span);
                }

                // Lower value expression
                auto init = lower_expr(*d.value);

                return make_hir_let(fresh_id(), std::move(pattern), type, std::move(init), d.span);

            } else if constexpr (std::is_same_v<T, parser::FuncDecl>) {
                // Nested functions are registered but don't produce runtime code here
                // They're lowered separately when called
                // Return a unit placeholder
                auto placeholder =
                    make_hir_literal(fresh_id(), int64_t(0), types::make_unit(), span);
                return make_hir_expr_stmt(fresh_id(), std::move(placeholder), span);

            } else if constexpr (std::is_same_v<T, parser::TypeAliasDecl>) {
                // Type aliases are handled in the type environment
                // Return a unit placeholder
                auto placeholder =
                    make_hir_literal(fresh_id(), int64_t(0), types::make_unit(), span);
                return make_hir_expr_stmt(fresh_id(), std::move(placeholder), span);

            } else {
                // Other nested declarations (struct, enum, impl, etc.) are rare
                // They're typically registered in type tables during an earlier pass
                auto placeholder =
                    make_hir_literal(fresh_id(), int64_t(0), types::make_unit(), span);
                return make_hir_expr_stmt(fresh_id(), std::move(placeholder), span);
            }
        },
        decl.kind);
}

} // namespace tml::hir
