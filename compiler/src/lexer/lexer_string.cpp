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
//!
//! Interpolation only starts when `{` is followed by a valid identifier start
//! character (letter or underscore). Otherwise `{` is treated as a literal:
//! - `"{ key: value }"` → literal string (no interpolation)
//! - `"{name}"` → interpolation (identifier follows `{`)
//! - `"\{"` → literal `{` (escaped, always works)

#include "lexer/lexer.hpp"

#include <charconv>
#include <system_error>
namespace tml::lexer {

// Helper to check if a character can start an identifier (for interpolation detection)
static bool is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

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
            return make_error_token("Unterminated string literal", "L002");
        }

        // Check for interpolation: { starts an interpolated expression
        // Only if followed by an identifier start character (letter or underscore)
        if (peek() == '{') {
            // Look ahead to see if this is interpolation or literal brace
            size_t next_pos = pos_ + 1;
            if (next_pos < source_.length() && is_ident_start(source_.content()[next_pos])) {
                // This is an interpolated string!
                // Return InterpStringStart with the text we've collected so far
                advance(); // consume the '{'
                interp_depth_++;
                in_interpolation_ = true;

                auto token = make_token(TokenKind::InterpStringStart);
                token.value = StringValue{std::move(value), false};
                return token;
            }
            // Not followed by identifier - treat as literal brace
            value += advance();
            continue;
        }

        // Allow literal } in strings (not inside interpolation)
        if (peek() == '}') {
            value += advance();
            continue;
        }

        if (peek() == '\\') {
            advance(); // consume backslash
            // Check for escaped brace (still supported for backwards compatibility)
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
        return make_error_token("Unterminated string literal", "L002");
    }

    // Skip closing quote
    advance();

    if (has_error) {
        return make_error_token("Invalid escape sequence in string", "L004");
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
            return make_error_token("Unterminated string literal", "L002");
        }

        // Check for another interpolation
        // Only if followed by an identifier start character (letter or underscore)
        if (peek() == '{') {
            size_t next_pos = pos_ + 1;
            if (next_pos < source_.length() && is_ident_start(source_.content()[next_pos])) {
                advance(); // consume the '{'
                // Return middle segment
                auto token = make_token(TokenKind::InterpStringMiddle);
                token.value = StringValue{std::move(value), false};
                return token;
            }
            // Not followed by identifier - treat as literal brace
            value += advance();
            continue;
        }

        // Allow literal } in string continuation (outside of interpolation expression)
        if (peek() == '}') {
            value += advance();
            continue;
        }

        if (peek() == '\\') {
            advance(); // consume backslash
            // Check for escaped brace (still supported for backwards compatibility)
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
        return make_error_token("Unterminated string literal", "L002");
    }

    // Skip closing quote
    advance();
    interp_depth_--;
    in_interpolation_ = false;

    if (has_error) {
        return make_error_token("Invalid escape sequence in string", "L004");
    }

    auto token = make_token(TokenKind::InterpStringEnd);
    token.value = StringValue{std::move(value), false};
    return token;
}

auto Lexer::check_string_has_interpolation() const -> bool {
    // Check if the current string (starting with ") contains an unescaped {
    // followed by an identifier start character
    size_t i = pos_ + 1; // Skip opening quote
    while (i < source_.length()) {
        char c = source_.content()[i];
        if (c == '"')
            return false; // End of string, no interpolation
        if (c == '\n')
            return false; // Unterminated
        if (c == '{') {
            // Only count as interpolation if followed by identifier start
            if (i + 1 < source_.length() && is_ident_start(source_.content()[i + 1])) {
                return true; // Found interpolation
            }
            // Literal brace, continue searching
            i++;
            continue;
        }
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
            return make_error_token("Unterminated raw string literal", "L013");
        }
        value += advance();
    }

    if (is_at_end()) {
        return make_error_token("Unterminated raw string literal", "L013");
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
        return make_error_token("Empty character literal", "L006");
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
        return make_error_token("Unterminated character literal", "L005");
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

// ============================================================================
// Template Literals (backtick strings - produce Text type)
// ============================================================================

auto Lexer::lex_template_literal() -> Token {
    // Skip opening backtick
    advance();

    std::string value;
    bool has_error = false;

    while (!is_at_end() && peek() != '`') {
        // Template literals support multi-line strings
        if (peek() == '\n') {
            value += advance();
            continue;
        }

        // Check for interpolation: { starts an interpolated expression
        // Only if followed by an identifier start character (letter or underscore)
        if (peek() == '{') {
            // Look ahead to see if this is interpolation or literal brace
            size_t next_pos = pos_ + 1;
            if (next_pos < source_.length() && is_ident_start(source_.content()[next_pos])) {
                // This is an interpolated template!
                // Return TemplateLiteralStart with the text we've collected so far
                advance(); // consume the '{'
                template_depth_++;
                in_template_literal_ = true;

                auto token = make_token(TokenKind::TemplateLiteralStart);
                token.value = StringValue{std::move(value), false};
                return token;
            }
            // Not followed by identifier - treat as literal brace
            value += advance();
            continue;
        }

        // Allow literal } in template literals (not inside interpolation)
        if (peek() == '}') {
            value += advance();
            continue;
        }

        if (peek() == '\\') {
            advance(); // consume backslash
            // Check for escaped backtick or brace
            if (peek() == '`' || peek() == '{' || peek() == '}') {
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
        return make_error_token("Unterminated template literal", "L015");
    }

    // Skip closing backtick
    advance();

    if (has_error) {
        return make_error_token("Invalid escape sequence in template literal", "L004");
    }

    // A simple template literal without interpolation - still produces TemplateLiteralEnd
    // to signal to the type checker that this is a Text type
    auto token = make_token(TokenKind::TemplateLiteralEnd);
    token.value = StringValue{std::move(value), false};
    return token;
}

auto Lexer::lex_template_literal_continue() -> Token {
    // Called after parsing an interpolated expression when we see '}'
    // We're positioned right after the '}'
    // Continue lexing the template until we hit another '{' or '`'

    std::string value;
    bool has_error = false;

    while (!is_at_end() && peek() != '`') {
        // Template literals support multi-line strings
        if (peek() == '\n') {
            value += advance();
            continue;
        }

        // Check for another interpolation
        // Only if followed by an identifier start character (letter or underscore)
        if (peek() == '{') {
            size_t next_pos = pos_ + 1;
            if (next_pos < source_.length() && is_ident_start(source_.content()[next_pos])) {
                advance(); // consume the '{'
                // Return middle segment
                auto token = make_token(TokenKind::TemplateLiteralMiddle);
                token.value = StringValue{std::move(value), false};
                return token;
            }
            // Not followed by identifier - treat as literal brace
            value += advance();
            continue;
        }

        // Allow literal } in template continuation (outside of interpolation expression)
        if (peek() == '}') {
            value += advance();
            continue;
        }

        if (peek() == '\\') {
            advance(); // consume backslash
            // Check for escaped backtick or brace
            if (peek() == '`' || peek() == '{' || peek() == '}') {
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
        return make_error_token("Unterminated template literal", "L015");
    }

    // Skip closing backtick
    advance();
    template_depth_--;
    in_template_literal_ = false;

    if (has_error) {
        return make_error_token("Invalid escape sequence in template literal", "L004");
    }

    auto token = make_token(TokenKind::TemplateLiteralEnd);
    token.value = StringValue{std::move(value), false};
    return token;
}

} // namespace tml::lexer
