# std.json — JSON Parsing and Serialization

## 1. Overview

The `std.json` package provides JSON parsing, serialization, and a dynamic value type.

```tml
use std::json
use std::json::{Json, parse, stringify}
```

> **Implementation Status**: The C++ native implementation is complete in `compiler/src/json/`.
> See [RFC-0015](../rfcs/RFC-0015-JSON.md) for the full specification.
> The TML stdlib bindings (Phase 8 in tasks) are pending.

### Key Features

- **Integer Precision** - Int64/Uint64 preserved without floating-point loss
- **Zero-Copy Parsing** - Uses `string_view` where possible
- **JSON-RPC 2.0** - Full support for MCP integration
- **Schema Validation** - Type checking, required/optional fields
- **Fluent Builder** - Chainable API for constructing JSON

## 2. Capabilities

```tml
// No capabilities required - pure data transformation
```

## 3. JSON Value Type

### 3.1 JsonNumber Type

To preserve integer precision, numbers are stored as a discriminated union:

```tml
pub type JsonNumber =
    | Int(I64)      // Integers without decimal point
    | Uint(U64)     // Large unsigned integers
    | Float(F64)    // Numbers with decimal or exponent
```

### 3.2 Json Type

```tml
pub type Json =
    | Null
    | Bool(Bool)
    | Number(JsonNumber)
    | String(String)
    | Array(List[Json])
    | Object(Map[String, Json])
```

### 3.2 Constructors

```tml
extend Json {
    /// Create null value
    pub func null() -> This { Null }

    /// Create from boolean
    pub func bool(v: Bool) -> This { Bool(v) }

    /// Create from number
    pub func number(v: impl Into[F64]) -> This { Number(v.into()) }

    /// Create from string
    pub func string(v: impl Into[String]) -> This { String(v.into()) }

    /// Create empty array
    pub func array() -> This { Array(List.new()) }

    /// Create from array
    pub func from_array(arr: List[Json]) -> This { Array(arr) }

    /// Create empty object
    pub func object() -> This { Object(Map.new()) }

    /// Create from map
    pub func from_object(map: Map[String, Json]) -> This { Object(map) }
}
```

### 3.3 Type Checks

```tml
extend Json {
    pub func is_null(this) -> Bool {
        when this { Null -> true, _ -> false }
    }

    pub func is_bool(this) -> Bool {
        when this { Bool(_) -> true, _ -> false }
    }

    pub func is_number(this) -> Bool {
        when this { Number(_) -> true, _ -> false }
    }

    pub func is_string(this) -> Bool {
        when this { String(_) -> true, _ -> false }
    }

    pub func is_array(this) -> Bool {
        when this { Array(_) -> true, _ -> false }
    }

    pub func is_object(this) -> Bool {
        when this { Object(_) -> true, _ -> false }
    }

    pub func is_i64(this) -> Bool {
        when this {
            Number(n) -> n.fract() == 0.0 and n >= I64.MIN as F64 and n <= I64.MAX as F64,
            _ -> false,
        }
    }

    pub func is_u64(this) -> Bool {
        when this {
            Number(n) -> n.fract() == 0.0 and n >= 0.0 and n <= U64.MAX as F64,
            _ -> false,
        }
    }
}
```

### 3.4 Value Access

```tml
extend Json {
    /// Get as boolean
    pub func as_bool(this) -> Maybe[Bool] {
        when this { Bool(v) -> Just(v), _ -> Nothing }
    }

    /// Get as f64
    pub func as_f64(this) -> Maybe[F64] {
        when this { Number(v) -> Just(v), _ -> Nothing }
    }

    /// Get as i64
    pub func as_i64(this) -> Maybe[I64] {
        when this {
            Number(v) -> if v.fract() == 0.0 { Just(v as I64) } else { None },
            _ -> Nothing,
        }
    }

    /// Get as u64
    pub func as_u64(this) -> Maybe[U64] {
        when this {
            Number(v) -> if v.fract() == 0.0 and v >= 0.0 { Just(v as U64) } else { None },
            _ -> Nothing,
        }
    }

    /// Get as string
    pub func as_str(this) -> Maybe[ref str] {
        when this { String(ref v) -> Just(v.as_str()), _ -> Nothing }
    }

    /// Get as array
    pub func as_array(this) -> Maybe[ref List[Json]] {
        when this { Array(ref v) -> Just(v), _ -> Nothing }
    }

    /// Get as mutable array
    pub func as_array_mut(this) -> Maybe[mut refList[Json]] {
        when this { Array(ref mut v) -> Just(v), _ -> Nothing }
    }

    /// Get as object
    pub func as_object(this) -> Maybe[ref Map[String, Json]] {
        when this { Object(ref v) -> Just(v), _ -> Nothing }
    }

    /// Get as mutable object
    pub func as_object_mut(this) -> Maybe[mut refMap[String, Json]] {
        when this { Object(ref mut v) -> Just(v), _ -> Nothing }
    }
}
```

