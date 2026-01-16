# Specification: JSON Parser

## 1. Token Types

```cpp
enum class JsonTokenKind {
    LBrace,      // {
    RBrace,      // }
    LBracket,    // [
    RBracket,    // ]
    Colon,       // :
    Comma,       // ,
    String,      // "..."
    Number,      // 123, 1.5, 1e10
    True,        // true
    False,       // false
    Null,        // null
    Eof,         // end of input
    Error,       // invalid token
};

struct JsonToken {
    JsonTokenKind kind;
    std::string_view lexeme;  // view into source
    size_t line;
    size_t column;
};
```

## 2. Lexer

```cpp
class JsonLexer {
    std::string_view source_;
    size_t pos_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;

public:
    explicit JsonLexer(std::string_view source);
    auto next_token() -> JsonToken;

private:
    auto scan_string() -> JsonToken;
    auto scan_number() -> JsonToken;
    auto scan_keyword() -> JsonToken;
    void skip_whitespace();
    auto peek() const -> char;
    auto advance() -> char;
};
```

## 3. String Escape Sequences

| Escape | Character |
|--------|-----------|
| `\"` | Quotation mark |
| `\\` | Reverse solidus |
| `\/` | Solidus |
| `\b` | Backspace |
| `\f` | Form feed |
| `\n` | Line feed |
| `\r` | Carriage return |
| `\t` | Tab |
| `\uXXXX` | Unicode code point |

## 4. Number Format

```
number = [ "-" ] int [ frac ] [ exp ]
int    = "0" | digit1-9 *digit
frac   = "." 1*digit
exp    = ("e" | "E") [ "+" | "-" ] 1*digit
```

## 5. Parser

```cpp
class JsonParser {
    JsonLexer lexer_;
    JsonToken current_;
    static constexpr size_t MAX_DEPTH = 1000;
    size_t depth_ = 0;

public:
    explicit JsonParser(std::string_view source);
    auto parse() -> Result<std::unique_ptr<JsonValue>, JsonError>;

private:
    auto parse_value() -> Result<std::unique_ptr<JsonValue>, JsonError>;
    auto parse_object() -> Result<std::unique_ptr<JsonValue>, JsonError>;
    auto parse_array() -> Result<std::unique_ptr<JsonValue>, JsonError>;
    auto parse_string() -> Result<std::string, JsonError>;
    auto parse_number() -> Result<double, JsonError>;

    void advance();
    auto expect(JsonTokenKind kind) -> Result<void, JsonError>;
    auto match(JsonTokenKind kind) -> bool;
};
```

## 6. Error Types

```cpp
struct JsonError {
    enum class Kind {
        UnexpectedToken,
        UnterminatedString,
        InvalidEscape,
        InvalidNumber,
        NestingTooDeep,
        TrailingComma,
        DuplicateKey,
    };

    Kind kind;
    std::string message;
    size_t line;
    size_t column;
};
```
