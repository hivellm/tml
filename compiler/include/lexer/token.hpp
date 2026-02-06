//! # Token Definitions
//!
//! This module defines the token types produced by the TML lexer.
//!
//! ## Overview
//!
//! TML tokens are categorized into:
//!
//! - **Literals**: Numbers, strings, characters, booleans, null
//! - **Keywords**: Reserved words like `func`, `let`, `if`, `when`
//! - **Operators**: Arithmetic, comparison, bitwise, logical, assignment
//! - **Delimiters**: Parentheses, brackets, braces, punctuation
//! - **Special**: End-of-file, newlines, error tokens
//!
//! ## TML-Specific Design
//!
//! TML uses word-based logical operators for clarity:
//! - `and`, `or`, `not` instead of `&&`, `||`, `!`
//! - `xor`, `shl`, `shr` for bitwise operations
//!
//! TML also uses `when` instead of `match` and `behavior` instead of `trait`.
//!
//! ## String Interpolation
//!
//! TML supports string interpolation with `{expr}` syntax:
//! ```tml
//! let greeting = "Hello {name}!"
//! ```
//! This produces `InterpStringStart`, expression tokens, then `InterpStringEnd`.

#ifndef TML_LEXER_TOKEN_HPP
#define TML_LEXER_TOKEN_HPP

#include "common.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace tml::lexer {

/// All possible token kinds in TML.
///
/// Each variant represents a distinct lexical element that can appear
/// in TML source code.
enum class TokenKind : uint8_t {
    // ========================================================================
    // End of File
    // ========================================================================
    Eof, ///< End of input stream

    // ========================================================================
    // Literals
    // ========================================================================
    IntLiteral,    ///< Integer: `42`, `0xFF`, `0b1010`, `0o755`, `1_000_000`
    FloatLiteral,  ///< Float: `3.14`, `1e10`, `2.5e-3`
    StringLiteral, ///< String: `"hello"`, `"line\nbreak"`
    CharLiteral,   ///< Character: `'a'`, `'\n'`, `'\u{1F600}'`
    BoolLiteral,   ///< Boolean: `true`, `false`
    NullLiteral,   ///< Null: `null`

    // ========================================================================
    // Interpolated Strings
    // ========================================================================
    InterpStringStart,  ///< Start of interpolated string: `"Hello {`
    InterpStringMiddle, ///< Middle of interpolated string: `} text {`
    InterpStringEnd,    ///< End of interpolated string: `} world"`

    // ========================================================================
    // Template Literals (produce Text type)
    // ========================================================================
    TemplateLiteralStart,  ///< Start of template literal: `` `Hello { ``
    TemplateLiteralMiddle, ///< Middle of template literal: `} text {`
    TemplateLiteralEnd,    ///< End of template literal: `` } world` ``

    // ========================================================================
    // Identifiers
    // ========================================================================
    Identifier, ///< User identifier: `foo`, `_bar`, `café`

    // ========================================================================
    // Keywords - Declarations
    // ========================================================================
    KwFunc,      ///< `func` - function declaration
    KwType,      ///< `type` - type/struct declaration
    KwUnion,     ///< `union` - union declaration (C-style)
    KwBehavior,  ///< `behavior` - trait declaration (like Rust's `trait`)
    KwImpl,      ///< `impl` - implementation block
    KwMod,       ///< `mod` - module declaration
    KwUse,       ///< `use` - import statement
    KwPub,       ///< `pub` - public visibility
    KwDecorator, ///< `decorator` - decorator definition
    KwCrate,     ///< `crate` - crate root reference
    KwSuper,     ///< `super` - parent module reference

    // ========================================================================
    // Keywords - Variables
    // ========================================================================
    KwLet,   ///< `let` - immutable binding
    KwVar,   ///< `var` - mutable binding (alias for `let mut`)
    KwConst, ///< `const` - compile-time constant

