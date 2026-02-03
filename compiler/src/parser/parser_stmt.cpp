//! # Parser - Statements
//!
//! This file implements statement parsing.
//!
//! ## Statement Types
//!
//! | Statement | Syntax                        | Notes                    |
//! |-----------|-------------------------------|--------------------------|
//! | Let       | `let x: T = expr`             | Immutable binding        |
//! | Var       | `var x: T = expr`             | Mutable (= `let mut`)    |
//! | Expr      | `expr`                        | Expression statement     |
//! | Decl      | `func`, `type`, etc.          | Nested declarations      |
//!
//! ## TML Explicit Typing
//!
//! TML requires explicit type annotations for all variables:
//! ```tml
//! let count: I32 = 0       // Required
//! var total: F64 = 0.0     // Required
//! ```
//!
//! This is by design for LLM clarity - no type inference on declarations.
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

    // Check if it's a declaration
    if (check(lexer::TokenKind::KwPub) || check(lexer::TokenKind::KwFunc) ||
        check(lexer::TokenKind::KwType) || check(lexer::TokenKind::KwBehavior) ||
        check(lexer::TokenKind::KwImpl)) {
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

    // Type annotation is REQUIRED in TML (explicit typing for LLM clarity)
    if (!check(lexer::TokenKind::Colon)) {
        auto fix = make_insertion_fix(previous().span, ": Type", "add type annotation");
        return ParseError{.message = "Expected ':' and type annotation after variable name (TML "
                                     "requires explicit types)",
                          .span = peek().span,
                          .notes = {"TML requires explicit type annotations for all variables"},
                          .fixes = {fix},
                          .code = "P008"};
    }
    advance(); // consume ':'

    auto type = parse_type();
    if (is_err(type))
        return unwrap_err(type);
    std::optional<TypePtr> type_annotation = std::move(unwrap(type));

    std::optional<ExprPtr> init;
    if (match(lexer::TokenKind::Assign)) {
        auto expr = parse_expr();
        if (is_err(expr))
            return unwrap_err(expr);
        init = std::move(unwrap(expr));
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

    // Type annotation is REQUIRED in TML
    if (!check(lexer::TokenKind::Colon)) {
        auto fix = make_insertion_fix(previous().span, ": Type", "add type annotation");
        return ParseError{.message = "Expected ':' and type annotation after variable name (TML "
                                     "requires explicit types)",
                          .span = peek().span,
                          .notes = {"TML requires explicit type annotations for all variables"},
                          .fixes = {fix},
                          .code = "P008"};
    }
    advance(); // consume ':'

    auto type = parse_type();
    if (is_err(type))
        return unwrap_err(type);
    std::optional<TypePtr> type_annotation = std::move(unwrap(type));

    std::optional<ExprPtr> init;
    if (match(lexer::TokenKind::Assign)) {
        auto expr = parse_expr();
        if (is_err(expr))
            return unwrap_err(expr);
        init = std::move(unwrap(expr));
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
