//! # Lexer - Numbers
//!
//! This file implements numeric literal lexing.
//!
//! ## Number Formats
//!
//! | Format     | Prefix | Example      | Base |
//! |------------|--------|--------------|------|
//! | Decimal    | (none) | `42`, `3.14` | 10   |
//! | Hexadecimal| `0x`   | `0xFF`       | 16   |
//! | Binary     | `0b`   | `0b1010`     | 2    |
//! | Octal      | `0o`   | `0o755`      | 8    |
//!
//! ## Features
//!
//! - **Numeric separators**: `1_000_000` for readability
//! - **Float literals**: `3.14`, `1e10`, `2.5e-3`
//! - **Type suffixes**: `42i32`, `3.14f64`
//!
//! ## Valid Suffixes
//!
//! - Integers: `i8`, `i16`, `i32`, `i64`, `i128`, `u8`..`u128`
//! - Floats: `f32`, `f64`

#include "lexer/lexer.hpp"

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>

namespace tml::lexer {

namespace {

// Valid integer suffixes
bool is_valid_int_suffix(std::string_view suffix) {
    return suffix == "i8" || suffix == "i16" || suffix == "i32" || suffix == "i64" ||
           suffix == "i128" || suffix == "u8" || suffix == "u16" || suffix == "u32" ||
           suffix == "u64" || suffix == "u128";
}

// Valid float suffixes
bool is_valid_float_suffix(std::string_view suffix) {
    return suffix == "f32" || suffix == "f64";
}

} // namespace

auto Lexer::lex_number() -> Token {
    char c = peek();

    // Check for hex, binary, octal prefixes
    if (c == '0' && !is_at_end()) {
        char next = peek_next();
        if (next == 'x' || next == 'X') {
            return lex_hex_number();
        }
        if (next == 'b' || next == 'B') {
            return lex_binary_number();
        }
        if (next == 'o' || next == 'O') {
            return lex_octal_number();
        }
    }

    return lex_decimal_number();
}

auto Lexer::lex_hex_number() -> Token {
    // Skip 0x
    advance();
    advance();

    size_t digit_start = pos_;

    while (!is_at_end()) {
        char c = peek();
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') ||
            c == '_') {
            advance();
        } else {
            break;
        }
    }

    if (pos_ == digit_start) {
        return make_error_token("Expected hexadecimal digits after '0x'", "L008");
    }

    // Parse the value (ignoring underscores)
    std::string digits;
    auto lexeme = source_.slice(digit_start, pos_);
    for (char ch : lexeme) {
        if (ch != '_') {
            digits += ch;
        }
    }

    uint64_t value = 0;
    auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
    if (result.ec != std::errc{}) {
        return make_error_token("Invalid hexadecimal number", "L008");
    }

    // Check for type suffix
    std::string suffix;
    if (!is_at_end() && (peek() == 'i' || peek() == 'u')) {
        size_t suffix_start = pos_;
        while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
            advance();
        }
        suffix = std::string(source_.slice(suffix_start, pos_));
        if (!is_valid_int_suffix(suffix)) {
            return make_error_token("Invalid integer type suffix '" + suffix + "'", "L003");
        }
    }

    auto token = make_token(TokenKind::IntLiteral);
    token.value = IntValue{value, 16, suffix};
    return token;
}

auto Lexer::lex_binary_number() -> Token {
    // Skip 0b
    advance();
    advance();

    size_t digit_start = pos_;

    while (!is_at_end()) {
        char c = peek();
        if (c == '0' || c == '1' || c == '_') {
            advance();
        } else {
            break;
        }
    }

    if (pos_ == digit_start) {
        return make_error_token("Expected binary digits after '0b'", "L009");
    }

    // Parse the value
    std::string digits;
    auto lexeme = source_.slice(digit_start, pos_);
    for (char ch : lexeme) {
        if (ch != '_') {
            digits += ch;
        }
    }

    uint64_t value = 0;
    auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value, 2);
    if (result.ec != std::errc{}) {
        return make_error_token("Invalid binary number", "L009");
    }

    // Check for type suffix
    std::string suffix;
    if (!is_at_end() && (peek() == 'i' || peek() == 'u')) {
        size_t suffix_start = pos_;
        while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
            advance();
        }
        suffix = std::string(source_.slice(suffix_start, pos_));
        if (!is_valid_int_suffix(suffix)) {
            return make_error_token("Invalid integer type suffix '" + suffix + "'", "L003");
        }
    }

    auto token = make_token(TokenKind::IntLiteral);
    token.value = IntValue{value, 2, suffix};
    return token;
}