    // ========================================================================
    // Keywords - Control Flow
    // ========================================================================
    KwIf,       ///< `if` - conditional expression
    KwThen,     ///< `then` - optional if-then syntax
    KwElse,     ///< `else` - else branch
    KwWhen,     ///< `when` - pattern matching (like Rust's `match`)
    KwLoop,     ///< `loop` - infinite loop
    KwWhile,    ///< `while` - conditional loop
    KwFor,      ///< `for` - iterator loop
    KwIn,       ///< `in` - iterator binding
    KwTo,       ///< `to` - exclusive range (`1 to 5` = `1..5`)
    KwThrough,  ///< `through` - inclusive range (`1 through 5` = `1..=5`)
    KwBreak,    ///< `break` - exit loop
    KwContinue, ///< `continue` - next iteration
    KwReturn,   ///< `return` - return from function

    // ========================================================================
    // Keywords - Logical Operators
    // ========================================================================
    KwAnd, ///< `and` - logical AND (preferred over `&&`)
    KwOr,  ///< `or` - logical OR (preferred over `||`)
    KwNot, ///< `not` - logical NOT (preferred over `!`)

    // ========================================================================
    // Keywords - Bitwise Operators
    // ========================================================================
    KwXor, ///< `xor` - bitwise XOR (alias for `^`)
    KwShl, ///< `shl` - shift left (alias for `<<`)
    KwShr, ///< `shr` - shift right (alias for `>>`)

    // ========================================================================
    // Keywords - Types
    // ========================================================================
    KwThis,     ///< `this` - self value in methods
    KwThisType, ///< `This` - self type (like Rust's `Self`)
    KwAs,       ///< `as` - type cast
    KwIs,       ///< `is` - type check (e.g., `obj is Dog`)

    // ========================================================================
    // Keywords - Memory
    // ========================================================================
    KwMut,      ///< `mut` - mutable modifier
    KwRef,      ///< `ref` - reference/borrow
    KwLife,     ///< `life` - lifetime parameter (e.g., `func foo[life a](x: ref[a] T)`)
    KwVolatile, ///< `volatile` - prevent optimization (for benchmarks, hardware)

    // ========================================================================
    // Keywords - Closures
    // ========================================================================
    KwDo,   ///< `do` - closure syntax: `do(x) x + 1`
    KwMove, ///< `move` - move closure: `move do(x) x + 1`

    // ========================================================================
    // Keywords - Other
    // ========================================================================
    KwAsync,    ///< `async` - async function/block
    KwAwait,    ///< `await` - await expression
    KwWith,     ///< `with` - effect handlers
    KwWhere,    ///< `where` - generic constraints
    KwDyn,      ///< `dyn` - trait objects
    KwLowlevel, ///< `lowlevel` - unsafe block (clearer than `unsafe`)
    KwQuote,    ///< `quote` - metaprogramming/macros

    // ========================================================================
    // Keywords - OOP (C#-style)
    // ========================================================================
    KwClass,      ///< `class` - class declaration
    KwInterface,  ///< `interface` - interface declaration
    KwExtends,    ///< `extends` - class inheritance
    KwImplements, ///< `implements` - interface implementation
    KwOverride,   ///< `override` - override virtual method
    KwVirtual,    ///< `virtual` - declare virtual method
    KwAbstract,   ///< `abstract` - abstract class/method
    KwSealed,     ///< `sealed` - prevent inheritance
    KwNamespace,  ///< `namespace` - namespace declaration
    KwBase,       ///< `base` - parent class reference
    KwProtected,  ///< `protected` - protected visibility
    KwPrivate,    ///< `private` - private visibility
    KwStatic,     ///< `static` - static member
    KwNew,        ///< `new` - constructor/object creation
    KwProp,       ///< `prop` - property declaration
    KwThrow,      ///< `throw` - throw exception/error

    // ========================================================================
    // Operators - Arithmetic
    // ========================================================================
    Plus,       ///< `+` addition
    Minus,      ///< `-` subtraction
    Star,       ///< `*` multiplication
    Slash,      ///< `/` division
    Percent,    ///< `%` remainder
    StarStar,   ///< `**` exponentiation
    PlusPlus,   ///< `++` increment
    MinusMinus, ///< `--` decrement

