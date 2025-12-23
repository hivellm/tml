#ifndef TML_LEXER_TOKEN_HPP
#define TML_LEXER_TOKEN_HPP

#include "tml/common.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace tml::lexer {

// Token kinds - all TML tokens
enum class TokenKind : uint8_t {
    // End of file
    Eof,

    // Literals
    IntLiteral,      // 42, 0xFF, 0b1010, 0o755, 1_000_000
    FloatLiteral,    // 3.14, 1e10, 2.5e-3
    StringLiteral,   // "hello", "line\nbreak"
    CharLiteral,     // 'a', '\n', '\u{1F600}'
    BoolLiteral,     // true, false

    // Identifiers
    Identifier,      // foo, _bar, cafe

    // Keywords - declarations
    KwFunc,          // func
    KwType,          // type
    KwBehavior,      // behavior (trait in Rust)
    KwImpl,          // impl
    KwMod,           // mod
    KwUse,           // use
    KwPub,           // pub
    KwDecorator,     // decorator
    KwCrate,         // crate
    KwSuper,         // super

    // Keywords - variables
    KwLet,           // let
    KwConst,         // const

    // Keywords - control flow
    KwIf,            // if
    KwThen,          // then
    KwElse,          // else
    KwWhen,          // when (match)
    KwLoop,          // loop
    KwWhile,         // while
    KwFor,           // for
    KwIn,            // in
    KwTo,            // to (range exclusive)
    KwThrough,       // through (range inclusive)
    KwBreak,         // break
    KwContinue,      // continue
    KwReturn,        // return

    // Keywords - logical operators (TML uses words, not symbols)
    KwAnd,           // and
    KwOr,            // or
    KwNot,           // not

    // Keywords - types
    KwThis,          // this (self value)
    KwThisType,      // This (Self type)
    KwAs,            // as

    // Keywords - memory
    KwMut,           // mut
    KwRef,           // ref

    // Keywords - closures
    KwDo,            // do (closure)

    // Keywords - other
    KwAsync,         // async
    KwAwait,         // await
    KwWith,          // with (effects)
    KwWhere,         // where (generic constraints)
    KwLowlevel,      // lowlevel (unsafe in Rust)
    KwQuote,         // quote (metaprogramming)

    // Operators - arithmetic
    Plus,            // +
    Minus,           // -
    Star,            // *
    Slash,           // /
    Percent,         // %
    StarStar,        // ** (power)
    PlusPlus,        // ++ (increment)
    MinusMinus,      // -- (decrement)

    // Operators - comparison
    Eq,              // ==
    Ne,              // !=
    Lt,              // <
    Gt,              // >
    Le,              // <=
    Ge,              // >=

    // Operators - bitwise
    BitAnd,          // &
    BitOr,           // |
    BitXor,          // ^
    BitNot,          // ~
    Shl,             // <<
    Shr,             // >>

    // Operators - assignment
    Assign,          // =
    PlusAssign,      // +=
    MinusAssign,     // -=
    StarAssign,      // *=
    SlashAssign,     // /=
    PercentAssign,   // %=
    BitAndAssign,    // &=
    BitOrAssign,     // |=
    BitXorAssign,    // ^=
    ShlAssign,       // <<=
    ShrAssign,       // >>=

    // Operators - other
    Arrow,           // ->
    FatArrow,        // =>
    Dot,             // .
    DotDot,          // .. (also via 'to' keyword)
    Colon,           // :
    ColonColon,      // ::
    Question,        // ? (ternary operator)
    Bang,            // ! (error propagation)
    At,              // @
    Pipe,            // | (in patterns)
    Dollar,          // $ (splice in quote)
    DollarBrace,     // ${ (splice block)

    // Delimiters
    LParen,          // (
    RParen,          // )
    LBracket,        // [
    RBracket,        // ]
    LBrace,          // {
    RBrace,          // }
    Comma,           // ,
    Semi,            // ;

    // Special
    Newline,         // significant newlines (for statement separation)
    Error,           // lexer error token
};

// Convert token kind to string for display
[[nodiscard]] auto token_kind_to_string(TokenKind kind) -> std::string_view;

// Check if token kind is a keyword
[[nodiscard]] auto is_keyword(TokenKind kind) -> bool;

// Check if token kind is a literal
[[nodiscard]] auto is_literal(TokenKind kind) -> bool;

// Check if token kind is an operator
[[nodiscard]] auto is_operator(TokenKind kind) -> bool;

// Integer literal value with original base
struct IntValue {
    uint64_t value;
    uint8_t base; // 2, 8, 10, or 16
};

// Float literal value
struct FloatValue {
    double value;
};

// String literal value (already unescaped)
struct StringValue {
    std::string value;
    bool is_raw; // r"..." raw string
};

// Char literal value
struct CharValue {
    char32_t value;
};

// Token - a lexical unit from source code
struct Token {
    TokenKind kind;
    SourceSpan span;
    std::string_view lexeme; // Raw text from source

    // Literal value (if applicable)
    std::variant<
        std::monostate,  // No value (keywords, operators, etc.)
        IntValue,
        FloatValue,
        StringValue,
        CharValue,
        bool             // BoolLiteral
    > value;

    [[nodiscard]] auto is(TokenKind k) const -> bool { return kind == k; }

    [[nodiscard]] auto is_one_of(std::initializer_list<TokenKind> kinds) const -> bool {
        for (auto k : kinds) {
            if (kind == k) return true;
        }
        return false;
    }

    [[nodiscard]] auto is_eof() const -> bool { return kind == TokenKind::Eof; }
    [[nodiscard]] auto is_error() const -> bool { return kind == TokenKind::Error; }

    // Get typed value (asserts correct type)
    [[nodiscard]] auto int_value() const -> const IntValue&;
    [[nodiscard]] auto float_value() const -> const FloatValue&;
    [[nodiscard]] auto string_value() const -> const StringValue&;
    [[nodiscard]] auto char_value() const -> const CharValue&;
    [[nodiscard]] auto bool_value() const -> bool;
};

} // namespace tml::lexer

#endif // TML_LEXER_TOKEN_HPP
