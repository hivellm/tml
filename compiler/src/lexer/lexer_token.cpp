//! # Lexer - Token Dispatch
//!
//! This file implements the main `next_token()` entry point.
//!
//! ## Token Dispatch Order
//!
//! 1. Skip whitespace (spaces, tabs, carriage returns)
//! 2. Return `Eof` if at end of input
//! 3. Return `Newline` for significant newlines
//! 4. Check for raw strings (`r"..."`)
//! 5. Lex identifiers and keywords
//! 6. Lex numbers
//! 7. Lex strings
//! 8. Lex characters
//! 9. Lex operators and delimiters
//!
//! ## Significant Newlines
//!
//! Unlike many languages, TML preserves newlines as tokens for
//! statement separation (similar to Go or Python without explicit
//! semicolons in most cases).

#include "lexer/lexer.hpp"

namespace tml::lexer {

auto Lexer::next_token() -> Token {
    skip_whitespace();
    token_start_ = pos_;

    if (is_at_end()) {
        return make_token(TokenKind::Eof);
    }

    char c = peek();

    // Newline (significant)
    if (c == '\n') {
        advance();
        return make_token(TokenKind::Newline);
    }

    // Doc comments (check before operators, since /// starts with /)
    if (c == '/' && is_doc_comment()) {
        return lex_doc_comment();
    }

    // Raw strings (check before identifiers, since 'r' is a valid identifier start)
    if (c == 'r' && peek_next() == '"') {
        return lex_raw_string();
    }

    // Identifiers and keywords
    if (is_identifier_start(static_cast<char32_t>(c))) {
        return lex_identifier();
    }

    // Numbers
    if (c >= '0' && c <= '9') {
        return lex_number();
    }

    // Strings
    if (c == '"') {
        return lex_string();
    }

    // Template literals (backtick strings - produce Text type)
    if (c == '`') {
        return lex_template_literal();
    }

    // Characters
    if (c == '\'') {
        return lex_char();
    }

    // Operators and delimiters
    return lex_operator();
}

} // namespace tml::lexer
