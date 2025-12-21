# std.json — JSON Parsing and Serialization

## 1. Overview

The `std.json` package provides JSON parsing, serialization, and a dynamic value type.

```tml
import std.json
import std.json.{Json, parse, stringify}
```

## 2. Capabilities

```tml
// No capabilities required - pure data transformation
```

## 3. JSON Value Type

### 3.1 Json Type

```tml
public type Json =
    | Null
    | Bool(Bool)
    | Number(F64)
    | String(String)
    | Array(List[Json])
    | Object(Map[String, Json])
```

### 3.2 Constructors

```tml
extend Json {
    /// Create null value
    public func null() -> This { Null }

    /// Create from boolean
    public func bool(v: Bool) -> This { Bool(v) }

    /// Create from number
    public func number(v: impl Into[F64]) -> This { Number(v.into()) }

    /// Create from string
    public func string(v: impl Into[String]) -> This { String(v.into()) }

    /// Create empty array
    public func array() -> This { Array(List.new()) }

    /// Create from array
    public func from_array(arr: List[Json]) -> This { Array(arr) }

    /// Create empty object
    public func object() -> This { Object(Map.new()) }

    /// Create from map
    public func from_object(map: Map[String, Json]) -> This { Object(map) }
}
```

### 3.3 Type Checks

```tml
extend Json {
    public func is_null(this) -> Bool {
        when this { Null -> true, _ -> false }
    }

    public func is_bool(this) -> Bool {
        when this { Bool(_) -> true, _ -> false }
    }

    public func is_number(this) -> Bool {
        when this { Number(_) -> true, _ -> false }
    }

    public func is_string(this) -> Bool {
        when this { String(_) -> true, _ -> false }
    }

    public func is_array(this) -> Bool {
        when this { Array(_) -> true, _ -> false }
    }

    public func is_object(this) -> Bool {
        when this { Object(_) -> true, _ -> false }
    }

    public func is_i64(this) -> Bool {
        when this {
            Number(n) -> n.fract() == 0.0 and n >= I64.MIN as F64 and n <= I64.MAX as F64,
            _ -> false,
        }
    }

    public func is_u64(this) -> Bool {
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
    public func as_bool(this) -> Option[Bool] {
        when this { Bool(v) -> Some(v), _ -> None }
    }

    /// Get as f64
    public func as_f64(this) -> Option[F64] {
        when this { Number(v) -> Some(v), _ -> None }
    }

    /// Get as i64
    public func as_i64(this) -> Option[I64] {
        when this {
            Number(v) -> if v.fract() == 0.0 { Some(v as I64) } else { None },
            _ -> None,
        }
    }

    /// Get as u64
    public func as_u64(this) -> Option[U64] {
        when this {
            Number(v) -> if v.fract() == 0.0 and v >= 0.0 { Some(v as U64) } else { None },
            _ -> None,
        }
    }

    /// Get as string
    public func as_str(this) -> Option[&str] {
        when this { String(ref v) -> Some(v.as_str()), _ -> None }
    }

    /// Get as array
    public func as_array(this) -> Option[&List[Json]] {
        when this { Array(ref v) -> Some(v), _ -> None }
    }

    /// Get as mutable array
    public func as_array_mut(this) -> Option[&mut List[Json]] {
        when this { Array(ref mut v) -> Some(v), _ -> None }
    }

    /// Get as object
    public func as_object(this) -> Option[&Map[String, Json]] {
        when this { Object(ref v) -> Some(v), _ -> None }
    }

    /// Get as mutable object
    public func as_object_mut(this) -> Option[&mut Map[String, Json]] {
        when this { Object(ref mut v) -> Some(v), _ -> None }
    }
}
```

### 3.5 Indexing

