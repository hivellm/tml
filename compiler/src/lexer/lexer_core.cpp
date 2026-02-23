TML_MODULE("compiler")

//! # Lexer Core
//!
//! This file implements core lexer functionality including:
//!
//! - **Keyword table**: Maps identifier text to token kinds
//! - **Character access**: `peek()`, `advance()`, `is_at_end()`
//! - **Token creation**: `make_token()`, `make_error_token()`
//! - **Comment handling**: Line (`//`) and block (`/* */`) comments
//!
//! ## Keyword Categories
//!
//! | Category     | Keywords                                    |
//! |--------------|---------------------------------------------|
//! | Declarations | `func`, `type`, `behavior`, `impl`, `mod`   |
//! | Variables    | `let`, `var`, `const`                       |
//! | Control flow | `if`, `else`, `when`, `loop`, `for`, `while`|
//! | Logical      | `and`, `or`, `not`                          |
//! | Memory       | `mut`, `ref`                                |

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
    {"enum", TokenKind::KwType}, // Alias for 'type' (enum declaration syntax)
    {"union", TokenKind::KwUnion},
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
    {"var", TokenKind::KwVar},
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

    // Bitwise operators (TML word aliases)
    {"xor", TokenKind::KwXor},
    {"shl", TokenKind::KwShl},
    {"shr", TokenKind::KwShr},

    // Types
    {"this", TokenKind::KwThis},
    {"This", TokenKind::KwThisType},
    {"as", TokenKind::KwAs},
    {"is", TokenKind::KwIs},

    // Memory
    {"mut", TokenKind::KwMut},
    {"ref", TokenKind::KwRef},
    {"life", TokenKind::KwLife},
    {"volatile", TokenKind::KwVolatile},

    // Closures
    {"do", TokenKind::KwDo},
    {"move", TokenKind::KwMove},

    // Other
    {"async", TokenKind::KwAsync},
    {"await", TokenKind::KwAwait},
    {"with", TokenKind::KwWith},
    {"where", TokenKind::KwWhere},
    {"dyn", TokenKind::KwDyn},
    {"lowlevel", TokenKind::KwLowlevel},
    {"unsafe", TokenKind::KwLowlevel}, // Alias for 'lowlevel' (Rust-style)
    {"quote", TokenKind::KwQuote},

    // Booleans (special - become BoolLiteral)
    {"true", TokenKind::BoolLiteral},
    {"false", TokenKind::BoolLiteral},

    // Null literal
    {"null", TokenKind::NullLiteral},

    // OOP (C#-style)
    {"class", TokenKind::KwClass},
    {"interface", TokenKind::KwInterface},
    {"extends", TokenKind::KwExtends},
    {"implements", TokenKind::KwImplements},
    {"override", TokenKind::KwOverride},
    {"virtual", TokenKind::KwVirtual},
    {"abstract", TokenKind::KwAbstract},
    {"sealed", TokenKind::KwSealed},
    {"namespace", TokenKind::KwNamespace},
    {"base", TokenKind::KwBase},
    {"protected", TokenKind::KwProtected},
    {"private", TokenKind::KwPrivate},
    {"static", TokenKind::KwStatic},
    {"prop", TokenKind::KwProp},
    {"throw", TokenKind::KwThrow},
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

auto Lexer::make_error_token(const std::string& message, const std::string& code) -> Token {
    report_error(message, code);

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

    errors_.push_back(LexerError{.message = message, .span = {start_loc, end_loc}, .code = ""});
}

void Lexer::report_error(const std::string& message, const std::string& code) {
    auto start_loc = source_.location(token_start_);
    auto end_loc = source_.location(pos_);

    errors_.push_back(LexerError{.message = message, .span = {start_loc, end_loc}, .code = code});
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
                // Check for doc comments - don't skip those
                if (is_doc_comment()) {
                    return;
                }
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

auto Lexer::is_doc_comment() const -> bool {
    // Check if we're at a doc comment: /// or //!
    // We must be at //, and the next char must be / or !
    // But //// is NOT a doc comment (4+ slashes)
    if (pos_ + 2 >= source_.length()) {
        return false;
    }
    if (peek() != '/' || peek_next() != '/') {
        return false;
    }
    char third = peek_n(2);
    if (third == '/' || third == '!') {
        // Check that it's not //// (4 slashes)
        if (third == '/' && pos_ + 3 < source_.length() && peek_n(3) == '/') {
            return false;
        }
        return true;
    }
    return false;
}

auto Lexer::lex_doc_comment() -> Token {
    token_start_ = pos_;

    // Determine the doc comment type
    advance();               // consume first /
    advance();               // consume second /
    char marker = advance(); // consume / or !

    TokenKind kind = (marker == '!') ? TokenKind::ModuleDocComment : TokenKind::DocComment;

    // Collect content from this line (skip leading space if present)
    std::string content;

    // Skip one leading space if present (common formatting)
    if (!is_at_end() && peek() == ' ') {
        advance();
    }

    // Read until end of line
    while (!is_at_end() && peek() != '\n') {
        content += advance();
    }

    // For consecutive doc comment lines, we merge them
    // Look ahead to see if the next non-empty line is also a doc comment
    size_t saved_pos = pos_;
    while (!is_at_end()) {
        // Skip the newline
        if (peek() == '\n') {
            advance();
        }

        // Skip whitespace (spaces and tabs only)
        while (!is_at_end() && (peek() == ' ' || peek() == '\t')) {
            advance();
        }

        // Check if this line is a continuation doc comment of the same type
        if (!is_at_end() && peek() == '/' && peek_next() == '/') {
            char next_marker = peek_n(2);
            // Must be same type (/// or //!) and not ////
            if (next_marker == marker) {
                // Check for //// (not a doc comment)
                if (marker == '/' && pos_ + 3 < source_.length() && peek_n(3) == '/') {
                    break;
                }
                // This is a continuation - consume it
                advance(); // /
                advance(); // /
                advance(); // / or !

                // Add newline to content
                content += '\n';

                // Skip one leading space if present
                if (!is_at_end() && peek() == ' ') {
                    advance();
                }

                // Read line content
                while (!is_at_end() && peek() != '\n') {
                    content += advance();
                }
                saved_pos = pos_;
            } else {
                // Different doc comment type or regular comment - stop
                break;
            }
        } else {
            // Not a doc comment continuation - restore position
            pos_ = saved_pos;
            break;
        }
    }

    // Create token
    auto start_loc = source_.location(token_start_);
    auto end_loc = source_.location(pos_ > 0 ? pos_ - 1 : 0);
    end_loc.length = static_cast<uint32_t>(pos_ - token_start_);

    return Token{.kind = kind,
                 .span = {start_loc, end_loc},
                 .lexeme = source_.slice(token_start_, pos_),
                 .value = DocValue{.content = std::move(content)}};
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
        report_error("Unterminated block comment", "L012");
    }
}

} // namespace tml::lexer
