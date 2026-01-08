//! # Lexer - Strings
//!
//! This file implements string and character literal lexing.
//!
//! ## String Types
//!
//! | Type         | Syntax        | Description                |
//! |--------------|---------------|----------------------------|
//! | Regular      | `"hello"`     | Escape sequences processed |
//! | Raw          | `r"hello"`    | No escape processing       |
//! | Interpolated | `"Hi {name}"` | Embedded expressions       |
//!
//! ## Escape Sequences
//!
//! | Escape | Character              |
//! |--------|------------------------|
//! | `\n`   | Newline                |
//! | `\t`   | Tab                    |
//! | `\r`   | Carriage return        |
//! | `\\`   | Backslash              |
//! | `\"`   | Double quote           |
//! | `\'`   | Single quote           |
//! | `\0`   | Null                   |
//! | `\xNN` | Hex byte               |
//! | `\u{N}`| Unicode codepoint      |
//!
//! ## Interpolation
//!
//! `"Hello {name}!"` produces:
//! 1. `InterpStringStart` ("Hello ")
//! 2. Expression tokens (name)
//! 3. `InterpStringEnd` ("!")

#include "lexer/lexer.hpp"

#include <charconv>
#include <system_error>
namespace tml::lexer {

// Helper to encode a Unicode codepoint as UTF-8
static void encode_utf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

auto Lexer::lex_string() -> Token {
    // Skip opening quote
    advance();

    std::string value;
    bool has_error = false;

    while (!is_at_end() && peek() != '"') {
        if (peek() == '\n') {
            return make_error_token("Unterminated string literal");
        }

        // Check for interpolation: { starts an interpolated expression
        if (peek() == '{') {
            // This is an interpolated string!
            // Return InterpStringStart with the text we've collected so far
            advance(); // consume the '{'
            interp_depth_++;
            in_interpolation_ = true;

            auto token = make_token(TokenKind::InterpStringStart);
            token.value = StringValue{std::move(value), false};
            return token;
        }

        if (peek() == '\\') {
            advance(); // consume backslash
            // Check for escaped brace
            if (peek() == '{' || peek() == '}') {
                value += advance();
                continue;
            }
            auto escape_result = parse_escape_sequence();
            if (is_ok(escape_result)) {
                encode_utf8(value, unwrap(escape_result));
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

auto Lexer::lex_interp_string_continue() -> Token {
    // Called after parsing an interpolated expression when we see '}'
    // We're positioned right after the '}'
    // Continue lexing the string until we hit another '{' or '"'

    std::string value;
    bool has_error = false;

    while (!is_at_end() && peek() != '"') {
        if (peek() == '\n') {
            return make_error_token("Unterminated string literal");
        }

        // Check for another interpolation
        if (peek() == '{') {
            advance(); // consume the '{'
            // Return middle segment
            auto token = make_token(TokenKind::InterpStringMiddle);
            token.value = StringValue{std::move(value), false};
            return token;
        }

        if (peek() == '\\') {
            advance(); // consume backslash
            // Check for escaped brace
            if (peek() == '{' || peek() == '}') {
                value += advance();
                continue;
            }
            auto escape_result = parse_escape_sequence();
            if (is_ok(escape_result)) {
                encode_utf8(value, unwrap(escape_result));
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
    interp_depth_--;
    in_interpolation_ = false;

    if (has_error) {
        return make_error_token("Invalid escape sequence in string");
    }

    auto token = make_token(TokenKind::InterpStringEnd);
    token.value = StringValue{std::move(value), false};
    return token;
}

auto Lexer::check_string_has_interpolation() const -> bool {
    // Check if the current string (starting with ") contains an unescaped {
    size_t i = pos_ + 1; // Skip opening quote
    while (i < source_.length()) {
        char c = source_.content()[i];
        if (c == '"')
            return false; // End of string, no interpolation
        if (c == '\n')
            return false; // Unterminated
        if (c == '{')
            return true; // Found interpolation
        if (c == '\\' && i + 1 < source_.length()) {
            i += 2; // Skip escape sequence
        } else {
            i++;
        }
    }
    return false;
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
    case 'n':
        return char32_t('\n');
    case 'r':
        return char32_t('\r');
    case 't':
        return char32_t('\t');
    case '\\':
        return char32_t('\\');
    case '\'':
        return char32_t('\'');
    case '"':
        return char32_t('"');
    case '0':
        return char32_t('\0');
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
    case 'u':
        return parse_unicode_escape();
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
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) {
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
    auto result =
        std::from_chars(hex_digits.data(), hex_digits.data() + hex_digits.size(), value, 16);
    if (result.ec != std::errc{}) {
        return "Invalid unicode escape value";
    }

    if (value > 0x10FFFF) {
        return "Unicode escape out of range";
    }

    return char32_t(value);
}

} // namespace tml::lexer