```tml
extend Json {
    /// Get value by key (for objects)
    public func get(this, key: &str) -> Option[&Json] {
        when this {
            Object(ref map) -> map.get(key),
            _ -> None,
        }
    }

    /// Get value by index (for arrays)
    public func get_index(this, index: U64) -> Option[&Json] {
        when this {
            Array(ref arr) -> arr.get(index),
            _ -> None,
        }
    }

    /// Get mutable value by key
    public func get_mut(this, key: &str) -> Option[&mut Json] {
        when this {
            Object(ref mut map) -> map.get_mut(key),
            _ -> None,
        }
    }

    /// Get nested value by path
    public func pointer(this, path: &str) -> Option[&Json] {
        if path.is_empty() {
            return Some(this)
        }

        var current = this
        loop part in path.trim_start_matches('/').split('/') {
            current = when current {
                Object(map) -> map.get(part)?,
                Array(arr) -> {
                    let idx: U64 = part.parse().ok()?
                    arr.get(idx)?
                },
                _ -> return None,
            }
        }
        return Some(current)
    }
}

// Index operator for objects
extend Json with Index[&str] {
    type Output = Json

    func index(this, key: &str) -> &Json {
        this.get(key).unwrap_or(&Null)
    }
}

// Index operator for arrays
extend Json with Index[U64] {
    type Output = Json

    func index(this, index: U64) -> &Json {
        this.get_index(index).unwrap_or(&Null)
    }
}
```

### 3.6 Mutation

```tml
extend Json {
    /// Set object key
    public func set(this, key: impl Into[String], value: impl Into[Json]) {
        when this {
            Object(ref mut map) -> { map.insert(key.into(), value.into()); },
            _ -> panic("set called on non-object"),
        }
    }

    /// Push to array
    public func push(this, value: impl Into[Json]) {
        when this {
            Array(ref mut arr) -> arr.push(value.into()),
            _ -> panic("push called on non-array"),
        }
    }

    /// Remove object key
    public func remove(this, key: &str) -> Option[Json] {
        when this {
            Object(ref mut map) -> map.remove(key),
            _ -> None,
        }
    }

    /// Take value, replacing with null
    public func take(this) -> Json {
        std.mem.replace(this, Null)
    }
}
```

## 4. Parsing

### 4.1 Parse Function

```tml
/// Parse JSON string
public func parse(s: &str) -> Result[Json, ParseError]

/// Parse JSON bytes
public func parse_bytes(bytes: &[U8]) -> Result[Json, ParseError]

/// Parse from reader
public func parse_reader[R: Read](reader: R) -> Result[Json, ParseError]
```

### 4.2 ParseError

```tml
public type ParseError {
    kind: ParseErrorKind,
    line: U64,
    column: U64,
    context: String,
}

public type ParseErrorKind =
    | UnexpectedToken(String)
    | UnexpectedEof
    | InvalidNumber
    | InvalidString
    | InvalidEscape
    | TrailingCharacters
    | RecursionLimit
    | NumberOutOfRange

extend ParseError {
    public func kind(this) -> &ParseErrorKind
    public func line(this) -> U64
    public func column(this) -> U64
}
```

### 4.3 Parser Options

```tml
public type ParserOptions {
    max_depth: U32,
    allow_trailing_comma: Bool,
    allow_comments: Bool,
}

extend ParserOptions {
    public func default() -> This {
        return This {
            max_depth: 128,
            allow_trailing_comma: false,
            allow_comments: false,
        }
    }
}

/// Parse with options
public func parse_with_options(s: &str, opts: ParserOptions) -> Result[Json, ParseError]
```

## 5. Serialization

### 5.1 Stringify Function

```tml
/// Serialize to compact JSON string
public func stringify(value: &Json) -> String

/// Serialize to pretty-printed JSON string
public func stringify_pretty(value: &Json) -> String

/// Serialize with options
public func stringify_with_options(value: &Json, opts: StringifyOptions) -> String

/// Serialize to writer
public func stringify_to[W: Write](value: &Json, writer: &mut W) -> Result[Unit, IoError]
```

### 5.2 StringifyOptions