### 3.5 Indexing

```tml
extend Json {
    /// Get value by key (for objects)
    pub func get(this, key: ref str) -> Maybe[ref Json] {
        when this {
            Object(ref map) -> map.get(key),
            _ -> Nothing,
        }
    }

    /// Get value by index (for arrays)
    pub func get_index(this, index: U64) -> Maybe[ref Json] {
        when this {
            Array(ref arr) -> arr.get(index),
            _ -> Nothing,
        }
    }

    /// Get mutable value by key
    pub func get_mut(this, key: ref str) -> Maybe[mut refJson] {
        when this {
            Object(ref mut map) -> map.get_mut(key),
            _ -> Nothing,
        }
    }

    /// Get nested value by path
    pub func pointer(this, path: ref str) -> Maybe[ref Json] {
        if path.is_empty() {
            return Just(this)
        }

        var current = this
        loop part in path.trim_start_matches('/').split('/') {
            current = when current {
                Object(map) -> map.get(part)!,
                Array(arr) -> {
                    let idx: U64 = part.parse().ok()!
                    arr.get(idx)!
                },
                _ -> return None,
            }
        }
        return Just(current)
    }
}

// Index operator for objects
extend Json with Index[ref str] {
    type Output = Json

    func index(this, key: ref str) -> ref Json {
        this.get(key).unwrap_or(ref Null)
    }
}

// Index operator for arrays
extend Json with Index[U64] {
    type Output = Json

    func index(this, index: U64) -> ref Json {
        this.get_index(index).unwrap_or(ref Null)
    }
}
```

### 3.6 Mutation

```tml
extend Json {
    /// Set object key
    pub func set(this, key: impl Into[String], value: impl Into[Json]) {
        when this {
            Object(ref mut map) -> { map.insert(key.into(), value.into()); },
            _ -> panic("set called on non-object"),
        }
    }

    /// Push to array
    pub func push(this, value: impl Into[Json]) {
        when this {
            Array(ref mut arr) -> arr.push(value.into()),
            _ -> panic("push called on non-array"),
        }
    }

    /// Remove object key
    pub func remove(this, key: ref str) -> Maybe[Json] {
        when this {
            Object(ref mut map) -> map.remove(key),
            _ -> Nothing,
        }
    }

    /// Take value, replacing with null
    pub func take(this) -> Json {
        std.mem.replace(this, Null)
    }
}
```

## 4. Parsing

### 4.1 Parse Function

```tml
/// Parse JSON string
pub func parse(s: ref str) -> Outcome[Json, ParseError]

/// Parse JSON bytes
pub func parse_bytes(bytes: ref [U8]) -> Outcome[Json, ParseError]

/// Parse from reader
pub func parse_reader[R: Read](reader: R) -> Outcome[Json, ParseError]
```

### 4.2 ParseError

```tml
pub type ParseError {
    kind: ParseErrorKind,
    line: U64,
    column: U64,
    context: String,
}

pub type ParseErrorKind =
    | UnexpectedToken(String)
    | UnexpectedEof
    | InvalidNumber
    | InvalidString
    | InvalidEscape
    | TrailingCharacters
    | RecursionLimit
    | NumberOutOfRange

extend ParseError {
    pub func kind(this) -> &ParseErrorKind
    pub func line(this) -> U64
    pub func column(this) -> U64
}
```

