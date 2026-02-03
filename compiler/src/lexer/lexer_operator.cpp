//! # Lexer - Operators
//!
//! This file implements operator and punctuation lexing.
//!
//! ## Single-Character Tokens
//!
//! `( ) [ ] { } , ; ~ @`
//!
//! ## Multi-Character Operators
//!
//! | Operator | Variants                |
//! |----------|-------------------------|
//! | `+`      | `++`, `+=`              |
//! | `-`      | `--`, `-=`, `->`        |
//! | `*`      | `**`, `*=`              |
//! | `/`      | `/=`                    |
//! | `%`      | `%=`                    |
//! | `=`      | `==`, `=>`              |
//! | `!`      | `!=`                    |
//! | `<`      | `<=`, `<<`, `<<=`       |
//! | `>`      | `>=`, `>>`, `>>=`       |
//! | `&`      | `&&`, `&=`              |
//! | `\|`     | `\|\|`, `\|=`           |
//! | `^`      | `^=`                    |
//! | `.`      | `..`                    |
//! | `:`      | `::`                    |
//!
//! ## Interpolation Support
//!
//! When `}` is encountered inside an interpolated string (tracked by
//! `interp_depth_`), the lexer continues lexing the string instead of
//! returning a `RBrace` token.

#include "lexer/lexer.hpp"

namespace tml::lexer {

auto Lexer::lex_operator() -> Token {
    char c = advance();

    switch (c) {
    // Single character tokens
    case '(':
        return make_token(TokenKind::LParen);
    case ')':
        return make_token(TokenKind::RParen);
    case '[':
        return make_token(TokenKind::LBracket);
    case ']':
        return make_token(TokenKind::RBracket);
    case '{':
        return make_token(TokenKind::LBrace);
    case '}':
        // When inside an interpolated string, continue lexing the string
        if (interp_depth_ > 0) {
            return lex_interp_string_continue();
        }
        // When inside a template literal, continue lexing the template
        if (template_depth_ > 0) {
            return lex_template_literal_continue();
        }
        return make_token(TokenKind::RBrace);
    case ',':
        return make_token(TokenKind::Comma);
    case ';':
        return make_token(TokenKind::Semi);
    case '~':
        return make_token(TokenKind::BitNot);
    case '@':
        return make_token(TokenKind::At);

    case '$':
        if (peek() == '{') {
            advance();
            return make_token(TokenKind::DollarBrace);
        }
        return make_token(TokenKind::Dollar);

    // Potentially multi-character tokens
    case '+':
        if (peek() == '+') {
            advance();
            return make_token(TokenKind::PlusPlus);
        }
        if (peek() == '=') {
            advance();
            return make_token(TokenKind::PlusAssign);
        }
        return make_token(TokenKind::Plus);

    case '-':
        if (peek() == '-') {
            advance();
            return make_token(TokenKind::MinusMinus);
        }
        if (peek() == '=') {
            advance();
            return make_token(TokenKind::MinusAssign);
        }
        if (peek() == '>') {
            advance();
            return make_token(TokenKind::Arrow);
        }
        return make_token(TokenKind::Minus);

    case '*':
        if (peek() == '*') {
            advance();
            return make_token(TokenKind::StarStar);
        }
        if (peek() == '=') {
            advance();
            return make_token(TokenKind::StarAssign);
        }
        return make_token(TokenKind::Star);

    case '/':
        if (peek() == '=') {
            advance();
            return make_token(TokenKind::SlashAssign);
        }
        return make_token(TokenKind::Slash);

    case '%':
        if (peek() == '=') {
            advance();
            return make_token(TokenKind::PercentAssign);
        }
        return make_token(TokenKind::Percent);

    case '=':
        if (peek() == '=') {
            advance();
            return make_token(TokenKind::Eq);
        }
        if (peek() == '>') {
            advance();
            return make_token(TokenKind::FatArrow);
        }
        return make_token(TokenKind::Assign);

    case '!':
        if (peek() == '=') {
            advance();
            return make_token(TokenKind::Ne);
        }
        return make_token(TokenKind::Bang);

    case '<':
        if (peek() == '=') {
            advance();
            return make_token(TokenKind::Le);
        }
        if (peek() == '<') {
            advance();
            if (peek() == '=') {
                advance();
                return make_token(TokenKind::ShlAssign);
            }
            return make_token(TokenKind::Shl);
        }
        return make_token(TokenKind::Lt);

    case '>':
        if (peek() == '=') {
            advance();
            return make_token(TokenKind::Ge);
        }
        if (peek() == '>') {
            advance();
            if (peek() == '=') {
                advance();
                return make_token(TokenKind::ShrAssign);
            }
            return make_token(TokenKind::Shr);
        }
        return make_token(TokenKind::Gt);

    case '&':
        if (peek() == '&') {
            advance();
            return make_token(TokenKind::AndAnd);
        }
        if (peek() == '=') {
            advance();
            return make_token(TokenKind::BitAndAssign);
        }
        return make_token(TokenKind::BitAnd);

    case '|':
        if (peek() == '|') {
            advance();
            return make_token(TokenKind::OrOr);
        }
        if (peek() == '=') {
            advance();
            return make_token(TokenKind::BitOrAssign);
        }
        return make_token(TokenKind::BitOr);

    case '^':
        if (peek() == '=') {
            advance();
            return make_token(TokenKind::BitXorAssign);
        }
        return make_token(TokenKind::BitXor);

    case '.':
        if (peek() == '.') {
            advance();
            return make_token(TokenKind::DotDot);
        }
        return make_token(TokenKind::Dot);

    case ':':
        if (peek() == ':') {
            advance();
            return make_token(TokenKind::ColonColon);
        }
        return make_token(TokenKind::Colon);

    case '?':
        return make_token(TokenKind::Question);

    default:
        return make_error_token("Unexpected character: " + std::string(1, c), "L001");
    }
}

} // namespace tml::lexer
