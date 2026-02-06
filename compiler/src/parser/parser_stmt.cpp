//! # Parser - Statements
//!
//! This file implements statement parsing.
//!
//! ## Statement Types
//!
//! | Statement | Syntax                        | Notes                    |
//! |-----------|-------------------------------|--------------------------|
//! | Let       | `let x: T = expr`             | Immutable binding        |
//! | Let       | `let x = expr`                | Type inferred from RHS   |
//! | Var       | `var x: T = expr`             | Mutable (= `let mut`)    |
//! | Var       | `var x = expr`                | Type inferred from RHS   |
//! | Expr      | `expr`                        | Expression statement     |
//! | Decl      | `const`, `func`, `type`, etc. | Nested declarations      |
//!
//! ## Type Inference
//!
//! TML supports optional type annotations when the type can be inferred:
//! ```tml
//! let count: I32 = 0       // Explicit type
//! let count = 0            // Inferred as I32 from literal
//! let result = get_value() // Inferred from function return type
//! ```
//!
//! When type annotation is omitted, an initializer is REQUIRED.
//!
//! ## Var Desugaring
//!
//! `var x: T = expr` is internally converted to `let mut x: T = expr`.

#include "parser/parser.hpp"

namespace tml::parser {

// ============================================================================
// Statement Parsing
// ============================================================================

auto Parser::parse_stmt() -> Result<StmtPtr, ParseError> {
    skip_newlines();

    if (check(lexer::TokenKind::KwLet)) {
        return parse_let_stmt();
    }

    // var is an alias for let mut (mutable variable)
    if (check(lexer::TokenKind::KwVar)) {
        return parse_var_stmt();
    }

    // Check if it's a declaration (including const inside functions)
    if (check(lexer::TokenKind::KwPub) || check(lexer::TokenKind::KwFunc) ||
        check(lexer::TokenKind::KwType) || check(lexer::TokenKind::KwBehavior) ||
        check(lexer::TokenKind::KwImpl) || check(lexer::TokenKind::KwConst)) {
        auto decl = parse_decl();
        if (is_err(decl))
            return unwrap_err(decl);
        auto span = unwrap(decl)->span;
        return make_box<Stmt>(Stmt{.kind = std::move(unwrap(decl)), .span = span});
    }

    return parse_expr_stmt();
}

auto Parser::parse_let_stmt() -> Result<StmtPtr, ParseError> {
    auto start_span = peek().span;

    auto let_tok = expect(lexer::TokenKind::KwLet, "Expected 'let'");
    if (is_err(let_tok))
        return unwrap_err(let_tok);

    // Check for 'volatile' modifier
    bool is_volatile = match(lexer::TokenKind::KwVolatile);

    auto pattern = parse_pattern();
    if (is_err(pattern))
        return unwrap_err(pattern);

    // Type annotation is optional - if omitted, initializer is required for inference
    std::optional<TypePtr> type_annotation;
    if (match(lexer::TokenKind::Colon)) {
        auto type = parse_type();
        if (is_err(type))
            return unwrap_err(type);
        type_annotation = std::move(unwrap(type));
    }

    std::optional<ExprPtr> init;
    if (match(lexer::TokenKind::Assign)) {
        auto expr = parse_expr();
        if (is_err(expr))
            return unwrap_err(expr);
        init = std::move(unwrap(expr));
    }

    // If no type annotation, initializer is required for type inference
    if (!type_annotation && !init) {
        return ParseError{.message = "Type annotation or initializer required",
                          .span = peek().span,
                          .notes = {"Add ': Type' or '= value' to declare the variable"},
                          .code = "P008"};
    }

    // Check for let-else syntax: let Pattern: T = expr else { block }
    skip_newlines();
    if (match(lexer::TokenKind::KwElse)) {
        // Must have an initializer for let-else
        if (!init) {
            return ParseError{.message = "let-else requires an initializer expression",
                              .span = peek().span,
                              .notes = {"let Pattern: T = expr else { ... } requires '= expr'"},
                              .code = "P009"};
        }

        // Parse the else block (must be a block expression)
        auto else_block = parse_block_expr();
        if (is_err(else_block))
            return unwrap_err(else_block);

        auto end_span = previous().span;

        auto let_else_stmt = LetElseStmt{.pattern = std::move(unwrap(pattern)),
                                         .type_annotation = std::move(type_annotation),
                                         .init = std::move(*init),
                                         .else_block = std::move(unwrap(else_block)),
                                         .span = SourceSpan::merge(start_span, end_span)};

        return make_box<Stmt>(Stmt{.kind = std::move(let_else_stmt),
                                   .span = SourceSpan::merge(start_span, end_span)});
    }

    auto end_span = previous().span;

    auto let_stmt = LetStmt{.pattern = std::move(unwrap(pattern)),
                            .type_annotation = std::move(type_annotation),
                            .init = std::move(init),
                            .span = SourceSpan::merge(start_span, end_span),
                            .is_volatile = is_volatile};

    return make_box<Stmt>(
        Stmt{.kind = std::move(let_stmt), .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_var_stmt() -> Result<StmtPtr, ParseError> {
    // 'var x: T = expr' is equivalent to 'let mut x: T = expr'
    auto start_span = peek().span;

    auto var_tok = expect(lexer::TokenKind::KwVar, "Expected 'var'");
    if (is_err(var_tok))
        return unwrap_err(var_tok);

    // Check for 'volatile' modifier
    bool is_volatile = match(lexer::TokenKind::KwVolatile);

    // Parse variable name (identifier pattern)
    auto name_tok = expect(lexer::TokenKind::Identifier, "Expected variable name after 'var'");
    if (is_err(name_tok))
        return unwrap_err(name_tok);

    std::string_view name = unwrap(name_tok).lexeme;

    // Type annotation is optional - if omitted, initializer is required for inference
    std::optional<TypePtr> type_annotation;
    if (match(lexer::TokenKind::Colon)) {
        auto type = parse_type();
        if (is_err(type))
            return unwrap_err(type);
        type_annotation = std::move(unwrap(type));
    }

    std::optional<ExprPtr> init;
    if (match(lexer::TokenKind::Assign)) {
        auto expr = parse_expr();
        if (is_err(expr))
            return unwrap_err(expr);
        init = std::move(unwrap(expr));
    }

    // If no type annotation, initializer is required for type inference
    if (!type_annotation && !init) {
        return ParseError{.message = "Type annotation or initializer required for 'var'",
                          .span = peek().span,
                          .notes = {"Add ': Type' or '= value' to declare the variable"},
                          .code = "P008"};
    }

    auto end_span = previous().span;

    // Create a mutable identifier pattern (equivalent to 'let mut name')
    auto pattern = make_ident_pattern(std::string(name), true, start_span);

    auto let_stmt = LetStmt{.pattern = std::move(pattern),
                            .type_annotation = std::move(type_annotation),
                            .init = std::move(init),
                            .span = SourceSpan::merge(start_span, end_span),
                            .is_volatile = is_volatile};

    return make_box<Stmt>(
        Stmt{.kind = std::move(let_stmt), .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_expr_stmt() -> Result<StmtPtr, ParseError> {
    auto expr = parse_expr();
    if (is_err(expr))
        return unwrap_err(expr);

    auto span = unwrap(expr)->span;

    return make_box<Stmt>(
        Stmt{.kind = ExprStmt{.expr = std::move(unwrap(expr)), .span = span}, .span = span});
}

} // namespace tml::parser
