# Working with JSON

JSON (JavaScript Object Notation) is a lightweight data interchange format widely used for APIs, configuration files, and data storage. TML provides native JSON support with integer precision preservation and a fluent API.

## Basic Parsing

Parse JSON strings into structured values:

```tml
use std::json::{parse, Json}

func main() -> Outcome[Unit, Error] {
    let json = parse("{\"name\": \"Alice\", \"age\": 30}")!

    // Access values
    let name = json["name"].as_str().unwrap()
    let age = json["age"].as_i64().unwrap()

    println("Name: {name}, Age: {age}")

    Ok(unit)
}
```

## JSON Value Types

TML's JSON implementation preserves integer precision using a discriminated union:

```tml
// Numbers are stored as one of:
// - Int(I64)   - integers without decimal
// - Uint(U64)  - large unsigned integers
// - Float(F64) - numbers with decimal or exponent

let integer = parse("42")!           // Stored as Int(42)
let large = parse("18446744073709551615")!  // Stored as Uint
let float = parse("3.14")!           // Stored as Float(3.14)
```

### Type Checking

```tml
let json = parse("{\"count\": 100, \"ratio\": 0.5}")!

let count_val = json["count"]
if count_val.is_integer() {
    let count = count_val.as_i64().unwrap()
    println("count is an integer: {count}")
}

let ratio_val = json["ratio"]
if ratio_val.is_float() {
    let ratio = ratio_val.as_f64().unwrap()
    println("ratio is a float: {ratio}")
}
```

### Type Check Methods

| Method | Returns true if |
|--------|-----------------|
| `is_null()` | Value is null |
| `is_bool()` | Value is true or false |
| `is_number()` | Value is any number |
| `is_integer()` | Value is Int64 or Uint64 |
| `is_float()` | Value is Float64 |
| `is_string()` | Value is a string |
| `is_array()` | Value is an array |
| `is_object()` | Value is an object |

## Accessing Values

### Safe Accessors

Use `Maybe` returning methods for safe access:

```tml
let json = parse("{\"name\": \"Bob\", \"age\": 25}")!

// Returns Maybe[T]
when json["name"].as_str() {
    Just(name) -> println("Name: {name}"),
    Nothing -> println("No name found"),
}

// Chain with unwrap_or for defaults
let age = json["age"].as_i64().unwrap_or(0)
```

### Object Navigation

```tml
let json = parse("{\"user\": {\"profile\": {\"email\": \"alice@example.com\"}}}")!

// Nested access
let email = json["user"]["profile"]["email"].as_str()

// JSON Pointer syntax
let email_val = json.pointer("/user/profile/email")
let email = email_val.and_then(do(v) v.as_str()).unwrap_or("unknown")
```

### Array Access

```tml
let json = parse("[10, 20, 30, 40, 50]")!

// By index
let first = json[0].as_i64().unwrap()  // 10
let third = json[2].as_i64().unwrap()  // 30

// Iterate
loop item in json.as_array().unwrap() {
    let val = item.as_i64().unwrap()
    println("{val}")
}
```

## Building JSON

### Using the Builder API

```tml
use std::json::builder::{object, array}

let user = object()
    .set("name", "Charlie")
    .set("email", "charlie@example.com")
    .set("age", 28)
    .set("active", true)
    .build()

println(stringify_pretty(ref user))
// {
//   "active": true,
//   "age": 28,
//   "email": "charlie@example.com",
//   "name": "Charlie"
// }
```

### Nested Structures

```tml
let response = object()
    .set("status", "success")
    .set("data", object()
        .set("users", array()
            .push(object()
                .set("id", 1)
                .set("name", "Alice")
                .build())
            .push(object()
                .set("id", 2)
                .set("name", "Bob")
                .build())
            .build())
        .build())
    .build()
```

## Serialization

### Compact Output

```tml
let json = object()
    .set("key", "value")
    .build()

let compact = stringify(ref json)
// {"key":"value"}
```

### Pretty Printed

```tml
let pretty = stringify_pretty(ref json)
// {
//   "key": "value"
// }
```

### Custom Options

```tml
let opts = StringifyOptions.default()
    .pretty()
    .indent("    ")  // 4 spaces
    .sort_keys()

let output = stringify_with_options(ref json, opts)
```

## Typed Serialization

### Serialize Behavior

Implement `Serialize` to convert types to JSON:

```tml
use std::json::{Json, Serialize}

type User {
    name: String,
    email: String,
    age: I32,
}

extend User with Serialize {
    func serialize(this) -> Json {
        object()
            .set("name", this.name)
            .set("email", this.email)
            .set("age", this.age)
            .build()
    }
}

// Usage
let user = User { name: "Diana", email: "diana@example.com", age: 32 }
let json = user.serialize()
```

