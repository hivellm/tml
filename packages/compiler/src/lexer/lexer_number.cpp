#include "lexer/lexer.hpp"

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>

namespace tml::lexer {

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
        return make_error_token("Expected hexadecimal digits after '0x'");
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
        return make_error_token("Invalid hexadecimal number");
    }

    auto token = make_token(TokenKind::IntLiteral);
    token.value = IntValue{value, 16};
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
        return make_error_token("Expected binary digits after '0b'");
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
        return make_error_token("Invalid binary number");
    }

    auto token = make_token(TokenKind::IntLiteral);
    token.value = IntValue{value, 2};
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
        return make_error_token("Expected octal digits after '0o'");
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
        return make_error_token("Invalid octal number");
    }

    auto token = make_token(TokenKind::IntLiteral);
    token.value = IntValue{value, 8};
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
            return make_error_token("Expected exponent digits");
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
            return make_error_token("Invalid floating-point number");
        }

        auto token = make_token(TokenKind::FloatLiteral);
        token.value = FloatValue{value};
        return token;
    } else {
        // Integer literal
        uint64_t value = 0;
        auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value, 10);
        if (result.ec != std::errc{}) {
            return make_error_token("Invalid integer number");
        }

        auto token = make_token(TokenKind::IntLiteral);
        token.value = IntValue{value, 10};
        return token;
    }
}

} // namespace tml::lexer