```tml
public type StringifyOptions {
    pretty: Bool,
    indent: String,
    sort_keys: Bool,
}

extend StringifyOptions {
    public func default() -> This {
        return This {
            pretty: false,
            indent: "  ",
            sort_keys: false,
        }
    }

    public func pretty(this) -> This { this.pretty = true; this }
    public func indent(this, indent: &str) -> This { this.indent = indent.into(); this }
    public func sort_keys(this) -> This { this.sort_keys = true; this }
}
```

## 6. Serialization Traits

### 6.1 Serialize Trait

```tml
public trait Serialize {
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
    func serialize(this) -> Json { Json.string(this.clone()) }
}

extend &str with Serialize {
    func serialize(this) -> Json { Json.string(this.into()) }
}

extend Option[T: Serialize] with Serialize {
    func serialize(this) -> Json {
        when this {
            Some(v) -> v.serialize(),
            None -> Json.null(),
        }
    }
}

extend List[T: Serialize] with Serialize {
    func serialize(this) -> Json {
        let arr = this.iter().map(|v| v.serialize()).collect()
        return Json.from_array(arr)
    }
}

extend Map[String, V: Serialize] with Serialize {
    func serialize(this) -> Json {
        var obj = Map.new()
        loop (k, v) in this.entries() {
            obj.insert(k.clone(), v.serialize())
        }
        return Json.from_object(obj)
    }
}
```

### 6.2 Deserialize Trait

```tml
public trait Deserialize {
    func deserialize(json: &Json) -> Result[This, DeserializeError]
}

public type DeserializeError {
    kind: DeserializeErrorKind,
    path: String,
}

public type DeserializeErrorKind =
    | TypeMismatch { expected: String, found: String }
    | MissingField(String)
    | InvalidValue(String)
    | Custom(String)

// Built-in implementations
extend Bool with Deserialize {
    func deserialize(json: &Json) -> Result[This, DeserializeError] {
        json.as_bool().ok_or(DeserializeError.type_mismatch("bool", json.type_name()))
    }
}

extend I64 with Deserialize {
    func deserialize(json: &Json) -> Result[This, DeserializeError] {
        json.as_i64().ok_or(DeserializeError.type_mismatch("i64", json.type_name()))
    }
}

extend String with Deserialize {
    func deserialize(json: &Json) -> Result[This, DeserializeError> {
        json.as_str().map(String.from).ok_or(DeserializeError.type_mismatch("string", json.type_name()))
    }
}

extend Option[T: Deserialize] with Deserialize {
    func deserialize(json: &Json) -> Result[This, DeserializeError> {
        if json.is_null() {
            return Ok(None)
        }
        return Ok(Some(T.deserialize(json)?))
    }
}

extend List[T: Deserialize] with Deserialize {
    func deserialize(json: &Json) -> Result[This, DeserializeError> {
        let arr = json.as_array().ok_or(DeserializeError.type_mismatch("array", json.type_name()))?
        var result = List.with_capacity(arr.len())
        loop (i, item) in arr.iter().enumerate() {
            result.push(T.deserialize(item).map_err(|e| e.with_path(i.to_string()))?)
        }
        return Ok(result)
    }
}
```

### 6.3 Derive Macros

```tml
// Automatic derive for structs
#[derive(Serialize, Deserialize)]
type User {
    name: String,
    email: String,
    age: Option[I32],
}

// With field renaming
#[derive(Serialize, Deserialize)]
type ApiResponse {
    #[json(rename = "statusCode")]
    status_code: I32,

    #[json(rename = "data")]
    payload: Json,

    #[json(skip_serializing_if = "Option.is_none")]
    error: Option[String],
}

// With default values
#[derive(Deserialize)]
type Config {
    host: String,

    #[json(default = "8080")]
    port: I32,

    #[json(default)]
    debug: Bool,
}
```

## 7. JSON Builder