    // ========================================================================
    // Operators - Comparison
    // ========================================================================
    Eq, ///< `==` equality
    Ne, ///< `!=` inequality
    Lt, ///< `<` less than
    Gt, ///< `>` greater than
    Le, ///< `<=` less than or equal
    Ge, ///< `>=` greater than or equal

    // ========================================================================
    // Operators - Bitwise
    // ========================================================================
    BitAnd, ///< `&` bitwise AND
    BitOr,  ///< `|` bitwise OR
    BitXor, ///< `^` bitwise XOR
    BitNot, ///< `~` bitwise NOT
    Shl,    ///< `<<` shift left
    Shr,    ///< `>>` shift right

    // ========================================================================
    // Operators - Logical Symbols
    // ========================================================================
    AndAnd, ///< `&&` logical AND (prefer `and` keyword)
    OrOr,   ///< `||` logical OR (prefer `or` keyword)

    // ========================================================================
    // Operators - Assignment
    // ========================================================================
    Assign,        ///< `=` assignment
    PlusAssign,    ///< `+=` add-assign
    MinusAssign,   ///< `-=` subtract-assign
    StarAssign,    ///< `*=` multiply-assign
    SlashAssign,   ///< `/=` divide-assign
    PercentAssign, ///< `%=` remainder-assign
    BitAndAssign,  ///< `&=` bitwise AND-assign
    BitOrAssign,   ///< `|=` bitwise OR-assign
    BitXorAssign,  ///< `^=` bitwise XOR-assign
    ShlAssign,     ///< `<<=` shift left-assign
    ShrAssign,     ///< `>>=` shift right-assign

    // ========================================================================
    // Operators - Other
    // ========================================================================
    Arrow,       ///< `->` return type annotation
    FatArrow,    ///< `=>` pattern arm / closure body
    Dot,         ///< `.` member access
    DotDot,      ///< `..` range (prefer `to` keyword)
    Colon,       ///< `:` type annotation
    ColonColon,  ///< `::` path separator
    Question,    ///< `?` error propagation / ternary
    Bang,        ///< `!` logical NOT / unwrap
    At,          ///< `@` attributes/decorators
    Pipe,        ///< `|` pattern alternation
    Dollar,      ///< `$` metaprogramming splice
    DollarBrace, ///< `${` splice block start

    // ========================================================================
    // Delimiters
    // ========================================================================
    LParen,   ///< `(` left parenthesis
    RParen,   ///< `)` right parenthesis
    LBracket, ///< `[` left bracket (generics, arrays)
    RBracket, ///< `]` right bracket
    LBrace,   ///< `{` left brace (blocks)
    RBrace,   ///< `}` right brace
    Comma,    ///< `,` comma separator
    Semi,     ///< `;` semicolon

    // ========================================================================
    // Special
    // ========================================================================
    Newline, ///< Significant newline (statement separator)
    Error,   ///< Lexer error token

    // ========================================================================
    // Documentation Comments
    // ========================================================================
    DocComment,       ///< `///` documentation comment for following item
    ModuleDocComment, ///< `//!` module-level documentation comment
};

// ============================================================================
// Token Utilities
// ============================================================================

/// Converts a token kind to its string representation for display.
[[nodiscard]] auto token_kind_to_string(TokenKind kind) -> std::string_view;

/// Checks if a token kind is a keyword.
[[nodiscard]] auto is_keyword(TokenKind kind) -> bool;

/// Checks if a token kind is a literal.
[[nodiscard]] auto is_literal(TokenKind kind) -> bool;

/// Checks if a token kind is an operator.
[[nodiscard]] auto is_operator(TokenKind kind) -> bool;

// ============================================================================
// Literal Value Types
// ============================================================================

/// Integer literal with its original base for error messages.
struct IntValue {
    /// The numeric value.
    uint64_t value;

