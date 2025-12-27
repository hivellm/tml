#include "tml/parser/parser.hpp"

namespace tml::parser {

// ============================================================================
// Statement Parsing
// ============================================================================

auto Parser::parse_stmt() -> Result<StmtPtr, ParseError> {
    skip_newlines();

    if (check(lexer::TokenKind::KwLet)) {
        return parse_let_stmt();
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

    auto pattern = parse_pattern();
    if (is_err(pattern))
        return unwrap_err(pattern);

    // Type annotation is REQUIRED in TML (explicit typing for LLM clarity)
    auto colon = expect(
        lexer::TokenKind::Colon,
        "Expected ':' and type annotation after variable name (TML requires explicit types)");
    if (is_err(colon))
        return unwrap_err(colon);

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
                            .span = SourceSpan::merge(start_span, end_span)};

    return make_box<Stmt>(
        Stmt{.kind = std::move(let_stmt), .span = SourceSpan::merge(start_span, end_span)});
}

auto Parser::parse_var_stmt() -> Result<StmtPtr, ParseError> {
    // TML doesn't have 'var' keyword - use 'let mut' for mutable bindings
    return ParseError{.message =
                          "TML doesn't have 'var' keyword - use 'let mut' for mutable bindings",
                      .span = peek().span,
                      .notes = {}};
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
