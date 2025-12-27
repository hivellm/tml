#include "lexer/lexer.hpp"

#include <algorithm>
#include <unordered_map>

namespace tml::lexer {

namespace {

// Keyword lookup table - TML keywords
const std::unordered_map<std::string_view, TokenKind> KEYWORDS = {
    // Declarations
    {"func", TokenKind::KwFunc},
    {"type", TokenKind::KwType},
    {"behavior", TokenKind::KwBehavior},
    {"impl", TokenKind::KwImpl},
    {"mod", TokenKind::KwMod},
    {"use", TokenKind::KwUse},
    {"pub", TokenKind::KwPub},
    {"decorator", TokenKind::KwDecorator},
    {"crate", TokenKind::KwCrate},
    {"super", TokenKind::KwSuper},

    // Variables
    {"let", TokenKind::KwLet},
    {"const", TokenKind::KwConst},

    // Control flow
    {"if", TokenKind::KwIf},
    {"then", TokenKind::KwThen},
    {"else", TokenKind::KwElse},
    {"when", TokenKind::KwWhen},
    {"loop", TokenKind::KwLoop},
    {"while", TokenKind::KwWhile},
    {"for", TokenKind::KwFor},
    {"in", TokenKind::KwIn},
    {"to", TokenKind::KwTo},
    {"through", TokenKind::KwThrough},
    {"break", TokenKind::KwBreak},
    {"continue", TokenKind::KwContinue},
    {"return", TokenKind::KwReturn},

    // Logical operators (TML uses words)
    {"and", TokenKind::KwAnd},
    {"or", TokenKind::KwOr},
    {"not", TokenKind::KwNot},

    // Types
    {"this", TokenKind::KwThis},
    {"This", TokenKind::KwThisType},
    {"as", TokenKind::KwAs},

    // Memory
    {"mut", TokenKind::KwMut},
    {"ref", TokenKind::KwRef},

    // Closures
    {"do", TokenKind::KwDo},

    // Other
    {"async", TokenKind::KwAsync},
    {"await", TokenKind::KwAwait},
    {"with", TokenKind::KwWith},
    {"where", TokenKind::KwWhere},
    {"dyn", TokenKind::KwDyn},
    {"lowlevel", TokenKind::KwLowlevel},
    {"quote", TokenKind::KwQuote},

    // Booleans (special - become BoolLiteral)
    {"true", TokenKind::BoolLiteral},
    {"false", TokenKind::BoolLiteral},
};

} // anonymous namespace

// Accessor function for keywords table
const std::unordered_map<std::string_view, TokenKind>& get_keywords() {
    return KEYWORDS;
}

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

    return Token{.kind = kind,
                 .span = {start_loc, end_loc},
                 .lexeme = source_.slice(token_start_, pos_),
                 .value = std::monostate{}};
}

auto Lexer::make_error_token(const std::string& message) -> Token {
    report_error(message);

    auto start_loc = source_.location(token_start_);
    auto end_loc = source_.location(pos_ > 0 ? pos_ - 1 : 0);

    return Token{.kind = TokenKind::Error,
                 .span = {start_loc, end_loc},
                 .lexeme = source_.slice(token_start_, pos_),
                 .value = std::monostate{}};
}

void Lexer::report_error(const std::string& message) {
    auto start_loc = source_.location(token_start_);
    auto end_loc = source_.location(pos_);

    errors_.push_back(LexerError{.message = message, .span = {start_loc, end_loc}});
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

} // namespace tml::lexer