    /// The base used in source: 2 (binary), 8 (octal), 10 (decimal), 16 (hex).
    uint8_t base;

    /// Optional type suffix: i8, i16, i32, i64, i128, u8, u16, u32, u64, u128.
    std::string suffix;
};

/// Floating-point literal value.
struct FloatValue {
    /// The numeric value.
    double value;

    /// Optional type suffix: f32, f64.
    std::string suffix;
};

/// String literal value with escape sequences already processed.
struct StringValue {
    /// The string content (unescaped).
    std::string value;

    /// Whether this was a raw string (`r"..."`).
    bool is_raw;
};

/// Character literal value (supports full Unicode).
struct CharValue {
    /// The Unicode code point.
    char32_t value;
};

/// Documentation comment value.
///
/// Contains the text content of a `///` or `//!` doc comment,
/// with the comment prefix stripped but markdown formatting preserved.
struct DocValue {
    /// The documentation text (markdown formatted).
    /// Multiple consecutive doc comment lines are joined with newlines.
    std::string content;
};

// ============================================================================
// Token
// ============================================================================

/// A lexical token from TML source code.
///
/// A token represents a single lexical unit produced by the lexer. It
/// contains the token kind, source location, and any associated value
/// (for literals).
///
/// # Example
///
/// For the source `let x = 42`, the lexer produces:
/// - `Token { kind: KwLet, lexeme: "let", ... }`
/// - `Token { kind: Identifier, lexeme: "x", ... }`
/// - `Token { kind: Assign, lexeme: "=", ... }`
/// - `Token { kind: IntLiteral, lexeme: "42", value: IntValue{42, 10} }`
struct Token {
    /// The kind of token.
    TokenKind kind;

    /// Source location of this token.
    SourceSpan span;

    /// Raw text from source code.
    std::string_view lexeme;

    /// Literal value (if applicable).
    ///
    /// - `std::monostate` for non-literals (keywords, operators, etc.)
    /// - `IntValue` for `IntLiteral`
    /// - `FloatValue` for `FloatLiteral`
    /// - `StringValue` for `StringLiteral`
    /// - `CharValue` for `CharLiteral`
    /// - `bool` for `BoolLiteral`
    /// - `DocValue` for `DocComment` and `ModuleDocComment`
    std::variant<std::monostate, IntValue, FloatValue, StringValue, CharValue, bool, DocValue>
        value;

    /// Checks if this token is of the given kind.
    [[nodiscard]] auto is(TokenKind k) const -> bool {
        return kind == k;
    }

    /// Checks if this token is one of the given kinds.
    [[nodiscard]] auto is_one_of(std::initializer_list<TokenKind> kinds) const -> bool {
        for (auto k : kinds) {
            if (kind == k)
                return true;
        }
        return false;
    }

    /// Checks if this is an end-of-file token.
    [[nodiscard]] auto is_eof() const -> bool {
        return kind == TokenKind::Eof;
    }

    /// Checks if this is an error token.
    [[nodiscard]] auto is_error() const -> bool {
        return kind == TokenKind::Error;
    }

    /// Gets the integer value. Asserts this is an `IntLiteral`.
    [[nodiscard]] auto int_value() const -> const IntValue&;

    /// Gets the float value. Asserts this is a `FloatLiteral`.
    [[nodiscard]] auto float_value() const -> const FloatValue&;

    /// Gets the string value. Asserts this is a `StringLiteral`.
    [[nodiscard]] auto string_value() const -> const StringValue&;

    /// Gets the character value. Asserts this is a `CharLiteral`.
    [[nodiscard]] auto char_value() const -> const CharValue&;

    /// Gets the boolean value. Asserts this is a `BoolLiteral`.
    [[nodiscard]] auto bool_value() const -> bool;

    /// Gets the doc comment value. Asserts this is a `DocComment` or `ModuleDocComment`.
    [[nodiscard]] auto doc_value() const -> const DocValue&;
};

} // namespace tml::lexer

#endif // TML_LEXER_TOKEN_HPP