### 4.3 Parser Options

```tml
pub type ParserOptions {
    max_depth: U32,
    allow_trailing_comma: Bool,
    allow_comments: Bool,
}

extend ParserOptions {
    pub func default() -> This {
        return This {
            max_depth: 128,
            allow_trailing_comma: false,
            allow_comments: false,
        }
    }
}

/// Parse with options
pub func parse_with_options(s: ref str, opts: ParserOptions) -> Outcome[Json, ParseError]
```

## 5. Serialization

### 5.1 Stringify Function

```tml
/// Serialize to compact JSON string
pub func stringify(value: ref Json) -> String

/// Serialize to pretty-printed JSON string
pub func stringify_pretty(value: ref Json) -> String

/// Serialize with options
pub func stringify_with_options(value: ref Json, opts: StringifyOptions) -> String

/// Serialize to writer
pub func stringify_to[W: Write](value: ref Json, writer: mut refW) -> Outcome[Unit, IoError]
```

### 5.2 StringifyOptions

```tml
pub type StringifyOptions {
    pretty: Bool,
    indent: String,
    sort_keys: Bool,
}

extend StringifyOptions {
    pub func default() -> This {
        return This {
            pretty: false,
            indent: "  ",
            sort_keys: false,
        }
    }

    pub func pretty(this) -> This { this.pretty = true; this }
    pub func indent(this, indent: ref str) -> This { this.indent = indent.into(); this }
    pub func sort_keys(this) -> This { this.sort_keys = true; this }
}
```

## 6. Serialization Traits

### 6.1 Serialize Trait

```tml
public behaviorSerialize {
    func serialize(this) -> Json
}

// Built-in implementations
extend Bool with Serialize {
    func serialize(this) -> Json { Json.bool(this) }
}

extend I32 with Serialize {
    func serialize(this) -> Json { Json.number(this as F64) }
}

extend I64 with Serialize {
    func serialize(this) -> Json { Json.number(this as F64) }
}

extend F64 with Serialize {
    func serialize(this) -> Json { Json.number(this) }
}

extend String with Serialize {
    func serialize(this) -> Json { Json.string(this.duplicate()) }
}

extend ref str with Serialize {
    func serialize(this) -> Json { Json.string(this.into()) }
}

extend Maybe[T: Serialize] with Serialize {
    func serialize(this) -> Json {
        when this {
            Just(v) -> v.serialize(),
            None -> Json.null(),
        }
    }
}

extend List[T: Serialize] with Serialize {
    func serialize(this) -> Json {
        let arr = this.iter().map(do(v) v.serialize()).collect()
        return Json.from_array(arr)
    }
}

extend Map[String, V: Serialize] with Serialize {
    func serialize(this) -> Json {
        var obj = Map.new()
        loop (k, v) in this.entries() {
            obj.insert(k.duplicate(), v.serialize())
        }
        return Json.from_object(obj)
    }
}
```

### 6.2 Deserialize Trait

```tml
public behaviorDeserialize {
    func deserialize(json: ref Json) -> Outcome[This, DeserializeError]
}

pub type DeserializeError {
    kind: DeserializeErrorKind,
    path: String,
}

pub type DeserializeErrorKind =
    | TypeMismatch { expected: String, found: String }
    | MissingField(String)
    | InvalidValue(String)
    | Custom(String)

// Built-in implementations
extend Bool with Deserialize {
    func deserialize(json: ref Json) -> Outcome[This, DeserializeError] {
        json.as_bool().ok_or(DeserializeError.type_mismatch("bool", json.type_name()))
    }
}

extend I64 with Deserialize {
    func deserialize(json: ref Json) -> Outcome[This, DeserializeError] {
        json.as_i64().ok_or(DeserializeError.type_mismatch("i64", json.type_name()))
    }
}

extend String with Deserialize {
    func deserialize(json: ref Json) -> Outcome[This, DeserializeError> {
        json.as_str().map(String.from).ok_or(DeserializeError.type_mismatch("string", json.type_name()))
    }
}

extend Maybe[T: Deserialize] with Deserialize {
    func deserialize(json: ref Json) -> Outcome[This, DeserializeError> {
        if json.is_null() {
            return Ok(None)
        }
        return Ok(Just(T.deserialize(json)!))
    }
}

extend List[T: Deserialize] with Deserialize {
    func deserialize(json: ref Json) -> Outcome[This, DeserializeError> {
        let arr = json.as_array().ok_or(DeserializeError.type_mismatch("array", json.type_name()))!
        var result = List.with_capacity(arr.len())
        loop (i, item) in arr.iter().enumerate() {
            result.push(T.deserialize(item).map_err(do(e) e.with_path(i.to_string()))!)
        }
        return Ok(result)
    }
}
```

