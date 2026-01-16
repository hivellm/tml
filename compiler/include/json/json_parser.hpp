//! # JSON Parser
//!
//! This module provides a zero-copy JSON parser for the TML compiler.
//! It includes a lexer for tokenization and a recursive descent parser
//! for building `JsonValue` trees.
//!
//! ## Features
//!
//! - **Zero-copy lexing**: Uses `std::string_view` to avoid allocations
//! - **Integer detection**: Numbers without decimals/exponents are parsed as integers
//! - **Error recovery**: Reports errors with precise line/column information
//! - **Depth limiting**: Prevents stack overflow on deeply nested input
//!
//! ## Token Types
//!
//! | Token | Description | Example |
//! |-------|-------------|---------|
//! | `LBrace` | Left brace | `{` |
//! | `RBrace` | Right brace | `}` |
//! | `LBracket` | Left bracket | `[` |
//! | `RBracket` | Right bracket | `]` |
//! | `Colon` | Colon | `:` |
//! | `Comma` | Comma | `,` |
//! | `String` | Quoted string | `"hello"` |
//! | `IntNumber` | Integer | `42`, `-123` |
//! | `FloatNumber` | Float | `3.14`, `1e10` |
//! | `True` | Boolean true | `true` |
//! | `False` | Boolean false | `false` |
//! | `Null` | Null value | `null` |
//!
//! ## Example
//!
//! ```cpp
//! #include "json/json_parser.hpp"
//! using namespace tml::json;
//!
//! // Parse JSON string
//! auto result = parse_json(R"({"name": "Alice", "age": 30})");
//! if (is_ok(result)) {
//!     auto& json = unwrap(result);
//!     auto* name = json.get("name");
//!     auto* age = json.get("age");
//!     std::cout << name->as_string() << " is " << age->as_i64() << std::endl;
//! } else {
//!     auto& error = unwrap_err(result);
//!     std::cerr << error.to_string() << std::endl;
//! }
//! ```

#pragma once

#include "common.hpp"
#include "json/json_error.hpp"
#include "json/json_value.hpp"

#include <string_view>
#include <vector>

namespace tml::json {

// ============================================================================
// Token Types
// ============================================================================

/// Token types for the JSON lexer.
///
/// These represent the lexical elements of JSON according to RFC 8259.
enum class JsonTokenKind : uint8_t {
    // Structural tokens
    LBrace,    ///< `{` - Start of object
    RBrace,    ///< `}` - End of object
    LBracket,  ///< `[` - Start of array
    RBracket,  ///< `]` - End of array
    Colon,     ///< `:` - Key-value separator
    Comma,     ///< `,` - Element separator

    // Value tokens
    String,      ///< `"..."` - String literal
    IntNumber,   ///< `123`, `-456` - Integer (no decimal or exponent)
    FloatNumber, ///< `1.5`, `1e10` - Float (has decimal or exponent)
    True,        ///< `true` - Boolean true
    False,       ///< `false` - Boolean false
    Null,        ///< `null` - Null value

    // Special tokens
    Eof,   ///< End of input
    Error  ///< Lexer error (check error message)
};

/// A token produced by the JSON lexer.
///
/// Contains the token type, its text representation, and location information.
/// For `String` tokens, `string_value` contains the unescaped content.
/// For `IntNumber`/`FloatNumber` tokens, `number_value` contains the parsed number.
struct JsonToken {
    /// The type of this token.
    JsonTokenKind kind;

    /// The original text of this token (view into source).
    std::string_view lexeme;

    /// Line number where this token starts (1-based).
    size_t line;

    /// Column number where this token starts (1-based).
    size_t column;

    /// Byte offset where this token starts.
    size_t offset;

    /// For `String` tokens: the unescaped string content.
    std::string string_value;

    /// For `IntNumber`/`FloatNumber` tokens: the parsed number.
    JsonNumber number_value;
};

// ============================================================================
// Lexer
// ============================================================================

/// Zero-copy JSON lexer.
///
/// The `JsonLexer` tokenizes a JSON input string, producing tokens one at a time.
/// It handles string escapes, number parsing, and whitespace skipping.
///
/// # Example
///
/// ```cpp
/// JsonLexer lexer(R"({"key": 42})");
/// while (true) {
///     JsonToken tok = lexer.next_token();
///     if (tok.kind == JsonTokenKind::Eof) break;
///     // Process token...
/// }
/// ```
class JsonLexer {
public:
    /// Creates a lexer for the given input.
    ///
    /// # Arguments
    ///
    /// * `input` - The JSON string to tokenize
    explicit JsonLexer(std::string_view input);