```tml
module builder

/// Build JSON object
public func object() -> ObjectBuilder {
    return ObjectBuilder { map: Map.new() }
}

public type ObjectBuilder {
    map: Map[String, Json],
}

extend ObjectBuilder {
    public func set(this, key: impl Into[String], value: impl Serialize) -> This {
        this.map.insert(key.into(), value.serialize())
        this
    }

    public func set_if(this, condition: Bool, key: impl Into[String], value: impl Serialize) -> This {
        if condition {
            this.set(key, value)
        } else {
            this
        }
    }

    public func merge(this, other: &Json) -> This {
        when other {
            Object(ref map) -> {
                loop (k, v) in map.entries() {
                    this.map.insert(k.clone(), v.clone())
                }
            },
            _ -> unit,
        }
        this
    }

    public func build(this) -> Json {
        return Json.from_object(this.map)
    }
}

/// Build JSON array
public func array() -> ArrayBuilder {
    return ArrayBuilder { arr: List.new() }
}

public type ArrayBuilder {
    arr: List[Json],
}

extend ArrayBuilder {
    public func push(this, value: impl Serialize) -> This {
        this.arr.push(value.serialize())
        this
    }

    public func extend(this, values: impl IntoIterator[Item = impl Serialize]) -> This {
        loop v in values {
            this.arr.push(v.serialize())
        }
        this
    }

    public func build(this) -> Json {
        return Json.from_array(this.arr)
    }
}
```

## 8. Streaming Parser

```tml
module stream

public type JsonReader[R: Read] {
    reader: R,
    buffer: List[U8],
}

extend JsonReader[R: Read] {
    public func new(reader: R) -> This

    /// Read next JSON value
    public func read(this) -> Result[Option[Json], ParseError]

    /// Read all values (for JSON Lines / NDJSON)
    public func read_all(this) -> JsonValues[R]
}

public type JsonValues[R: Read] { ... }

extend JsonValues[R: Read] with Iterator {
    type Item = Result[Json, ParseError]
}

public type JsonWriter[W: Write] {
    writer: W,
}

extend JsonWriter[W: Write] {
    public func new(writer: W) -> This

    /// Write JSON value with newline (NDJSON)
    public func write(this, value: &Json) -> Result[Unit, IoError]

    /// Write with custom separator
    public func write_sep(this, value: &Json, sep: &str) -> Result[Unit, IoError]

    public func flush(this) -> Result[Unit, IoError]
}
```

## 9. Examples

### 9.1 Basic Usage

```tml
module json_example
import std.json.{Json, parse, stringify, stringify_pretty}

func main() -> Result[Unit, Error] {
    // Parse JSON
    let json = parse(r#"{"name": "Alice", "age": 30}"#)?

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

    println(stringify_pretty(&user))

    return Ok(unit)
}
```

### 9.2 Typed Serialization

```tml
module typed_json
import std.json.{Json, Serialize, Deserialize}

#[derive(Serialize, Deserialize)]
type User {
    id: U64,
    name: String,
    email: String,
    roles: List[String],
}

func main() -> Result[Unit, Error] {
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
    let json_str = r#"{"id": 2, "name": "Bob", "email": "bob@example.com", "roles": ["user"]}"#
    let parsed = parse(json_str)?
    let user: User = User.deserialize(&parsed)?

    println("Loaded user: " + user.name)

    return Ok(unit)
}
```

### 9.3 JSON Pointer

```tml
module pointer_example
import std.json.parse

func main() -> Result[Unit, Error] {
    let json = parse(r#"
    {
        "users": [
            {"name": "Alice", "address": {"city": "NYC"}},
            {"name": "Bob", "address": {"city": "LA"}}
        ]
    }
    "#)?

    // Access nested value
    let city = json.pointer("/users/0/address/city")
        .and_then(|v| v.as_str())
        .unwrap_or("unknown")

    println("First user's city: " + city)

    return Ok(unit)
}
```

---

*Previous: [08-COMPRESS.md](./08-COMPRESS.md)*
*Next: [10-REGEX.md](./10-REGEX.md) — Regular Expressions*
