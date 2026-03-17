# Chapter 21 — Working with JSON

JSON (JavaScript Object Notation) is the dominant format for web APIs, configuration
files, and data interchange. TML's `std::json` module is a pure-TML implementation
with native integer precision preservation, a fluent builder API, typed serialization
behaviors, and a streaming interface for large payloads.

## Parsing JSON

The `parse` function takes a `Str` and returns `Outcome[Json, ParseError]`. Use the
`!` operator to propagate errors or handle them explicitly with `when`:

```tml
use std::json::{parse, stringify, Json}

func main() -> Outcome[Unit, Error] {
    let json: Json = parse("{\"name\": \"Alice\", \"age\": 30}")!

    let name: Str = json["name"].as_str().unwrap_or("unknown")
    let age: I64  = json["age"].as_i64().unwrap_or(0)

    println("Name: {name}, Age: {age}")

    Ok(unit)
}
```

## Number Precision

JavaScript's JSON spec uses 64-bit floating point for all numbers, which loses
precision for large integers. TML's parser preserves numeric types as a discriminated
union so you never lose bits:

```tml
let int_val:  Json = parse("42")!                   // stored as Int(I64)
let uint_val: Json = parse("18446744073709551615")!  // stored as Uint(U64)
let float_val: Json = parse("3.14")!                // stored as Float(F64)

println(int_val.is_integer())   // true
println(uint_val.is_integer())  // true
println(float_val.is_float())   // true
```

### Type Check Methods

| Method | Returns `true` when |
|--------|---------------------|
| `is_null()` | Value is JSON `null` |
| `is_bool()` | Value is `true` or `false` |
| `is_number()` | Value is any numeric type |
| `is_integer()` | Value is `Int(I64)` or `Uint(U64)` |
| `is_float()` | Value is `Float(F64)` |
| `is_string()` | Value is a JSON string |
| `is_array()` | Value is a JSON array |
| `is_object()` | Value is a JSON object |

## Accessing Values

### Safe Accessors

Every accessor returns `Maybe[T]`. Use `unwrap_or` to supply a default, or
`when` to branch explicitly:

```tml
let json: Json = parse("{\"user\": {\"score\": 95, \"active\": true}}")!

let user: Json = json["user"]

// Option 1: unwrap with default
let score: I64 = user["score"].as_i64().unwrap_or(0)

// Option 2: pattern match
when user["active"].as_bool() {
    Just(flag) => println("active: {flag}"),
    Nothing    => println("active field missing"),
}
```

### Accessor Methods

| Method | Return type | Notes |
|--------|-------------|-------|
| `as_str()` | `Maybe[Str]` | |
| `as_i64()` | `Maybe[I64]` | Coerces `Uint` if it fits |
| `as_u64()` | `Maybe[U64]` | Use for large unsigned integers |
| `as_f64()` | `Maybe[F64]` | Coerces integers to float |
| `as_bool()` | `Maybe[Bool]` | |
| `as_array()` | `Maybe[ref List[Json]]` | |
| `as_object()` | `Maybe[ref HashMap[Str, Json]]` | |

### Nested Object Access

Square-bracket indexing works on both objects (by `Str`) and arrays (by `I64`).
Accessing a missing key returns `Json::Null` rather than panicking:

```tml
let json: Json = parse("{\"a\": {\"b\": {\"c\": 42}}}")!

let c: I64 = json["a"]["b"]["c"].as_i64().unwrap_or(0)  // 42
let x: I64 = json["a"]["z"]["c"].as_i64().unwrap_or(0)  // 0 — safe
```

### JSON Pointer Navigation

JSON Pointer (RFC 6901) lets you address nested values with a path string:

```tml
let json: Json = parse("{\"users\": [{\"name\": \"Bob\"}, {\"name\": \"Carol\"}]}")!

let name: Str = json
    .pointer("/users/1/name")
    .and_then(do(v) v.as_str())
    .unwrap_or("unknown")  // "Carol"
```

### Array Iteration

```tml
let json: Json = parse("[10, 20, 30, 40]")!

loop item in json.as_array().unwrap() {
    let n: I64 = item.as_i64().unwrap_or(0)
    println(n)
}

// Or collect into a typed list
let numbers: List[I64] = json.as_array().unwrap()
    .map(do(v) v.as_i64().unwrap_or(0))
    .collect()
```

## Building JSON

### The Builder API

The `object()` and `array()` builders produce `Json` values without string
interpolation or manual escaping:

```tml
use std::json::builder::{object, array}
use std::json::stringify_pretty

let user: Json = object()
    .set("id", 1)
    .set("name", "Charlie")
    .set("email", "charlie@example.com")
    .set("active", true)
    .build()

println(stringify_pretty(ref user))
// {
//   "active": true,
//   "email": "charlie@example.com",
//   "id": 1,
//   "name": "Charlie"
// }
```

### Nested Structures

Builders compose: pass the result of one `.build()` as the value in another:

```tml
let response: Json = object()
    .set("status", "ok")
    .set("data", object()
        .set("users", array()
            .push(object().set("id", 1).set("name", "Alice").build())
            .push(object().set("id", 2).set("name", "Bob").build())
            .build())
        .build())
    .build()
```

### Null and Literal Values

```tml
let val: Json = object()
    .set("result", Json::Null)
    .set("count", 0)
    .set("ratio", 0.75)
    .build()
```

## Serialization

### Compact Output

```tml
use std::json::stringify

let json: Json = object().set("key", "value").set("n", 42).build()
let compact: Str = stringify(ref json)
// {"key":"value","n":42}
```

### Pretty-Printed Output

```tml
use std::json::stringify_pretty

let pretty: Str = stringify_pretty(ref json)
// {
//   "key": "value",
//   "n": 42
// }
```

### Custom Options

```tml
use std::json::{stringify_with_options, StringifyOptions}

let opts = StringifyOptions::default()
    .pretty()
    .indent("    ")  // 4 spaces instead of 2
    .sort_keys()     // alphabetical key order

let output: Str = stringify_with_options(ref json, opts)
```

## Typed Serialization

### The `Serialize` and `Deserialize` Behaviors

For types that map cleanly to JSON, implement `Serialize` and `Deserialize`
to convert between your domain types and `Json` values:

```tml
use std::json::{Json, Serialize, Deserialize, DeserializeError}
use std::json::builder::object

type User {
    id: I64,
    name: Str,
    email: Str,
    active: Bool,
}

extend User : Serialize {
    func serialize(ref self) -> Json {
        object()
            .set("id", self.id)
            .set("name", self.name)
            .set("email", self.email)
            .set("active", self.active)
            .build()
    }
}

extend User : Deserialize {
    func deserialize(json: ref Json) -> Outcome[User, DeserializeError] {
        Ok(User {
            id:     I64.deserialize(ref json["id"])!,
            name:   Str.deserialize(ref json["name"])!,
            email:  Str.deserialize(ref json["email"])!,
            active: Bool.deserialize(ref json["active"])!,
        })
    }
}
```

Usage:

```tml
func main() -> Outcome[Unit, Error] {
    // Serialize
    let user = User { id: 1, name: "Diana", email: "diana@example.com", active: true }
    let json: Json = user.serialize()
    println(stringify(ref json))

    // Deserialize
    let raw: Json = parse("{\"id\": 2, \"name\": \"Eve\", \"email\": \"eve@example.com\", \"active\": false}")!
    let parsed_user: User = User.deserialize(ref raw)!
    println(parsed_user.name)  // "Eve"

    Ok(unit)
}
```

### Derive for Simple Structs

For structs where field names match JSON keys, `@derive` generates both behaviors
automatically:

```tml
@derive(Serialize, Deserialize)
type Config {
    host: Str,
    port: I32,
    debug: Bool,
    max_connections: I32,
}

let cfg = Config { host: "localhost", port: 8080, debug: false, max_connections: 100 }
let json: Json = cfg.serialize()
let parsed: Config = Config.deserialize(ref json)!
```

### Field Attributes

Control how individual fields are serialized:

```tml
@derive(Serialize, Deserialize)
type ApiResponse {
    // Serialize with a different JSON key name
    @json(rename = "statusCode")
    status_code: I32,

    // Omit the field when serializing if it is Nothing
    @json(skip_serializing_if = "Maybe::is_nothing")
    error_message: Maybe[Str],

    // Use a default value when the key is absent during deserialization
    @json(default = "8080")
    port: I32,
}
```

## Modifying JSON in Place

JSON values support in-place mutation:

```tml
var json: Json = parse("{\"count\": 0, \"items\": []}")!

// Object mutation
json.set("count", 5)
json.set("label", "results")
json.remove("obsolete_key")

// Array mutation
json["items"].push(object().set("id", 1).build())
json["items"].push(object().set("id", 2).build())

// Merge two objects (overlay wins on conflict)
let overlay: Json = parse("{\"count\": 10, \"extra\": true}")!
json.merge(ref overlay)

// Extend an array with another array
var list1: Json = parse("[1, 2, 3]")!
let list2: Json = parse("[4, 5, 6]")!
list1.extend(ref list2)
```