    /// Returns the next token from the input.
    ///
    /// Call this repeatedly until `JsonTokenKind::Eof` is returned.
    /// If an error occurs, returns a token with `kind == JsonTokenKind::Error`.
    ///
    /// # Returns
    ///
    /// The next token from the input.
    auto next_token() -> JsonToken;

    /// Returns `true` if any errors occurred during lexing.
    [[nodiscard]] auto has_errors() const -> bool { return has_errors_; }

    /// Returns the list of errors encountered during lexing.
    [[nodiscard]] auto errors() const -> const std::vector<JsonError>& { return errors_; }

private:
    std::string_view input_;
    size_t pos_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;
    bool has_errors_ = false;
    std::vector<JsonError> errors_;

    /// Returns the current character without advancing.
    [[nodiscard]] auto peek() const -> char;

    /// Returns the next character without advancing.
    [[nodiscard]] auto peek_next() const -> char;

    /// Advances and returns the current character.
    auto advance() -> char;

    /// Skips whitespace characters.
    void skip_whitespace();

    /// Creates a token at the given position.
    auto make_token(JsonTokenKind kind, size_t start_pos, size_t start_line,
                    size_t start_col) -> JsonToken;

    /// Scans a string token.
    auto scan_string() -> JsonToken;

    /// Scans a number token.
    auto scan_number() -> JsonToken;

    /// Scans a keyword token (true, false, null).
    auto scan_keyword() -> JsonToken;

    /// Adds an error to the error list.
    void add_error(const std::string& msg);

    /// Adds an error with location information.
    void add_error(const std::string& msg, size_t line, size_t col);
};

// ============================================================================
// Parser
// ============================================================================

/// Recursive descent JSON parser.
///
/// The `JsonParser` builds a `JsonValue` tree from tokenized input.
/// It enforces a maximum nesting depth to prevent stack overflow.
///
/// # Example
///
/// ```cpp
/// JsonParser parser(R"([1, 2, 3])");
/// auto result = parser.parse();
/// if (is_ok(result)) {
///     auto& arr = unwrap(result);
///     // Use array...
/// }
/// ```
class JsonParser {
public:
    /// Creates a parser for the given input.
    ///
    /// # Arguments
    ///
    /// * `input` - The JSON string to parse
    explicit JsonParser(std::string_view input);

    /// Parses the input and returns a `JsonValue`.
    ///
    /// # Returns
    ///
    /// `Ok(JsonValue)` on success, `Err(JsonError)` on failure.
    [[nodiscard]] auto parse() -> Result<JsonValue, JsonError>;

private:
    JsonLexer lexer_;
    JsonToken current_;
    static constexpr size_t MAX_DEPTH = 1000;
    size_t depth_ = 0;

    /// Advances to the next token.
    void advance();

    /// Checks if the current token is of the expected kind.
    [[nodiscard]] auto check(JsonTokenKind kind) const -> bool;

    /// Advances if the current token matches, returns true if matched.
    auto match(JsonTokenKind kind) -> bool;

    /// Requires the current token to be of the expected kind.
    [[nodiscard]] auto expect(JsonTokenKind kind) -> bool;

    /// Creates an error at the current position.
    [[nodiscard]] auto make_error(const std::string& msg) const -> JsonError;

    /// Parses a value (any JSON type).
    auto parse_value() -> Result<JsonValue, JsonError>;

    /// Parses an object.
    auto parse_object() -> Result<JsonValue, JsonError>;

    /// Parses an array.
    auto parse_array() -> Result<JsonValue, JsonError>;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/// Parses a JSON string and returns a `JsonValue`.
///
/// This is the main entry point for parsing JSON.
///
/// # Arguments
///
/// * `input` - The JSON string to parse
///
/// # Returns
///
/// `Ok(JsonValue)` on success, `Err(JsonError)` on failure.
///
/// # Example
///
/// ```cpp
/// auto result = parse_json(R"({"message": "hello"})");
/// if (is_ok(result)) {
///     auto& json = unwrap(result);
///     std::cout << json.get("message")->as_string() << std::endl;
/// }
/// ```
[[nodiscard]] auto parse_json(std::string_view input) -> Result<JsonValue, JsonError>;

} // namespace tml::json
