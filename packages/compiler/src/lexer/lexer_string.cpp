#include "tml/lexer/lexer.hpp"

#include <charconv>
#include <system_error>
namespace tml::lexer {

auto Lexer::lex_string() -> Token {
    // Skip opening quote
    advance();

    std::string value;
    bool has_error = false;

    while (!is_at_end() && peek() != '"') {
        if (peek() == '\n') {
            return make_error_token("Unterminated string literal");
        }

        if (peek() == '\\') {
            advance(); // consume backslash
            auto escape_result = parse_escape_sequence();
            if (is_ok(escape_result)) {
                // Encode as UTF-8
                char32_t cp = unwrap(escape_result);
                if (cp < 0x80) {
                    value += static_cast<char>(cp);
                } else if (cp < 0x800) {
                    value += static_cast<char>(0xC0 | (cp >> 6));
                    value += static_cast<char>(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    value += static_cast<char>(0xE0 | (cp >> 12));
                    value += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    value += static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    value += static_cast<char>(0xF0 | (cp >> 18));
                    value += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                    value += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    value += static_cast<char>(0x80 | (cp & 0x3F));
                }
            } else {
                report_error(unwrap_err(escape_result));
                has_error = true;
            }
        } else {
            value += advance();
        }
    }

    if (is_at_end()) {
        return make_error_token("Unterminated string literal");
    }

    // Skip closing quote
    advance();

    if (has_error) {
        return make_error_token("Invalid escape sequence in string");
    }

    auto token = make_token(TokenKind::StringLiteral);
    token.value = StringValue{std::move(value), false};
    return token;
}

auto Lexer::lex_raw_string() -> Token {
    // Skip r"
    advance();
    advance();

    std::string value;

    while (!is_at_end() && peek() != '"') {
        if (peek() == '\n') {
            return make_error_token("Unterminated raw string literal");
        }
        value += advance();
    }

    if (is_at_end()) {
        return make_error_token("Unterminated raw string literal");
    }

    // Skip closing quote
    advance();

    auto token = make_token(TokenKind::StringLiteral);
    token.value = StringValue{std::move(value), true};
    return token;
}

auto Lexer::lex_char() -> Token {
    // Skip opening quote
    advance();

    if (is_at_end() || peek() == '\'') {
        return make_error_token("Empty character literal");
    }

    char32_t value;
    if (peek() == '\\') {
        advance();
        auto escape_result = parse_escape_sequence();
        if (is_err(escape_result)) {
            return make_error_token(unwrap_err(escape_result));
        }
        value = unwrap(escape_result);
    } else {
        value = decode_utf8();
    }

    if (is_at_end() || peek() != '\'') {
        return make_error_token("Unterminated character literal");
    }

    // Skip closing quote
    advance();

    auto token = make_token(TokenKind::CharLiteral);
    token.value = CharValue{value};
    return token;
}

auto Lexer::parse_escape_sequence() -> Result<char32_t, std::string> {
    if (is_at_end()) {
        return "Unexpected end of file in escape sequence";
    }

    char c = advance();
    switch (c) {
        case 'n': return char32_t('\n');
        case 'r': return char32_t('\r');
        case 't': return char32_t('\t');
        case '\\': return char32_t('\\');
        case '\'': return char32_t('\'');
        case '"': return char32_t('"');
        case '0': return char32_t('\0');
        case 'x': {
            // \xNN - two hex digits
            if (pos_ + 2 > source_.length()) {
                return "Expected two hex digits after \\x";
            }
            char h1 = advance();
            char h2 = advance();
            uint8_t val = 0;
            auto result = std::from_chars(&h1, &h1 + 1, val, 16);
            if (result.ec != std::errc{}) {
                return "Invalid hex digit in \\x escape";
            }
            val <<= 4;
            uint8_t val2 = 0;
            result = std::from_chars(&h2, &h2 + 1, val2, 16);
            if (result.ec != std::errc{}) {
                return "Invalid hex digit in \\x escape";
            }
            val |= val2;
            return char32_t(val);
        }
        case 'u': return parse_unicode_escape();
        default:
            return "Unknown escape sequence: \\" + std::string(1, c);
    }
}

auto Lexer::parse_unicode_escape() -> Result<char32_t, std::string> {
    // \u{NNNN} format
    if (is_at_end() || peek() != '{') {
        return "Expected '{' after \\u";
    }
    advance();

    std::string hex_digits;
    while (!is_at_end() && peek() != '}') {
        char c = peek();
        if ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F')) {
            hex_digits += advance();
        } else {
            return "Invalid character in unicode escape";
        }

        if (hex_digits.size() > 6) {
            return "Unicode escape too long (max 6 hex digits)";
        }
    }

    if (is_at_end()) {
        return "Unterminated unicode escape";
    }

    if (hex_digits.empty()) {
        return "Empty unicode escape";
    }

    advance(); // consume '}'

    uint32_t value = 0;
    auto result = std::from_chars(hex_digits.data(), hex_digits.data() + hex_digits.size(), value, 16);
    if (result.ec != std::errc{}) {
        return "Invalid unicode escape value";
    }

    if (value > 0x10FFFF) {
        return "Unicode escape out of range";
    }

    return char32_t(value);
}


} // namespace tml::lexer
