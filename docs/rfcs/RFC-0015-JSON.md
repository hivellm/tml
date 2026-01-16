# RFC-0015: Native JSON Implementation

## Status
Active - **Implemented in v0.6.0**

## Summary

This RFC defines TML's native JSON implementation for parsing, serialization, building, validation, and JSON-RPC 2.0 support. The implementation is self-contained (no external dependencies) and optimized for MCP (Model Context Protocol) integration.

## Motivation

TML needs native JSON support for:

1. **MCP Integration** - Model Context Protocol requires JSON-RPC 2.0
2. **Configuration** - Parse config files, package manifests
3. **API Communication** - HTTP APIs, WebSockets, REST
4. **Data Interchange** - Import/export data between systems
5. **Integer Precision** - Preserve I64/U64 without floating-point loss

### Why Native Implementation?

- **No Dependencies** - Self-contained, no external library required
- **Integer Preservation** - Discriminated union for Int64/Uint64/Double
- **Zero-Copy Parsing** - Uses `string_view` where possible
- **Move Semantics** - Efficient memory management via RAII
- **Thread-Safe** - Stateless parser, safe for concurrent use

## Specification

### 1. JSON Value Types

#### 1.1 JsonNumber

Discriminated union preserving numeric precision:

```cpp
struct JsonNumber {
    enum class Kind { Int64, Uint64, Double };
    Kind kind;
    union {
        int64_t i64;
        uint64_t u64;
        double f64;
    };
};
```

**Semantics:**
- Integers without decimal/exponent → stored as Int64 or Uint64
- Numbers with decimal or exponent → stored as Double
- Integer overflow → parse error

#### 1.2 JsonValue

Variant type representing any JSON value:

```cpp
struct JsonValue {
    std::variant<
        std::monostate,     // null
        bool,               // boolean
        JsonNumber,         // number
        std::string,        // string
        Box<JsonArray>,     // array
        Box<JsonObject>     // object
    > data;
};

using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;
```

#### 1.3 Type Query Methods

```cpp
bool is_null() const;
bool is_bool() const;
bool is_number() const;
bool is_integer() const;   // Int64 or Uint64
bool is_float() const;     // Double
bool is_string() const;
bool is_array() const;
bool is_object() const;
```

#### 1.4 Value Accessors

```cpp
// Primitive accessors (undefined behavior if wrong type)
bool as_bool() const;
int64_t as_i64() const;
uint64_t as_u64() const;
double as_f64() const;
const std::string& as_string() const;
const JsonArray& as_array() const;
const JsonObject& as_object() const;

// Safe accessors
std::optional<int64_t> try_as_i64() const;
std::optional<uint64_t> try_as_u64() const;

// Object access
JsonValue* get(const std::string& key);
const JsonValue* get(const std::string& key) const;
```

### 2. JSON Parser

#### 2.1 Parse Function

```cpp
auto parse_json(std::string_view input) -> std::variant<JsonValue, JsonError>;
```

#### 2.2 JsonError

```cpp
struct JsonError {
    std::string message;
    size_t line;
    size_t column;
};
```

#### 2.3 Parser Features

- **RFC 8259 Compliant** - Full JSON specification support
- **Max Depth Limit** - 1000 levels to prevent stack overflow
- **Line/Column Tracking** - Precise error location
- **Unicode Support** - `\uXXXX` escape sequences
- **Integer Detection** - Numbers without `.` or `e/E` stored as integers

### 3. JSON Serializer

#### 3.1 Serialization Functions

```cpp
// Compact output
auto to_string() const -> std::string;

// Pretty-printed output
auto to_string_pretty(int indent = 2) const -> std::string;

// Streaming output (no intermediate allocations)
auto write_to(std::ostream& os) const -> std::ostream&;
auto write_to_pretty(std::ostream& os, int indent = 2) const -> std::ostream&;
```

#### 3.2 Serialization Rules

- **null** → `null`
- **bool** → `true` or `false`
- **Int64/Uint64** → integer format (no decimal)
- **Double** → floating-point format, 17-digit precision
- **NaN/Infinity** → serialized as `null` (JSON limitation)
- **string** → quoted with escapes (`\"`, `\\`, `\n`, `\t`, etc.)
- **array** → `[...]` with comma-separated elements
- **object** → `{...}` with ordered keys (deterministic output)

### 4. JSON Builder API

#### 4.1 Fluent Builder

```cpp
auto json = JsonBuilder()
    .object()
        .field("name", "Alice")
        .field("age", 30)
        .field("roles", JsonBuilder()
            .array()
                .item("admin")
                .item("user")
            .end()
            .build())
    .end()
    .build();
```

#### 4.2 Builder Methods

```cpp
class JsonBuilder {
    auto object() -> JsonBuilder&;
    auto array() -> JsonBuilder&;
    auto field(const std::string& key, auto value) -> JsonBuilder&;
    auto item(auto value) -> JsonBuilder&;
    auto end() -> JsonBuilder&;
    auto build() -> JsonValue;
};
```

### 5. Object/Array Operations

#### 5.1 Merge Objects

```cpp
void JsonValue::merge(JsonValue other);
```

Merges all key-value pairs from `other` into `this` object. Existing keys are replaced.

#### 5.2 Extend Arrays

```cpp
void JsonValue::extend(JsonValue other);
```

Appends all elements from `other` array to `this` array.

### 6. Schema Validation

#### 6.1 JsonSchema

```cpp
auto schema = JsonSchema::object()
    .required("name", JsonSchema::string())
    .required("age", JsonSchema::integer())
    .optional("email", JsonSchema::string());

auto result = schema.validate(json_value);
if (!result.valid) {
    std::cerr << "Error at " << result.path << ": " << result.error;
}
```

#### 6.2 Schema Types