auto Lexer::lex_octal_number() -> Token {
    // Skip 0o
    advance();
    advance();

    size_t digit_start = pos_;

    while (!is_at_end()) {
        char c = peek();
        if ((c >= '0' && c <= '7') || c == '_') {
            advance();
        } else {
            break;
        }
    }

    if (pos_ == digit_start) {
        return make_error_token("Expected octal digits after '0o'", "L010");
    }

    // Parse the value
    std::string digits;
    auto lexeme = source_.slice(digit_start, pos_);
    for (char ch : lexeme) {
        if (ch != '_') {
            digits += ch;
        }
    }

    uint64_t value = 0;
    auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value, 8);
    if (result.ec != std::errc{}) {
        return make_error_token("Invalid octal number", "L010");
    }

    // Check for type suffix
    std::string suffix;
    if (!is_at_end() && (peek() == 'i' || peek() == 'u')) {
        size_t suffix_start = pos_;
        while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
            advance();
        }
        suffix = std::string(source_.slice(suffix_start, pos_));
        if (!is_valid_int_suffix(suffix)) {
            return make_error_token("Invalid integer type suffix '" + suffix + "'", "L003");
        }
    }

    auto token = make_token(TokenKind::IntLiteral);
    token.value = IntValue{value, 8, suffix};
    return token;
}

auto Lexer::lex_decimal_number() -> Token {
    bool has_dot = false;
    bool has_exp = false;

    // Integer part
    while (!is_at_end()) {
        char c = peek();
        if ((c >= '0' && c <= '9') || c == '_') {
            advance();
        } else {
            break;
        }
    }

    // Check for decimal point
    if (!is_at_end() && peek() == '.') {
        // Look ahead to distinguish from range operator (..)
        if (peek_next() >= '0' && peek_next() <= '9') {
            has_dot = true;
            advance(); // consume '.'

            // Fractional part
            while (!is_at_end()) {
                char c = peek();
                if ((c >= '0' && c <= '9') || c == '_') {
                    advance();
                } else {
                    break;
                }
            }
        }
    }

    // Check for exponent
    if (!is_at_end() && (peek() == 'e' || peek() == 'E')) {
        has_exp = true;
        advance();

        // Optional sign
        if (!is_at_end() && (peek() == '+' || peek() == '-')) {
            advance();
        }

        // Exponent digits
        size_t exp_start = pos_;
        while (!is_at_end()) {
            char c = peek();
            if ((c >= '0' && c <= '9') || c == '_') {
                advance();
            } else {
                break;
            }
        }

        if (pos_ == exp_start) {
            return make_error_token("Expected exponent digits", "L003");
        }
    }

    // Build the number string without underscores
    std::string digits;
    auto lexeme = source_.slice(token_start_, pos_);
    for (char ch : lexeme) {
        if (ch != '_') {
            digits += ch;
        }
    }

    if (has_dot || has_exp) {
        // Float literal
        // Note: std::from_chars for floats is not supported on macOS libc++
        // Use strtod instead for cross-platform compatibility
        char* end_ptr = nullptr;
        errno = 0;
        double value = std::strtod(digits.c_str(), &end_ptr);
        if (errno == ERANGE || end_ptr != digits.c_str() + digits.size()) {
            return make_error_token("Invalid floating-point number", "L003");
        }

        // Check for float type suffix
        std::string suffix;
        if (!is_at_end() && peek() == 'f') {
            size_t suffix_start = pos_;
            while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
                advance();
            }
            suffix = std::string(source_.slice(suffix_start, pos_));
            if (!is_valid_float_suffix(suffix)) {
                return make_error_token("Invalid float type suffix '" + suffix + "'", "L003");
            }
        }

        auto token = make_token(TokenKind::FloatLiteral);
        token.value = FloatValue{value, suffix};
        return token;
    } else {
        // Check for type suffix - could be int (i/u) or float (f)
        std::string suffix;
        if (!is_at_end() && (peek() == 'i' || peek() == 'u' || peek() == 'f')) {
            char first = peek();
            size_t suffix_start = pos_;
            while (!is_at_end() && (std::isalnum(peek()) || peek() == '_')) {
                advance();
            }
            suffix = std::string(source_.slice(suffix_start, pos_));

            if (first == 'f') {
                // This is a float with suffix (e.g., 42f32)
                if (!is_valid_float_suffix(suffix)) {
                    return make_error_token("Invalid float type suffix '" + suffix + "'", "L003");
                }
                // Parse as integer first then convert to float
                uint64_t int_value = 0;
                auto result =
                    std::from_chars(digits.data(), digits.data() + digits.size(), int_value, 10);
                if (result.ec != std::errc{}) {
                    return make_error_token("Invalid number", "L003");
                }

                auto token = make_token(TokenKind::FloatLiteral);
                token.value = FloatValue{static_cast<double>(int_value), suffix};
                return token;
            } else {
                // Integer suffix
                if (!is_valid_int_suffix(suffix)) {
                    return make_error_token("Invalid integer type suffix '" + suffix + "'", "L003");
                }
            }
        }

        // Integer literal
        uint64_t value = 0;
        auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value, 10);
        if (result.ec != std::errc{}) {
            return make_error_token("Invalid integer number", "L003");
        }

        auto token = make_token(TokenKind::IntLiteral);
        token.value = IntValue{value, 10, suffix};
        return token;
    }
}

} // namespace tml::lexer
