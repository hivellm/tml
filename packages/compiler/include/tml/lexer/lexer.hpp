#ifndef TML_LEXER_LEXER_HPP
#define TML_LEXER_LEXER_HPP

#include "tml/common.hpp"
#include "tml/lexer/source.hpp"
#include "tml/lexer/token.hpp"
#include <vector>

namespace tml::lexer {

// Lexer error
struct LexerError {
    std::string message;
    SourceSpan span;
};

// Lexer - tokenizes TML source code
class Lexer {
public:
    explicit Lexer(const Source& source);

    // Get the next token
    [[nodiscard]] auto next_token() -> Token;

    // Tokenize entire source and return all tokens
    [[nodiscard]] auto tokenize() -> std::vector<Token>;

    // Get all errors encountered during lexing
    [[nodiscard]] auto errors() const -> const std::vector<LexerError>& { return errors_; }

    // Check if any errors occurred
    [[nodiscard]] auto has_errors() const -> bool { return !errors_.empty(); }

private:
    const Source& source_;
    size_t pos_ = 0;           // Current byte position
    size_t token_start_ = 0;   // Start of current token
    std::vector<LexerError> errors_;

    // Character access
    [[nodiscard]] auto peek() const -> char;
    [[nodiscard]] auto peek_next() const -> char;
    [[nodiscard]] auto peek_n(size_t n) const -> char;
    auto advance() -> char;
    [[nodiscard]] auto is_at_end() const -> bool;

    // Token creation
    [[nodiscard]] auto make_token(TokenKind kind) -> Token;
    [[nodiscard]] auto make_error_token(const std::string& message) -> Token;

    // Lexing helpers
    void skip_whitespace();
    void skip_line_comment();
    void skip_block_comment();

    // Token lexers
    [[nodiscard]] auto lex_identifier() -> Token;
    [[nodiscard]] auto lex_number() -> Token;
    [[nodiscard]] auto lex_string() -> Token;
    [[nodiscard]] auto lex_raw_string() -> Token;
    [[nodiscard]] auto lex_char() -> Token;
    [[nodiscard]] auto lex_operator() -> Token;

    // Number parsing helpers
    [[nodiscard]] auto lex_hex_number() -> Token;
    [[nodiscard]] auto lex_binary_number() -> Token;
    [[nodiscard]] auto lex_octal_number() -> Token;
    [[nodiscard]] auto lex_decimal_number() -> Token;

    // String/char escape handling
    [[nodiscard]] auto parse_escape_sequence() -> Result<char32_t, std::string>;
    [[nodiscard]] auto parse_unicode_escape() -> Result<char32_t, std::string>;

    // Unicode helpers
    [[nodiscard]] static auto is_identifier_start(char32_t c) -> bool;
    [[nodiscard]] static auto is_identifier_continue(char32_t c) -> bool;
    [[nodiscard]] auto decode_utf8() -> char32_t;
    [[nodiscard]] auto utf8_char_length(char c) const -> size_t;

    // Keyword lookup
    [[nodiscard]] static auto lookup_keyword(std::string_view ident) -> std::optional<TokenKind>;

    // Error reporting
    void report_error(const std::string& message);
};

} // namespace tml::lexer

#endif // TML_LEXER_LEXER_HPP