```cpp
static auto any() -> JsonSchema;       // Accept any type
static auto null() -> JsonSchema;      // Expect null
static auto boolean() -> JsonSchema;   // Expect boolean
static auto integer() -> JsonSchema;   // Expect Int64/Uint64
static auto number() -> JsonSchema;    // Expect any number
static auto string() -> JsonSchema;    // Expect string
static auto array() -> JsonSchema;     // Expect array (any elements)
static auto array_of(JsonSchema) -> JsonSchema;  // Typed array
static auto object() -> JsonSchema;    // Expect object (any fields)
```

#### 6.3 Field Validation

```cpp
auto required(const std::string& name, JsonSchema schema) -> JsonSchema&&;
auto optional(const std::string& name, JsonSchema schema) -> JsonSchema&&;
```

#### 6.4 ValidationResult

```cpp
struct ValidationResult {
    bool valid;
    std::string error;  // Error message if invalid
    std::string path;   // JSON path to invalid value (e.g., "users[0].name")
};
```

### 7. JSON-RPC 2.0 Support

#### 7.1 JsonRpcRequest

```cpp
struct JsonRpcRequest {
    std::string method;
    std::optional<JsonValue> params;
    std::optional<JsonValue> id;  // null for notifications

    bool is_notification() const;
    auto to_json() const -> JsonValue;
    static auto from_json(const JsonValue&) -> std::variant<JsonRpcRequest, JsonRpcError>;
};
```

#### 7.2 JsonRpcResponse

```cpp
struct JsonRpcResponse {
    std::optional<JsonValue> result;
    std::optional<JsonRpcError> error;
    JsonValue id;

    bool is_error() const;
    auto to_json() const -> JsonValue;
    static auto from_json(const JsonValue&) -> std::variant<JsonRpcResponse, JsonRpcError>;
    static auto success(JsonValue result, JsonValue id) -> JsonRpcResponse;
    static auto failure(JsonRpcError error, JsonValue id) -> JsonRpcResponse;
};
```

#### 7.3 JsonRpcError

```cpp
struct JsonRpcError {
    int32_t code;
    std::string message;
    std::optional<JsonValue> data;

    static auto from_code(JsonRpcErrorCode code) -> JsonRpcError;
};

enum class JsonRpcErrorCode {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
};
```

## Implementation Files

### Headers (`compiler/include/json/`)

| File | Purpose |
|------|---------|
| `json.hpp` | Main public header (includes all) |
| `json_error.hpp` | JsonError struct with line/column |
| `json_value.hpp` | JsonNumber, JsonValue, JsonArray, JsonObject |
| `json_parser.hpp` | JsonLexer, JsonParser |
| `json_builder.hpp` | Fluent builder API |
| `json_rpc.hpp` | JSON-RPC 2.0 structs |
| `json_schema.hpp` | Schema validation |

### Sources (`compiler/src/json/`)

| File | Purpose |
|------|---------|
| `json_value.cpp` | Value equality, merge, extend |
| `json_parser.cpp` | Lexer + recursive descent parser |
| `json_serializer.cpp` | to_string(), write_to() |
| `json_builder.cpp` | Builder implementation |
| `json_rpc.cpp` | JSON-RPC helpers |
| `json_schema.cpp` | Schema validation |

## Examples

### Basic Parsing and Access

```cpp
#include "json/json.hpp"
using namespace tml::json;

auto result = parse_json(R"({"name": "Alice", "age": 30})");
if (auto* json = std::get_if<JsonValue>(&result)) {
    auto name = json->get("name")->as_string();  // "Alice"
    auto age = json->get("age")->as_i64();       // 30 (integer preserved)
}
```

### Building JSON

```cpp
auto user = JsonBuilder()
    .object()
        .field("name", "Bob")
        .field("email", "bob@example.com")
        .field("active", true)
    .end()
    .build();

std::cout << user.to_string_pretty();
// {
//   "active": true,
//   "email": "bob@example.com",
//   "name": "Bob"
// }
```

### Schema Validation

```cpp
auto schema = JsonSchema::object()
    .required("method", JsonSchema::string())
    .required("params", JsonSchema::array())
    .optional("id", JsonSchema::integer());

auto result = schema.validate(request);
if (!result.valid) {
    std::cerr << "Validation failed: " << result.error << std::endl;
}
```

### JSON-RPC Request/Response

```cpp
// Parse request
auto req = JsonRpcRequest::from_json(parsed);
if (auto* request = std::get_if<JsonRpcRequest>(&req)) {
    if (request->method == "sum") {
        // Handle method...
        auto response = JsonRpcResponse::success(JsonValue(42), *request->id);
        std::cout << response.to_json().to_string();
    }
}
```

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Parse | O(n) | Single pass, zero-copy strings |
| Serialize | O(n) | Single pass output |
| Object lookup | O(log n) | std::map ordered keys |
| Array access | O(1) | std::vector random access |
| Schema validate | O(n) | Recursive traversal |

## Compatibility

- **RFC 8259** - Full JSON specification compliance
- **JSON-RPC 2.0** - Full specification compliance
- **C++17** - Requires C++17 or later
- **Thread Safety** - Parser is stateless, safe for concurrent parsing

## Alternatives Rejected

1. **nlohmann/json** - External dependency, no integer preservation
2. **RapidJSON** - Complex API, SAX/DOM split
3. **simdjson** - SIMD-only, overkill for MCP use case
4. **boost::json** - Boost dependency too large

## References

- [RFC 8259: The JavaScript Object Notation (JSON) Data Interchange Format](https://datatracker.ietf.org/doc/html/rfc8259)
- [JSON-RPC 2.0 Specification](https://www.jsonrpc.org/specification)
- [Model Context Protocol (MCP)](https://modelcontextprotocol.io/)
