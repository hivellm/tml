#include "tml/lexer/lexer.hpp"

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

    // Characters
    if (c == '\'') {
        return lex_char();
    }

    // Operators and delimiters
    return lex_operator();
}


} // namespace tml::lexer
