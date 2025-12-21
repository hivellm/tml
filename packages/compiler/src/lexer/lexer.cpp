#include "tml/lexer/lexer.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <unordered_map>

namespace tml::lexer {

namespace {

// Keyword lookup table
const std::unordered_map<std::string_view, TokenKind> KEYWORDS = {
    // Declarations
    {"func", TokenKind::KwFunc},
    {"type", TokenKind::KwType},
    {"trait", TokenKind::KwTrait},
    {"impl", TokenKind::KwImpl},
    {"mod", TokenKind::KwMod},
    {"use", TokenKind::KwUse},
    {"pub", TokenKind::KwPub},

    // Variables
    {"let", TokenKind::KwLet},
    {"var", TokenKind::KwVar},
    {"const", TokenKind::KwConst},

    // Control flow
    {"if", TokenKind::KwIf},
    {"else", TokenKind::KwElse},
    {"when", TokenKind::KwWhen},
    {"loop", TokenKind::KwLoop},
    {"while", TokenKind::KwWhile},
    {"for", TokenKind::KwFor},
    {"in", TokenKind::KwIn},
    {"break", TokenKind::KwBreak},
    {"continue", TokenKind::KwContinue},
    {"return", TokenKind::KwReturn},

    // Types
    {"self", TokenKind::KwSelf},
    {"Self", TokenKind::KwSelfType},
    {"as", TokenKind::KwAs},
    {"where", TokenKind::KwWhere},

    // Memory
    {"mut", TokenKind::KwMut},
    {"ref", TokenKind::KwRef},
    {"move", TokenKind::KwMove},
    {"copy", TokenKind::KwCopy},

    // Other
    {"async", TokenKind::KwAsync},
    {"await", TokenKind::KwAwait},
    {"caps", TokenKind::KwCaps},
    {"unsafe", TokenKind::KwUnsafe},
    {"extern", TokenKind::KwExtern},

    // Booleans (special - become BoolLiteral)
    {"true", TokenKind::BoolLiteral},
    {"false", TokenKind::BoolLiteral},
};

} // namespace

Lexer::Lexer(const Source& source) : source_(source) {}

auto Lexer::peek() const -> char {
    return source_.at(pos_);
}

auto Lexer::peek_next() const -> char {
    return source_.at(pos_ + 1);
}

auto Lexer::peek_n(size_t n) const -> char {
    return source_.at(pos_ + n);
}

auto Lexer::advance() -> char {
    char c = peek();
    ++pos_;
    return c;
}

auto Lexer::is_at_end() const -> bool {
    return pos_ >= source_.length();
}

auto Lexer::make_token(TokenKind kind) -> Token {
    auto start_loc = source_.location(token_start_);
    auto end_loc = source_.location(pos_ > 0 ? pos_ - 1 : 0);
    end_loc.length = static_cast<uint32_t>(pos_ - token_start_);

    return Token{
        .kind = kind,
        .span = {start_loc, end_loc},
        .lexeme = source_.slice(token_start_, pos_),
        .value = std::monostate{}
    };
}

auto Lexer::make_error_token(const std::string& message) -> Token {
    report_error(message);

    auto start_loc = source_.location(token_start_);
    auto end_loc = source_.location(pos_ > 0 ? pos_ - 1 : 0);

    return Token{
        .kind = TokenKind::Error,
        .span = {start_loc, end_loc},
        .lexeme = source_.slice(token_start_, pos_),
        .value = std::monostate{}
    };
}

void Lexer::report_error(const std::string& message) {
    auto start_loc = source_.location(token_start_);
    auto end_loc = source_.location(pos_);

    errors_.push_back(LexerError{
        .message = message,
        .span = {start_loc, end_loc}
    });
}

void Lexer::skip_whitespace() {
    while (!is_at_end()) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                advance();
                break;
            case '\n':
                // Newlines are significant in TML for statement separation
                return;
            case '/':
                if (peek_next() == '/') {
                    skip_line_comment();
                } else if (peek_next() == '*') {
                    skip_block_comment();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

void Lexer::skip_line_comment() {
    // Skip //
    advance();
    advance();

    while (!is_at_end() && peek() != '\n') {
        advance();
    }
}

void Lexer::skip_block_comment() {
    // Skip /*
    advance();
    advance();

    int depth = 1;
    while (!is_at_end() && depth > 0) {
        if (peek() == '/' && peek_next() == '*') {
            advance();
            advance();
            ++depth;
        } else if (peek() == '*' && peek_next() == '/') {
            advance();
            advance();
            --depth;
        } else {
            advance();
        }
    }

    if (depth > 0) {
        report_error("Unterminated block comment");
    }
}

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

    // Raw strings
    if (c == 'r' && peek_next() == '"') {
        return lex_raw_string();
    }

    // Characters
    if (c == '\'') {
        return lex_char();
    }

    // Operators and delimiters
    return lex_operator();
}

auto Lexer::lex_identifier() -> Token {
    while (!is_at_end() && is_identifier_continue(static_cast<char32_t>(peek()))) {
        advance();
    }

    auto lexeme = source_.slice(token_start_, pos_);

    // Check for keyword
    auto kw = lookup_keyword(lexeme);
    if (kw) {
        auto token = make_token(*kw);
        // Handle boolean literals
        if (*kw == TokenKind::BoolLiteral) {
            token.value = (lexeme == "true");
        }
        return token;
    }

    return make_token(TokenKind::Identifier);
}

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
        if ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F') ||
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
        double value = 0;
        auto result = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (result.ec != std::errc{}) {
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

auto Lexer::lex_operator() -> Token {
    char c = advance();

    switch (c) {
        // Single character tokens
        case '(': return make_token(TokenKind::LParen);
        case ')': return make_token(TokenKind::RParen);
        case '[': return make_token(TokenKind::LBracket);
        case ']': return make_token(TokenKind::RBracket);
        case '{': return make_token(TokenKind::LBrace);
        case '}': return make_token(TokenKind::RBrace);
        case ',': return make_token(TokenKind::Comma);
        case ';': return make_token(TokenKind::Semi);
        case '~': return make_token(TokenKind::BitNot);
        case '@': return make_token(TokenKind::At);
        case '#': return make_token(TokenKind::Hash);
        case '?': return make_token(TokenKind::Question);

        // Potentially multi-character tokens
        case '+':
            if (peek() == '=') { advance(); return make_token(TokenKind::PlusAssign); }
            return make_token(TokenKind::Plus);

        case '-':
            if (peek() == '=') { advance(); return make_token(TokenKind::MinusAssign); }
            if (peek() == '>') { advance(); return make_token(TokenKind::Arrow); }
            return make_token(TokenKind::Minus);

        case '*':
            if (peek() == '=') { advance(); return make_token(TokenKind::StarAssign); }
            return make_token(TokenKind::Star);

        case '/':
            if (peek() == '=') { advance(); return make_token(TokenKind::SlashAssign); }
            return make_token(TokenKind::Slash);

        case '%':
            if (peek() == '=') { advance(); return make_token(TokenKind::PercentAssign); }
            return make_token(TokenKind::Percent);

        case '=':
            if (peek() == '=') { advance(); return make_token(TokenKind::Eq); }
            if (peek() == '>') { advance(); return make_token(TokenKind::FatArrow); }
            return make_token(TokenKind::Assign);

        case '!':
            if (peek() == '=') { advance(); return make_token(TokenKind::Ne); }
            return make_token(TokenKind::Not);

        case '<':
            if (peek() == '=') { advance(); return make_token(TokenKind::Le); }
            if (peek() == '<') {
                advance();
                if (peek() == '=') { advance(); return make_token(TokenKind::ShlAssign); }
                return make_token(TokenKind::Shl);
            }
            return make_token(TokenKind::Lt);

        case '>':
            if (peek() == '=') { advance(); return make_token(TokenKind::Ge); }
            if (peek() == '>') {
                advance();
                if (peek() == '=') { advance(); return make_token(TokenKind::ShrAssign); }
                return make_token(TokenKind::Shr);
            }
            return make_token(TokenKind::Gt);

        case '&':
            if (peek() == '&') { advance(); return make_token(TokenKind::And); }
            if (peek() == '=') { advance(); return make_token(TokenKind::BitAndAssign); }
            return make_token(TokenKind::BitAnd);

        case '|':
            if (peek() == '|') { advance(); return make_token(TokenKind::Or); }
            if (peek() == '=') { advance(); return make_token(TokenKind::BitOrAssign); }
            return make_token(TokenKind::BitOr);

        case '^':
            if (peek() == '=') { advance(); return make_token(TokenKind::BitXorAssign); }
            return make_token(TokenKind::BitXor);

        case '.':
            if (peek() == '.') {
                advance();
                if (peek() == '=') { advance(); return make_token(TokenKind::DotDotEq); }
                return make_token(TokenKind::DotDot);
            }
            return make_token(TokenKind::Dot);

        case ':':
            if (peek() == ':') { advance(); return make_token(TokenKind::ColonColon); }
            return make_token(TokenKind::Colon);

        default:
            return make_error_token("Unexpected character: " + std::string(1, c));
    }
}

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
    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        c == '_') {
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
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

auto Lexer::lookup_keyword(std::string_view ident) -> std::optional<TokenKind> {
    auto it = KEYWORDS.find(ident);
    if (it != KEYWORDS.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace tml::lexer
