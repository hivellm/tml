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
//!
//! ## Compile-Time Constants
//!
//! Special identifiers are expanded at lex time:
//! - `__FILE__`    → StringLiteral with the source file path
//! - `__DIRNAME__` → StringLiteral with the source file's directory
//! - `__LINE__`    → IntLiteral with the current line number

#include "lexer/lexer.hpp"

#include <algorithm>
#include <string>
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

    // Compile-time constants: __FILE__, __DIRNAME__, __LINE__
    if (lexeme == "__FILE__") {
        auto token = make_token(TokenKind::StringLiteral);
        std::string filepath(source_.filename());
        std::replace(filepath.begin(), filepath.end(), '\\', '/');
        token.value = StringValue{.value = std::move(filepath), .is_raw = false};
        return token;
    }

    if (lexeme == "__DIRNAME__") {
        auto token = make_token(TokenKind::StringLiteral);
        std::string filepath(source_.filename());
        std::replace(filepath.begin(), filepath.end(), '\\', '/');
        auto slash_pos = filepath.rfind('/');
        std::string dir = (slash_pos != std::string::npos) ? filepath.substr(0, slash_pos) : ".";
        token.value = StringValue{.value = std::move(dir), .is_raw = false};
        return token;
    }

    if (lexeme == "__LINE__") {
        auto token = make_token(TokenKind::IntLiteral);
        auto loc = source_.location(token_start_);
        token.value = IntValue{.value = static_cast<uint64_t>(loc.line), .base = 10, .suffix = ""};
        return token;
    }

    return make_token(TokenKind::Identifier);
}

} // namespace tml::lexer
