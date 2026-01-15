//! # TML Lexer
//!
//! This module implements the lexical analyzer (lexer/tokenizer) for TML.
//! The lexer converts source text into a stream of tokens for parsing.
//!
//! ## Features
//!
//! - **UTF-8 support**: Identifiers can contain Unicode characters
//! - **Multiple number bases**: Decimal, hex (`0x`), binary (`0b`), octal (`0o`)
//! - **Numeric separators**: `1_000_000` for readability
//! - **String interpolation**: `"Hello {name}!"` produces special tokens
//! - **Raw strings**: `r"no \escapes"` for regex patterns, etc.
//! - **Unicode escapes**: `'\u{1F600}'` in strings and characters
//!
//! ## Token Stream
//!
//! The lexer produces tokens incrementally via `next_token()` or all at once
//! via `tokenize()`. Significant newlines are preserved for statement parsing.
//!
//! ## Error Recovery
//!
//! The lexer continues after errors, producing `TokenKind::Error` tokens.
//! All errors are collected and can be retrieved via `errors()`.
//!
//! ## Example
//!
//! ```cpp
//! Source source = Source::from_string("let x = 42");
//! Lexer lexer(source);
//!
//! // Incremental tokenization
//! Token token = lexer.next_token();
//! while (!token.is_eof()) {
//!     process(token);
//!     token = lexer.next_token();
//! }
//!
//! // Or get all tokens at once
//! std::vector<Token> tokens = lexer.tokenize();
//! ```

#ifndef TML_LEXER_LEXER_HPP
#define TML_LEXER_LEXER_HPP

#include "common.hpp"
#include "lexer/source.hpp"
#include "lexer/token.hpp"

#include <vector>

namespace tml::lexer {

/// An error encountered during lexical analysis.
///
/// Lexer errors include information about what went wrong and where
/// in the source it occurred.
struct LexerError {
    std::string message; ///< Human-readable error description.
    SourceSpan span;     ///< Location of the error in source.
};

/// Lexical analyzer for TML source code.
///
/// The `Lexer` converts TML source text into a stream of tokens. It handles
/// all lexical elements including keywords, identifiers, literals, operators,
/// and comments.
///
/// # String Interpolation
///
/// When the lexer encounters an interpolated string like `"Hello {name}!"`,
/// it produces:
/// 1. `InterpStringStart` - `"Hello {`
/// 2. (expression tokens) - `name`
/// 3. `InterpStringEnd` - `}!"`
///
/// Nested interpolations are supported via depth tracking.
///
/// # Usage
///
/// ```cpp
/// Source source = Source::from_string("func main() { }");
/// Lexer lexer(source);
///
/// for (Token tok = lexer.next_token(); !tok.is_eof(); tok = lexer.next_token()) {
///     std::cout << token_kind_to_string(tok.kind) << "\n";
/// }
///
/// if (lexer.has_errors()) {
///     for (const auto& err : lexer.errors()) {
///         report(err);
///     }
/// }
/// ```
class Lexer {
public:
    /// Constructs a lexer for the given source.
    ///
    /// The source must outlive the lexer.
    explicit Lexer(const Source& source);

    /// Returns the next token from the source.
    ///
    /// Returns `TokenKind::Eof` when the end of input is reached.
    /// May return `TokenKind::Error` for malformed input.
    [[nodiscard]] auto next_token() -> Token;

    /// Tokenizes the entire source and returns all tokens.
    ///
    /// The returned vector includes the final `Eof` token.
    [[nodiscard]] auto tokenize() -> std::vector<Token>;

    /// Returns all errors encountered during lexing.
    [[nodiscard]] auto errors() const -> const std::vector<LexerError>& {
        return errors_;
    }

    /// Returns true if any errors occurred during lexing.
    [[nodiscard]] auto has_errors() const -> bool {
        return !errors_.empty();
    }

private:
    // ========================================================================
    // State
    // ========================================================================

    const Source& source_;           ///< Reference to source being lexed.
    size_t pos_ = 0;                 ///< Current byte position in source.
    size_t token_start_ = 0;         ///< Start position of current token.
    std::vector<LexerError> errors_; ///< Accumulated lexer errors.

    // ========================================================================
    // Interpolated String State
    // ========================================================================

    int interp_depth_ = 0;          ///< Nesting depth of `{` in interpolated strings.
    bool in_interpolation_ = false; ///< True when inside `{expr}` of a string.

    // ========================================================================
    // Template Literal State (produces Text type)
    // ========================================================================

    int template_depth_ = 0;           ///< Nesting depth of `{` in template literals.
    bool in_template_literal_ = false; ///< True when inside `{expr}` of a template literal.

    // ========================================================================
    // Character Access
    // ========================================================================

    /// Returns current character without advancing.
    [[nodiscard]] auto peek() const -> char;

