#include "tml/lexer/lexer.hpp"

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
