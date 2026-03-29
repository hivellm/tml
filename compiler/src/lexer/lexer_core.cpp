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
#include "simd/simd_utils.h"

#include <algorithm>
#include <unordered_map>

#if TML_SSE2
#include <emmintrin.h>
#ifdef _MSC_VER
#include <intrin.h> // _BitScanForward
#else
#include <x86intrin.h>
#endif
#endif

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
    const char* src = source_.content().data();
    size_t len = source_.length();

#if TML_SSE2
    // SSE2 fast path: scan 16 bytes at a time for non-whitespace.
    // We only skip ' ', '\t', '\r' — NOT '\n' (significant token).
    // We also need to stop at '/' (potential comment start).
    while (pos_ + 16 <= len) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + pos_));
        __m128i cmp_space = _mm_cmpeq_epi8(chunk, _mm_set1_epi8(' '));
        __m128i cmp_tab = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\t'));
        __m128i cmp_cr = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\r'));
        __m128i ws = _mm_or_si128(_mm_or_si128(cmp_space, cmp_tab), cmp_cr);
        int mask = _mm_movemask_epi8(ws);

        if (mask == 0xFFFF) {
            // All 16 bytes are whitespace — skip entire chunk
            pos_ += 16;
            continue;
        }

        // Find first non-whitespace byte
        int first_non_ws;
#ifdef _MSC_VER
        unsigned long idx;
        _BitScanForward(&idx, static_cast<unsigned long>(~mask & 0xFFFF));
        first_non_ws = static_cast<int>(idx);
#else
        first_non_ws = __builtin_ctz(~mask & 0xFFFF);
#endif
        pos_ += first_non_ws;
        // Fall through to scalar loop for comment detection
        goto scalar_tail;
    }
scalar_tail:
#endif // TML_SSE2

    // Scalar path: handles remaining bytes, comment detection, and newlines
    while (pos_ < len) {
        char c = src[pos_];
        switch (c) {
        case ' ':
        case '\t':
        case '\r':
            ++pos_;
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

    // Read until end of line — SIMD bulk append
    {
        const char* src = source_.content().data();
        size_t len = source_.length();
        size_t line_start = pos_;
#if TML_SSE2
        const __m128i nl_vec = _mm_set1_epi8('\n');
        while (pos_ + 16 <= len) {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + pos_));
            int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, nl_vec));
            if (mask != 0) {
#ifdef _MSC_VER
                unsigned long idx;
                _BitScanForward(&idx, static_cast<unsigned long>(mask));
                pos_ += idx;
#else
                pos_ += __builtin_ctz(mask);
#endif
                break;
            }
            pos_ += 16;
        }
#endif
        // Scalar tail for remaining bytes
        while (pos_ < len && src[pos_] != '\n') {
            ++pos_;
        }
        content.append(src + line_start, pos_ - line_start);
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

                // Read line content — SIMD bulk append
                {
                    const char* s = source_.content().data();
                    size_t l = source_.length();
                    size_t ls = pos_;
#if TML_SSE2
                    const __m128i nlv = _mm_set1_epi8('\n');
                    while (pos_ + 16 <= l) {
                        __m128i ch = _mm_loadu_si128(reinterpret_cast<const __m128i*>(s + pos_));
                        int m = _mm_movemask_epi8(_mm_cmpeq_epi8(ch, nlv));
                        if (m != 0) {
#ifdef _MSC_VER
                            unsigned long ix;
                            _BitScanForward(&ix, static_cast<unsigned long>(m));
                            pos_ += ix;
#else
                            pos_ += __builtin_ctz(m);
#endif
                            break;
                        }
                        pos_ += 16;
                    }
#endif
                    while (pos_ < l && s[pos_] != '\n') {
                        ++pos_;
                    }
                    content.append(s + ls, pos_ - ls);
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
    pos_ += 2;

    const char* src = source_.content().data();
    size_t len = source_.length();

#if TML_SSE2
    // SSE2: scan 16 bytes at a time for '\n'
    const __m128i nl_vec = _mm_set1_epi8('\n');
    while (pos_ + 16 <= len) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + pos_));
        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, nl_vec));
        if (mask != 0) {
            // Found newline — advance to it but don't consume it
#ifdef _MSC_VER
            unsigned long idx;
            _BitScanForward(&idx, static_cast<unsigned long>(mask));
            pos_ += idx;
#else
            pos_ += __builtin_ctz(mask);
#endif
            return;
        }
        pos_ += 16;
    }
#endif

    // Scalar tail
    while (pos_ < len && src[pos_] != '\n') {
        ++pos_;
    }
}

void Lexer::skip_block_comment() {
    // Skip /*
    pos_ += 2;

    const char* src = source_.content().data();
    size_t len = source_.length();

    int depth = 1;

#if TML_SSE2
    // SSE2: scan for '*' or '/' in 16-byte chunks, then check pairs at hit positions
    const __m128i star_vec = _mm_set1_epi8('*');
    const __m128i slash_vec = _mm_set1_epi8('/');

    while (depth > 0 && pos_ + 16 <= len) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + pos_));
        __m128i cmp_star = _mm_cmpeq_epi8(chunk, star_vec);
        __m128i cmp_slash = _mm_cmpeq_epi8(chunk, slash_vec);
        int mask = _mm_movemask_epi8(_mm_or_si128(cmp_star, cmp_slash));

        if (mask == 0) {
            // No '*' or '/' in this chunk — skip all 16 bytes
            pos_ += 16;
            continue;
        }

        // Process hits one by one within this chunk
        size_t chunk_end = pos_ + 16;
        while (pos_ < chunk_end && pos_ < len && depth > 0) {
            char c = src[pos_];
            if (c == '/' && pos_ + 1 < len && src[pos_ + 1] == '*') {
                pos_ += 2;
                ++depth;
            } else if (c == '*' && pos_ + 1 < len && src[pos_ + 1] == '/') {
                pos_ += 2;
                --depth;
            } else {
                ++pos_;
            }
        }
    }
#endif

    // Scalar tail
    while (pos_ < len && depth > 0) {
        if (src[pos_] == '/' && pos_ + 1 < len && src[pos_ + 1] == '*') {
            pos_ += 2;
            ++depth;
        } else if (src[pos_] == '*' && pos_ + 1 < len && src[pos_ + 1] == '/') {
            pos_ += 2;
            --depth;
        } else {
            ++pos_;
        }
    }

    if (depth > 0) {
        report_error("Unterminated block comment", "L012");
    }
}

} // namespace tml::lexer
