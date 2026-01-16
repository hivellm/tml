//! # JSON Parser Implementation
//!
//! This module implements the JSON lexer and recursive descent parser.
//! The lexer tokenizes input using `std::string_view` for zero-copy operation.
//! The parser builds a `JsonValue` tree from the token stream.
//!
//! ## Architecture
//!
//! The parsing process has two stages:
//!
//! 1. **Lexing**: `JsonLexer` converts input string to tokens
//! 2. **Parsing**: `JsonParser` builds `JsonValue` tree from tokens
//!
//! ## Lexer Details
//!
//! The lexer handles:
//! - Structural tokens: `{`, `}`, `[`, `]`, `:`, `,`
//! - String literals with escape sequences
//! - Numbers (integers and floats)
//! - Keywords: `true`, `false`, `null`
//! - Whitespace skipping
//!
//! ## Parser Details
//!
//! The parser uses recursive descent with:
//! - Depth limiting (MAX_DEPTH = 1000) to prevent stack overflow
//! - Error recovery with precise location information
//! - Integer detection for numbers without decimal/exponent
//!
//! ## Example
//!
//! ```cpp
//! auto result = parse_json(R"({"name": "Alice", "age": 30})");
//! if (is_ok(result)) {
//!     auto& json = unwrap(result);
//!     // json.get("name")->as_string() == "Alice"
//!     // json.get("age")->as_i64() == 30
//! }
//! ```

#include "json/json_parser.hpp"

#include <cctype>
#include <charconv>
#include <cstdlib>