    /// Returns next character without advancing.
    [[nodiscard]] auto peek_next() const -> char;

    /// Returns character n positions ahead without advancing.
    [[nodiscard]] auto peek_n(size_t n) const -> char;

    /// Consumes and returns the current character.
    auto advance() -> char;

    /// Returns true if at end of source.
    [[nodiscard]] auto is_at_end() const -> bool;

    // ========================================================================
    // Token Creation
    // ========================================================================

    /// Creates a token of the given kind with current span.
    [[nodiscard]] auto make_token(TokenKind kind) -> Token;

    /// Creates an error token with the given message.
    [[nodiscard]] auto make_error_token(const std::string& message) -> Token;

    // ========================================================================
    // Whitespace and Comments
    // ========================================================================

    /// Skips whitespace (but tracks significant newlines).
    void skip_whitespace();

    /// Skips a line comment (`// ...`).
    void skip_line_comment();

    /// Skips a block comment (`/* ... */`), supporting nesting.
    void skip_block_comment();

    /// Checks if current position is a doc comment (`///` or `//!`).
    /// Returns true and sets kind if it's a doc comment.
    [[nodiscard]] auto is_doc_comment() const -> bool;

    /// Lexes a documentation comment (`///` or `//!`).
    /// Collects consecutive doc comment lines into a single token.
    [[nodiscard]] auto lex_doc_comment() -> Token;

    // ========================================================================
    // Token Lexers
    // ========================================================================

    /// Lexes an identifier or keyword.
    [[nodiscard]] auto lex_identifier() -> Token;

    /// Lexes a numeric literal (integer or float).
    [[nodiscard]] auto lex_number() -> Token;

    /// Lexes a string literal (possibly interpolated).
    [[nodiscard]] auto lex_string() -> Token;

    /// Lexes a raw string literal (`r"..."`).
    [[nodiscard]] auto lex_raw_string() -> Token;

    /// Lexes a character literal.
    [[nodiscard]] auto lex_char() -> Token;

    /// Lexes an operator or punctuation.
    [[nodiscard]] auto lex_operator() -> Token;

    // ========================================================================
    // Interpolated String Support
    // ========================================================================

    /// Continues lexing an interpolated string after `}`.
    [[nodiscard]] auto lex_interp_string_continue() -> Token;

    /// Checks if current string contains interpolation.
    [[nodiscard]] auto check_string_has_interpolation() const -> bool;

    // ========================================================================
    // Template Literal Support (produces Text type)
    // ========================================================================

    /// Lexes a template literal (backtick string).
    [[nodiscard]] auto lex_template_literal() -> Token;

    /// Continues lexing a template literal after `}`.
    [[nodiscard]] auto lex_template_literal_continue() -> Token;

    // ========================================================================
    // Number Parsing
    // ========================================================================

    /// Lexes a hexadecimal number (`0x...`).
    [[nodiscard]] auto lex_hex_number() -> Token;

    /// Lexes a binary number (`0b...`).
    [[nodiscard]] auto lex_binary_number() -> Token;

    /// Lexes an octal number (`0o...`).
    [[nodiscard]] auto lex_octal_number() -> Token;

    /// Lexes a decimal number (integer or float).
    [[nodiscard]] auto lex_decimal_number() -> Token;

    // ========================================================================
    // Escape Sequence Handling
    // ========================================================================

    /// Parses an escape sequence (e.g., `\n`, `\t`, `\\`).
    [[nodiscard]] auto parse_escape_sequence() -> Result<char32_t, std::string>;

    /// Parses a Unicode escape (`\u{XXXX}`).
    [[nodiscard]] auto parse_unicode_escape() -> Result<char32_t, std::string>;

    // ========================================================================
    // Unicode Support
    // ========================================================================

    /// Returns true if `c` can start an identifier.
    [[nodiscard]] static auto is_identifier_start(char32_t c) -> bool;

    /// Returns true if `c` can continue an identifier.
    [[nodiscard]] static auto is_identifier_continue(char32_t c) -> bool;

    /// Decodes a UTF-8 character at current position.
    [[nodiscard]] auto decode_utf8() -> char32_t;

    /// Returns the byte length of a UTF-8 character.
    [[nodiscard]] auto utf8_char_length(char c) const -> size_t;

    // ========================================================================
    // Keyword Lookup
    // ========================================================================

    /// Looks up a keyword by identifier text.
    ///
    /// Returns the keyword token kind, or `std::nullopt` if not a keyword.
    [[nodiscard]] static auto lookup_keyword(std::string_view ident) -> std::optional<TokenKind>;

    // ========================================================================
    // Error Reporting
    // ========================================================================

    /// Reports a lexer error at the current position.
    void report_error(const std::string& message);
};

} // namespace tml::lexer

#endif // TML_LEXER_LEXER_HPP
