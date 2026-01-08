//! # Lexer - Identifiers
//!
//! This file implements identifier and keyword lexing.
//!
//! ## Identifier Rules
//!
//! - Start with letter (a-z, A-Z) or underscore
//! - Continue with letters, digits, or underscores
//! - Unicode letters supported for internationalization
//!
//! ## Keyword Lookup
//!
//! After lexing an identifier, it's checked against the keyword table.
//! If found, the token kind is set to the keyword. Boolean literals
//! (`true`, `false`) are special-cased to set their value.

#include "lexer/lexer.hpp"

#include <unordered_map>

namespace tml::lexer {

// Get keywords from core module
extern const std::unordered_map<std::string_view, TokenKind>& get_keywords();

auto Lexer::lex_identifier() -> Token {
    while (!is_at_end() && is_identifier_continue(static_cast<char32_t>(peek()))) {
        advance();
    }

    auto lexeme = source_.slice(token_start_, pos_);

    // Check if it's a keyword
    const auto& keywords = get_keywords();
    auto it = keywords.find(lexeme);
    if (it != keywords.end()) {
        auto token = make_token(it->second);
        // Handle boolean literals
        if (it->second == TokenKind::BoolLiteral) {
            token.value = (lexeme == "true");
        }
        return token;
    }

    return make_token(TokenKind::Identifier);
}

} // namespace tml::lexer