namespace tml::json {

// ============================================================================
// JsonLexer Implementation
// ============================================================================

/// Creates a new lexer for the given input.
///
/// # Arguments
///
/// * `input` - The JSON string to tokenize
JsonLexer::JsonLexer(std::string_view input) : input_(input) {}

/// Returns the current character without advancing the position.
///
/// # Returns
///
/// The character at the current position, or `'\0'` if at end of input.
auto JsonLexer::peek() const -> char {
    if (pos_ >= input_.size()) {
        return '\0';
    }
    return input_[pos_];
}

/// Returns the next character without advancing the position.
///
/// # Returns
///
/// The character after the current position, or `'\0'` if at end.
auto JsonLexer::peek_next() const -> char {
    if (pos_ + 1 >= input_.size()) {
        return '\0';
    }
    return input_[pos_ + 1];
}

/// Advances the position and returns the current character.
///
/// Updates line and column tracking for newlines.
///
/// # Returns
///
/// The character that was at the current position before advancing.
auto JsonLexer::advance() -> char {
    if (pos_ >= input_.size()) {
        return '\0';
    }
    char c = input_[pos_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

/// Skips whitespace characters (space, tab, newline, carriage return).
///
/// Advances the position until a non-whitespace character is found.
void JsonLexer::skip_whitespace() {
    while (pos_ < input_.size()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
        } else {
            break;
        }
    }
}

/// Creates a token with the given kind and position information.
///
/// # Arguments
///
/// * `kind` - The token type
/// * `start_pos` - Byte offset where the token starts
/// * `start_line` - Line number where the token starts
/// * `start_col` - Column number where the token starts
///
/// # Returns
///
/// A `JsonToken` with the lexeme extracted from input.
auto JsonLexer::make_token(JsonTokenKind kind, size_t start_pos, size_t start_line,
                           size_t start_col) -> JsonToken {
    JsonToken tok;
    tok.kind = kind;
    tok.lexeme = input_.substr(start_pos, pos_ - start_pos);
    tok.line = start_line;
    tok.column = start_col;
    tok.offset = start_pos;
    return tok;
}

/// Adds an error at the current position.
///
/// # Arguments
///
/// * `msg` - The error message
void JsonLexer::add_error(const std::string& msg) {
    has_errors_ = true;
    errors_.push_back(JsonError::make(msg, line_, column_));
}

/// Adds an error at a specific position.
///
/// # Arguments
///
/// * `msg` - The error message
/// * `line` - Line number where the error occurred
/// * `col` - Column number where the error occurred
void JsonLexer::add_error(const std::string& msg, size_t line, size_t col) {
    has_errors_ = true;
    errors_.push_back(JsonError::make(msg, line, col));
}

/// Scans a string token starting at the current position.
///
/// Handles escape sequences according to RFC 8259:
/// - `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`
/// - `\uXXXX` for Unicode code points
///
/// # Returns
///
/// A `String` token with unescaped content, or `Error` token on failure.
auto JsonLexer::scan_string() -> JsonToken {
    size_t start_pos = pos_;
    size_t start_line = line_;
    size_t start_col = column_;

    advance(); // Skip opening quote

    std::string value;
    while (pos_ < input_.size()) {
        char c = peek();

        if (c == '"') {
            advance(); // Skip closing quote
            JsonToken tok = make_token(JsonTokenKind::String, start_pos, start_line, start_col);
            tok.string_value = std::move(value);
            return tok;
        }

        if (c == '\\') {
            advance(); // Skip backslash
            char escaped = peek();
            switch (escaped) {
            case '"':
                value += '"';
                advance();
                break;
            case '\\':
                value += '\\';
                advance();
                break;
            case '/':
                value += '/';
                advance();
                break;
            case 'b':
                value += '\b';
                advance();
                break;
            case 'f':
                value += '\f';
                advance();
                break;
            case 'n':
                value += '\n';
                advance();
                break;
            case 'r':
                value += '\r';
                advance();
                break;
            case 't':
                value += '\t';
                advance();
                break;
            case 'u': {
                advance(); // Skip 'u'
                // Parse 4 hex digits
                if (pos_ + 4 > input_.size()) {
                    add_error("Incomplete unicode escape sequence");
                    return make_token(JsonTokenKind::Error, start_pos, start_line, start_col);
                }
                std::string_view hex = input_.substr(pos_, 4);
                unsigned int codepoint = 0;
                auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + 4, codepoint, 16);
                if (ec != std::errc{} || ptr != hex.data() + 4) {
                    add_error("Invalid unicode escape sequence");
                    return make_token(JsonTokenKind::Error, start_pos, start_line, start_col);
                }
                pos_ += 4;
                column_ += 4;
                // Convert codepoint to UTF-8
                if (codepoint < 0x80) {
                    value += static_cast<char>(codepoint);
                } else if (codepoint < 0x800) {
                    value += static_cast<char>(0xC0 | (codepoint >> 6));
                    value += static_cast<char>(0x80 | (codepoint & 0x3F));
                } else {
                    value += static_cast<char>(0xE0 | (codepoint >> 12));
                    value += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    value += static_cast<char>(0x80 | (codepoint & 0x3F));
                }
                break;
            }
            default:
                add_error("Invalid escape sequence: \\" + std::string(1, escaped));
                return make_token(JsonTokenKind::Error, start_pos, start_line, start_col);
            }
        } else if (static_cast<unsigned char>(c) < 0x20) {
            add_error("Control character in string");
            return make_token(JsonTokenKind::Error, start_pos, start_line, start_col);
        } else {
            value += c;
            advance();
        }
    }

    add_error("Unterminated string", start_line, start_col);
    return make_token(JsonTokenKind::Error, start_pos, start_line, start_col);
}

/// Scans a number token starting at the current position.
///
/// Parses numbers according to RFC 8259:
/// - Optional leading minus sign
/// - Integer part (no leading zeros except for `0`)
/// - Optional fractional part (`.` followed by digits)
/// - Optional exponent part (`e`/`E` followed by optional sign and digits)
///
/// Numbers without decimal point or exponent are stored as integers.
///
/// # Returns
///
/// An `IntNumber` or `FloatNumber` token, or `Error` token on failure.
auto JsonLexer::scan_number() -> JsonToken {
    size_t start_pos = pos_;
    size_t start_line = line_;
    size_t start_col = column_;

    bool is_float = false;

    // Optional minus sign
    if (peek() == '-') {
        advance();
    }

    // Integer part
    if (peek() == '0') {
        advance();
    } else if (std::isdigit(peek())) {
        while (std::isdigit(peek())) {
            advance();
        }
    } else {
        add_error("Invalid number");
        return make_token(JsonTokenKind::Error, start_pos, start_line, start_col);
    }

    // Fractional part
    if (peek() == '.') {
        is_float = true;
        advance();
        if (!std::isdigit(peek())) {
            add_error("Expected digit after decimal point");
            return make_token(JsonTokenKind::Error, start_pos, start_line, start_col);
        }
        while (std::isdigit(peek())) {
            advance();
        }
    }

    // Exponent part
    if (peek() == 'e' || peek() == 'E') {
        is_float = true;
        advance();
        if (peek() == '+' || peek() == '-') {
            advance();
        }
        if (!std::isdigit(peek())) {
            add_error("Expected digit in exponent");
            return make_token(JsonTokenKind::Error, start_pos, start_line, start_col);
        }
        while (std::isdigit(peek())) {
            advance();
        }
    }

    std::string_view num_str = input_.substr(start_pos, pos_ - start_pos);
    JsonToken tok = make_token(is_float ? JsonTokenKind::FloatNumber : JsonTokenKind::IntNumber,
                               start_pos, start_line, start_col);

    if (is_float) {
        // Parse as double
        double value = std::strtod(std::string(num_str).c_str(), nullptr);
        tok.number_value = JsonNumber(value);
    } else {
        // Parse as integer
        bool negative = (num_str[0] == '-');
        if (negative) {
            int64_t value = 0;
            auto [ptr, ec] =
                std::from_chars(num_str.data(), num_str.data() + num_str.size(), value);
            if (ec != std::errc{}) {
                // Overflow - fall back to double
                double dval = std::strtod(std::string(num_str).c_str(), nullptr);
                tok.kind = JsonTokenKind::FloatNumber;
                tok.number_value = JsonNumber(dval);
            } else {
                tok.number_value = JsonNumber(value);
            }
        } else {
            uint64_t value = 0;
            auto [ptr, ec] =
                std::from_chars(num_str.data(), num_str.data() + num_str.size(), value);
            if (ec != std::errc{}) {
                // Overflow - fall back to double
                double dval = std::strtod(std::string(num_str).c_str(), nullptr);
                tok.kind = JsonTokenKind::FloatNumber;
                tok.number_value = JsonNumber(dval);
            } else if (value <= static_cast<uint64_t>(INT64_MAX)) {
                tok.number_value = JsonNumber(static_cast<int64_t>(value));
            } else {
                tok.number_value = JsonNumber(value);
            }
        }
    }

    return tok;
}

/// Scans a keyword token (`true`, `false`, `null`).
///
/// # Returns
///
/// A `True`, `False`, or `Null` token, or `Error` for unknown keywords.
auto JsonLexer::scan_keyword() -> JsonToken {
    size_t start_pos = pos_;
    size_t start_line = line_;
    size_t start_col = column_;

    while (std::isalpha(peek())) {
        advance();
    }

    std::string_view word = input_.substr(start_pos, pos_ - start_pos);

    if (word == "true") {
        return make_token(JsonTokenKind::True, start_pos, start_line, start_col);
    }
    if (word == "false") {
        return make_token(JsonTokenKind::False, start_pos, start_line, start_col);
    }
    if (word == "null") {
        return make_token(JsonTokenKind::Null, start_pos, start_line, start_col);
    }

    add_error("Unknown keyword: " + std::string(word), start_line, start_col);
    return make_token(JsonTokenKind::Error, start_pos, start_line, start_col);
}

/// Returns the next token from the input.
///
/// Skips whitespace, then identifies and returns the next token.
/// Returns `Eof` when the end of input is reached.
///
/// # Returns
///
/// The next `JsonToken` from the input stream.
auto JsonLexer::next_token() -> JsonToken {
    skip_whitespace();

    if (pos_ >= input_.size()) {
        JsonToken tok;
        tok.kind = JsonTokenKind::Eof;
        tok.line = line_;
        tok.column = column_;
        tok.offset = pos_;
        return tok;
    }

    size_t start_pos = pos_;
    size_t start_line = line_;
    size_t start_col = column_;
    char c = peek();

    switch (c) {
    case '{':
        advance();
        return make_token(JsonTokenKind::LBrace, start_pos, start_line, start_col);
    case '}':
        advance();
        return make_token(JsonTokenKind::RBrace, start_pos, start_line, start_col);
    case '[':
        advance();
        return make_token(JsonTokenKind::LBracket, start_pos, start_line, start_col);
    case ']':
        advance();
        return make_token(JsonTokenKind::RBracket, start_pos, start_line, start_col);
    case ':':
        advance();
        return make_token(JsonTokenKind::Colon, start_pos, start_line, start_col);
    case ',':
        advance();
        return make_token(JsonTokenKind::Comma, start_pos, start_line, start_col);
    case '"':
        return scan_string();
    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return scan_number();
    case 't':
    case 'f':
    case 'n':
        return scan_keyword();
    default:
        advance();
        add_error("Unexpected character: " + std::string(1, c), start_line, start_col);
        return make_token(JsonTokenKind::Error, start_pos, start_line, start_col);
    }
}

// ============================================================================
// JsonParser Implementation
// ============================================================================

/// Creates a new parser for the given input.
///
/// Initializes the lexer and reads the first token.
///
/// # Arguments
///
/// * `input` - The JSON string to parse
JsonParser::JsonParser(std::string_view input) : lexer_(input) {
    advance();
}

/// Advances to the next token from the lexer.
void JsonParser::advance() {
    current_ = lexer_.next_token();
}

/// Checks if the current token matches the expected kind.
///
/// # Arguments
///
/// * `kind` - The token kind to check for
///
/// # Returns
///
/// `true` if the current token matches, `false` otherwise.
auto JsonParser::check(JsonTokenKind kind) const -> bool {
    return current_.kind == kind;
}

/// Advances if the current token matches the expected kind.
///
/// # Arguments
///
/// * `kind` - The token kind to match
///
/// # Returns
///
/// `true` if matched and advanced, `false` otherwise.
auto JsonParser::match(JsonTokenKind kind) -> bool {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

/// Expects the current token to be of the given kind, advances if so.
///
/// # Arguments
///
/// * `kind` - The expected token kind
///
/// # Returns
///
/// `true` if the token matched and was consumed.
auto JsonParser::expect(JsonTokenKind kind) -> bool {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

/// Creates an error at the current token's position.
///
/// # Arguments
///
/// * `msg` - The error message
///
/// # Returns
///
/// A `JsonError` with the current position information.
auto JsonParser::make_error(const std::string& msg) const -> JsonError {
    return JsonError::make(msg, current_.line, current_.column, current_.offset);
}

/// Parses the entire input and returns a `JsonValue`.
///
/// Expects exactly one JSON value followed by end of input.
///
/// # Returns
///
/// `Ok(JsonValue)` on success, `Err(JsonError)` on failure.
auto JsonParser::parse() -> Result<JsonValue, JsonError> {
    auto result = parse_value();
    if (is_err(result)) {
        return result;
    }

    if (!check(JsonTokenKind::Eof)) {
        return make_error("Unexpected content after JSON value");
    }

    return result;
}

/// Parses a single JSON value.
///
/// Handles all JSON types: null, boolean, number, string, array, object.
///
/// # Returns
///
/// `Ok(JsonValue)` on success, `Err(JsonError)` on failure.
auto JsonParser::parse_value() -> Result<JsonValue, JsonError> {
    if (depth_ >= MAX_DEPTH) {
        return make_error("Maximum nesting depth exceeded");
    }

    switch (current_.kind) {
    case JsonTokenKind::Null:
        advance();
        return JsonValue();

    case JsonTokenKind::True:
        advance();
        return JsonValue(true);

    case JsonTokenKind::False:
        advance();
        return JsonValue(false);

    case JsonTokenKind::IntNumber:
    case JsonTokenKind::FloatNumber: {
        JsonNumber num = current_.number_value;
        advance();
        return JsonValue(num);
    }

    case JsonTokenKind::String: {
        std::string str = std::move(current_.string_value);
        advance();
        return JsonValue(std::move(str));
    }

    case JsonTokenKind::LBrace:
        return parse_object();

    case JsonTokenKind::LBracket:
        return parse_array();

    case JsonTokenKind::Error:
        return make_error("Lexer error: " + std::string(current_.lexeme));

    case JsonTokenKind::Eof:
        return make_error("Unexpected end of input");

    default:
        return make_error("Unexpected token");
    }
}

/// Parses a JSON object (`{...}`).
///
/// Expects the current token to be `{`.
/// Parses key-value pairs separated by commas.
/// Trailing commas are not allowed.
///
/// # Returns
///
/// `Ok(JsonValue)` containing the object, `Err(JsonError)` on failure.
auto JsonParser::parse_object() -> Result<JsonValue, JsonError> {
    ++depth_;
    advance(); // Skip '{'

    JsonObject obj;

    if (check(JsonTokenKind::RBrace)) {
        advance();
        --depth_;
        return JsonValue(std::move(obj));
    }

    while (true) {
        // Expect string key
        if (!check(JsonTokenKind::String)) {
            --depth_;
            return make_error("Expected string key in object");
        }
        std::string key = std::move(current_.string_value);
        advance();

        // Expect colon
        if (!match(JsonTokenKind::Colon)) {
            --depth_;
            return make_error("Expected ':' after object key");
        }

        // Parse value
        auto value_result = parse_value();
        if (is_err(value_result)) {
            --depth_;
            return value_result;
        }

        obj[std::move(key)] = std::move(unwrap(value_result));

        // Check for comma or end
        if (check(JsonTokenKind::Comma)) {
            advance();
            // Check for trailing comma (not allowed in JSON)
            if (check(JsonTokenKind::RBrace)) {
                --depth_;
                return make_error("Trailing comma in object");
            }
        } else if (check(JsonTokenKind::RBrace)) {
            advance();
            --depth_;
            return JsonValue(std::move(obj));
        } else {
            --depth_;
            return make_error("Expected ',' or '}' in object");
        }
    }
}

/// Parses a JSON array (`[...]`).
///
/// Expects the current token to be `[`.
/// Parses values separated by commas.
/// Trailing commas are not allowed.
///
/// # Returns
///
/// `Ok(JsonValue)` containing the array, `Err(JsonError)` on failure.
auto JsonParser::parse_array() -> Result<JsonValue, JsonError> {
    ++depth_;
    advance(); // Skip '['

    JsonArray arr;

    if (check(JsonTokenKind::RBracket)) {
        advance();
        --depth_;
        return JsonValue(std::move(arr));
    }

    while (true) {
        // Parse value
        auto value_result = parse_value();
        if (is_err(value_result)) {
            --depth_;
            return value_result;
        }

        arr.push_back(std::move(unwrap(value_result)));

        // Check for comma or end
        if (check(JsonTokenKind::Comma)) {
            advance();
            // Check for trailing comma (not allowed in JSON)
            if (check(JsonTokenKind::RBracket)) {
                --depth_;
                return make_error("Trailing comma in array");
            }
        } else if (check(JsonTokenKind::RBracket)) {
            advance();
            --depth_;
            return JsonValue(std::move(arr));
        } else {
            --depth_;
            return make_error("Expected ',' or ']' in array");
        }
    }
}

// ============================================================================
// Convenience Functions
// ============================================================================

/// Parses a JSON string and returns a `JsonValue`.
///
/// This is the main entry point for parsing JSON. It creates a parser,
/// parses the input, and returns the result.
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
/// auto result = parse_json(R"({"key": "value"})");
/// if (is_ok(result)) {
///     auto& json = unwrap(result);
///     std::cout << json.get("key")->as_string() << std::endl;
/// } else {
///     std::cerr << unwrap_err(result).to_string() << std::endl;
/// }
/// ```
auto parse_json(std::string_view input) -> Result<JsonValue, JsonError> {
    JsonParser parser(input);
    return parser.parse();
}

} // namespace tml::json