### 6.3 Derive Macros

```tml
// Automatic derive for structs
@derive(Serialize, Deserialize)]
type User {
    name: String,
    email: String,
    age: Maybe[I32],
}

// With field renaming
@derive(Serialize, Deserialize)]
type ApiResponse {
    @json(rename = "statusCode")]
    status_code: I32,

    @json(rename = "data")]
    payload: Json,

    @json(skip_serializing_if = "Option.is_none")]
    error: Maybe[String],
}

// With default values
@derive(Deserialize)]
type Config {
    host: String,

    @json(default = "8080")]
    port: I32,

    @json(default)]
    debug: Bool,
}
```

## 7. JSON Builder

```tml
mod builder

/// Build JSON object
pub func object() -> ObjectBuilder {
    return ObjectBuilder { map: Map.new() }
}

pub type ObjectBuilder {
    map: Map[String, Json],
}

extend ObjectBuilder {
    pub func set(this, key: impl Into[String], value: impl Serialize) -> This {
        this.map.insert(key.into(), value.serialize())
        this
    }

    pub func set_if(this, condition: Bool, key: impl Into[String], value: impl Serialize) -> This {
        if condition {
            this.set(key, value)
        } else {
            this
        }
    }

    pub func merge(this, other: ref Json) -> This {
        when other {
            Object(ref map) -> {
                loop (k, v) in map.entries() {
                    this.map.insert(k.duplicate(), v.duplicate())
                }
            },
            _ -> unit,
        }
        this
    }

    pub func build(this) -> Json {
        return Json.from_object(this.map)
    }
}

/// Build JSON array
pub func array() -> ArrayBuilder {
    return ArrayBuilder { arr: List.new() }
}

pub type ArrayBuilder {
    arr: List[Json],
}

extend ArrayBuilder {
    pub func push(this, value: impl Serialize) -> This {
        this.arr.push(value.serialize())
        this
    }

    pub func extend(this, values: impl IntoIterator[Item = impl Serialize]) -> This {
        loop v in values {
            this.arr.push(v.serialize())
        }
        this
    }

    pub func build(this) -> Json {
        return Json.from_array(this.arr)
    }
}
```

## 8. Streaming Parser

```tml
mod stream

pub type JsonReader[R: Read] {
    reader: R,
    buffer: List[U8],
}

extend JsonReader[R: Read] {
    pub func new(reader: R) -> This

    /// Read next JSON value
    pub func read(this) -> Outcome[Maybe[Json], ParseError]

    /// Read all values (for JSON Lines / NDJSON)
    pub func read_all(this) -> JsonValues[R]
}

pub type JsonValues[R: Read] { ... }

extend JsonValues[R: Read] with Iterator {
    type Item = Outcome[Json, ParseError]
}

pub type JsonWriter[W: Write] {
    writer: W,
}

extend JsonWriter[W: Write] {
    pub func new(writer: W) -> This

    /// Write JSON value with newline (NDJSON)
    pub func write(this, value: ref Json) -> Outcome[Unit, IoError]

    /// Write with custom separator
    pub func write_sep(this, value: ref Json, sep: ref str) -> Outcome[Unit, IoError]

    pub func flush(this) -> Outcome[Unit, IoError]
}
```

## 9. Examples

### 9.1 Basic Usage