## Error Handling

### Parse Errors

```tml
use std::json::{parse, ParseError}

let result = parse("{ bad json }")
when result {
    Ok(json) => println(stringify(ref json)),
    Err(e)   => {
        println("parse error at line {e.line}, column {e.column}")
        println("message: {e.message}")
    },
}
```

### Common Parse Errors

| Error Kind | Cause |
|------------|-------|
| `UnexpectedToken` | Invalid character or structure |
| `UnexpectedEof` | Input ended before the value was complete |
| `InvalidNumber` | Malformed numeric literal |
| `InvalidString` | Unterminated string or invalid escape sequence |
| `InvalidEscape` | Unrecognized `\X` escape in a string |
| `RecursionLimit` | Object or array nesting exceeds 1000 levels |

### Validation Pattern

Validate required fields early and return a typed error:

```tml
func parse_config(json: ref Json) -> Outcome[Config, Error] {
    let host: Str = json["host"].as_str()
        .ok_or(Error::new("config: missing 'host' field"))?
    let port: I64 = json["port"].as_i64()
        .ok_or(Error::new("config: missing 'port' field"))?

    if port < 1 or port > 65535 {
        return Err(Error::new("config: port must be 1–65535"))
    }

    Ok(Config { host: host, port: port as I32 })
}
```

## Streaming JSON

For large files or network responses, the streaming interface avoids loading the
entire document into memory at once.

### Reading NDJSON (Newline-Delimited JSON)

```tml
use std::json::stream::{JsonReader}
use std::file::File

func main() -> Outcome[Unit, Error] {
    let file = File::open("events.jsonl")!
    let reader = JsonReader::new(file)

    loop line in reader.read_all() {
        when line {
            Ok(json)  => process_event(json),
            Err(e)    => println("skipping invalid line: {e.message}"),
        }
    }

    Ok(unit)
}
```

### Writing NDJSON

```tml
use std::json::stream::JsonWriter
use std::file::File

func main() -> Outcome[Unit, Error] {
    let file = File::create("output.jsonl")!
    var writer = JsonWriter::new(file)

    loop event in events {
        let json: Json = event.serialize()
        writer.write(ref json)!
    }
    writer.flush()!

    Ok(unit)
}
```

## JSON-RPC

The `std::json::rpc` module provides a JSON-RPC 2.0 client and server:

```tml
use std::json::rpc::{RpcClient, RpcServer, RpcRequest, RpcResponse}

// Client: call a remote procedure
let client = RpcClient::new("https://api.example.com/rpc")
let result: Json = client.call("add", [Json::Int(1), Json::Int(2)])!
println(result.as_i64().unwrap())  // 3

// Server: handle incoming requests
var server = RpcServer::new()

server.register("add", do(params: ref Json) -> Outcome[Json, Error] {
    let a: I64 = params[0].as_i64().unwrap_or(0)
    let b: I64 = params[1].as_i64().unwrap_or(0)
    Ok(Json::Int(a + b))
})

server.listen("0.0.0.0:8080")!
```

## Best Practices

### Use typed access, not raw indexing

```tml
// Prefer
let name: Str = json["name"].as_str().unwrap_or("unknown")

// Over raw access that panics on type mismatch
let name = json["name"]  // type is Json, not Str — easy to misuse
```

### Use `@derive` for data transfer objects

Let the compiler generate serialization rather than writing it by hand:

```tml
@derive(Serialize, Deserialize)
type Message {
    id:        U64,
    content:   Str,
    timestamp: I64,
}
```

### Handle large integers explicitly

Integers beyond `I64::MAX` are stored as `Uint(U64)`. Always check before
calling `as_i64()` on values that might overflow:

```tml
let val: Json = parse("18446744073709551615")!

if val.is_integer() {
    if let Just(n) = val.as_u64() {
        println("large uint: {n}")
    }
}
```

### Stream large files

Never load gigabyte JSON logs into memory. Use `JsonReader` with NDJSON or
parse the file in structured chunks:

```tml
let reader = JsonReader::new(file)
loop line in reader.read_all() {
    // process one record at a time
}
```

---

*Previous: [Chapter 20 — Standard Library](ch20-00-standard-library.md)*
*Next: [Chapter 22 — Cryptography](ch22-00-crypto.md)*