### Deserialize Behavior

Implement `Deserialize` to parse JSON into types:

```tml
use std::json::{Json, Deserialize, DeserializeError}

extend User with Deserialize {
    func deserialize(json: ref Json) -> Outcome[This, DeserializeError] {
        Ok(User {
            name: String.deserialize(json["name"])!,
            email: String.deserialize(json["email"])!,
            age: I32.deserialize(json["age"])!,
        })
    }
}

// Usage
let json = parse("{\"name\": \"Eve\", \"email\": \"eve@example.com\", \"age\": 28}")!
let user: User = User.deserialize(ref json)!
```

### Derive Macros

For simple structs, use `@derive`:

```tml
@derive(Serialize, Deserialize)
type Config {
    host: String,
    port: I32,
    debug: Bool,
}

// Automatic serialization/deserialization
let config = Config { host: "localhost", port: 8080, debug: true }
let json = config.serialize()
let parsed: Config = Config.deserialize(ref some_json)!
```

### Field Attributes

```tml
@derive(Serialize, Deserialize)
type ApiResponse {
    @json(rename = "statusCode")
    status_code: I32,

    @json(skip_serializing_if = "Maybe.is_nothing")
    error: Maybe[String],

    @json(default = "8080")
    port: I32,
}
```

## Error Handling

### Parse Errors

```tml
let result = parse("invalid json")

when result {
    Ok(json) -> println("Parsed: {stringify(ref json)}"),
    Err(error) -> {
        println("Parse error at line {error.line}, column {error.column}")
        println("Error: {error.message}")
    },
}
```

### Common Parse Errors

| Error | Cause |
|-------|-------|
| `UnexpectedToken` | Invalid syntax |
| `UnexpectedEof` | Incomplete JSON |
| `InvalidNumber` | Malformed number |
| `InvalidString` | Unterminated or invalid escape |
| `RecursionLimit` | Nesting too deep (>1000 levels) |

## Mutation

### Modifying Objects

```tml
var json = parse("{\"count\": 0}")!

// Set or update a key
json.set("count", 42)
json.set("new_key", "new_value")

// Remove a key
json.remove("old_key")
```

### Modifying Arrays

```tml
var json = parse("[1, 2, 3]")!

// Push to array
json.push(4)
json.push(5)

// Result: [1, 2, 3, 4, 5]
```

### Merge and Extend

```tml
// Merge objects
var base = parse("{\"a\": 1, \"b\": 2}")!
let overlay = parse("{\"b\": 3, \"c\": 4}")!
base.merge(overlay)
// Result: {"a": 1, "b": 3, "c": 4}

// Extend arrays
var arr1 = parse("[1, 2]")!
let arr2 = parse("[3, 4]")!
arr1.extend(arr2)
// Result: [1, 2, 3, 4]
```

## Streaming

For large JSON files or network streams:

```tml
use std::json::stream::{JsonReader, JsonWriter}
use std::fs::File

// Read JSON values from file (NDJSON format)
let file = File.open("data.jsonl")!
let reader = JsonReader.new(file)

loop json in reader.read_all() {
    when json {
        Ok(value) -> process(value),
        Err(e) -> println("Error: {e}"),
    }
}

// Write JSON values to file
let out = File.create("output.jsonl")!
var writer = JsonWriter.new(out)

loop item in items {
    writer.write(ref item.serialize())!
}
writer.flush()!
```

## Best Practices

### 1. Use Type-Safe Access

```tml
// Prefer
let name = json["name"].as_str().unwrap_or("unknown")

// Over
let name = json["name"]  // May panic on wrong type
```

### 2. Validate Early

```tml
func process_config(json: ref Json) -> Outcome[Config, Error] {
    // Validate required fields first
    let host = json["host"].as_str()
        .ok_or(Error.new("missing host"))?
    let port = json["port"].as_i64()
        .ok_or(Error.new("missing port"))?

    Ok(Config { host: host.into(), port: port as I32 })
}
```

### 3. Use Derive for Data Types

```tml
// Let the compiler generate boilerplate
@derive(Serialize, Deserialize)
type Message {
    id: U64,
    content: String,
    timestamp: I64,
}
```

### 4. Handle Large Numbers

```tml
// JSON integers beyond I64 range are stored as Uint64
let big = parse("18446744073709551615")!
if big.is_integer() {
    // Use as_u64() for large unsigned values
    let value = big.as_u64().unwrap()
}
```

---

*Previous: [ch13-00-modules.md](ch13-00-modules.md)*
*Next: [appendix-00.md](appendix-00.md)*