```tml
mod json_example
use std::json::{Json, parse, stringify, stringify_pretty}

func main() -> Outcome[Unit, Error] {
    // Parse JSON
    let json = parse("{\"name\": \"Alice\", \"age\": 30}")!

    // Access values
    let name = json["name"].as_str().unwrap()
    let age = json["age"].as_i64().unwrap()
    println("Name: " + name + ", Age: " + age.to_string())

    // Build JSON
    let user = Json.object()
        .set("name", "Bob")
        .set("email", "bob@example.com")
        .set("active", true)
        .build()

    println(stringify_pretty(ref user))

    return Ok(unit)
}
```

### 9.2 Typed Serialization

```tml
mod typed_json
use std::json::{Json, Serialize, Deserialize}

@derive(Serialize, Deserialize)]
type User {
    id: U64,
    name: String,
    email: String,
    roles: List[String],
}

func main() -> Outcome[Unit, Error] {
    // Serialize
    let user = User {
        id: 1,
        name: "Alice",
        email: "alice@example.com",
        roles: ["admin", "user"].into(),
    }

    let json = user.serialize()
    println(stringify_pretty(&json))

    // Deserialize
    let json_str = "{\"id\": 2, \"name\": \"Bob\", \"email\": \"bob@example.com\", \"roles\": [\"user\"]}"
    let parsed = parse(json_str)!
    let user: User = User.deserialize(ref parsed)!

    println("Loaded user: " + user.name)

    return Ok(unit)
}
```

### 9.3 JSON Pointer

```tml
mod pointer_example
use std::json::parse

func main() -> Outcome[Unit, Error] {
    let json = parse("{\"users\": [{\"name\": \"Alice\", \"address\": {\"city\": \"NYC\"}}, {\"name\": \"Bob\", \"address\": {\"city\": \"LA\"}}]}")!

    // Access nested value
    let city_val = json.pointer("/users/0/address/city")
    let city = city_val.and_then(do(v) v.as_str()).unwrap_or("unknown")

    println("First user's city: " + city)

    return Ok(unit)
}
```

## 10. C++ Native Implementation

The compiler includes a native C++ JSON library used internally. This implementation:

### 10.1 Files

| Location | File | Purpose |
|----------|------|---------|
| `compiler/include/json/` | `json.hpp` | Main public header |
| | `json_value.hpp` | JsonNumber, JsonValue, JsonArray, JsonObject |
| | `json_parser.hpp` | Lexer and parser |
| | `json_builder.hpp` | Fluent builder API |
| | `json_rpc.hpp` | JSON-RPC 2.0 structs |
| | `json_schema.hpp` | Schema validation |
| `compiler/src/json/` | `json_parser.cpp` | Lexer + recursive descent parser |
| | `json_serializer.cpp` | to_string(), write_to() |
| | `json_builder.cpp` | Builder implementation |
| | `json_rpc.cpp` | JSON-RPC helpers |
| | `json_schema.cpp` | Schema validation |

### 10.2 C++ Usage Example

```cpp
#include "json/json.hpp"
using namespace tml::json;

// Parse JSON
auto result = parse_json(R"({"name": "Alice", "age": 30})");
if (auto* json = std::get_if<JsonValue>(&result)) {
    auto name = json->get("name")->as_string();  // "Alice"
    auto age = json->get("age")->as_i64();       // 30 (integer preserved!)
}

// Build JSON
auto user = JsonBuilder()
    .object()
        .field("name", "Bob")
        .field("active", true)
    .end()
    .build();

std::cout << user.to_string_pretty();

// Schema validation
auto schema = JsonSchema::object()
    .required("name", JsonSchema::string())
    .required("age", JsonSchema::integer());

auto validation = schema.validate(some_json);
if (!validation.valid) {
    std::cerr << "Error: " << validation.error << std::endl;
}
```

### 10.3 JSON-RPC Example

```cpp
// Parse JSON-RPC request
auto req = JsonRpcRequest::from_json(parsed);
if (auto* request = std::get_if<JsonRpcRequest>(&req)) {
    // Handle method...
    auto response = JsonRpcResponse::success(JsonValue(42), *request->id);
    std::cout << response.to_json().to_string();
}
```

See [RFC-0015](../rfcs/RFC-0015-JSON.md) for the complete specification.

---

*Previous: [08-COMPRESS.md](./08-COMPRESS.md)*
*Next: [10-REGEX.md](./10-REGEX.md) — Regular Expressions*
