#include "lexer/lexer.hpp"

#include <algorithm>

namespace tml::lexer {

auto Lexer::tokenize() -> std::vector<Token> {
    std::vector<Token> tokens;

    while (true) {
        auto token = next_token();
        tokens.push_back(token);
        if (token.is_eof()) {
            break;
        }
    }

    return tokens;
}

auto Lexer::is_identifier_start(char32_t c) -> bool {
    // ASCII letters and underscore
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        return true;
    }

    // Unicode XID_Start (simplified - allow letters from other scripts)
    // Full Unicode support would use ICU or similar
    if (c > 127) {
        // Basic Latin-1 Supplement letters
        if (c >= 0xC0 && c <= 0xFF && c != 0xD7 && c != 0xF7) {
            return true;
        }
        // General Unicode letter categories (simplified)
        if (c >= 0x100) {
            return true; // Allow extended Unicode for now
        }
    }

    return false;
}

auto Lexer::is_identifier_continue(char32_t c) -> bool {
    if (is_identifier_start(c)) {
        return true;
    }

    // ASCII digits
    if (c >= '0' && c <= '9') {
        return true;
    }

    // Unicode combining marks, etc. (simplified)
    if (c >= 0x300 && c <= 0x36F) {
        return true; // Combining diacritical marks
    }

    return false;
}

auto Lexer::decode_utf8() -> char32_t {
    char c = advance();

    if ((c & 0x80) == 0) {
        return static_cast<char32_t>(c);
    }

    char32_t result;
    size_t remaining;

    if ((c & 0xE0) == 0xC0) {
        result = c & 0x1F;
        remaining = 1;
    } else if ((c & 0xF0) == 0xE0) {
        result = c & 0x0F;
        remaining = 2;
    } else if ((c & 0xF8) == 0xF0) {
        result = c & 0x07;
        remaining = 3;
    } else {
        return 0xFFFD; // Replacement character
    }

    for (size_t i = 0; i < remaining && !is_at_end(); ++i) {
        c = advance();
        if ((c & 0xC0) != 0x80) {
            return 0xFFFD;
        }
        result = (result << 6) | (c & 0x3F);
    }

    return result;
}

auto Lexer::utf8_char_length(char c) const -> size_t {
    if ((c & 0x80) == 0)
        return 1;
    if ((c & 0xE0) == 0xC0)
        return 2;
    if ((c & 0xF0) == 0xE0)
        return 3;
    if ((c & 0xF8) == 0xF0)
        return 4;
    return 1;
}

} // namespace tml::lexer
