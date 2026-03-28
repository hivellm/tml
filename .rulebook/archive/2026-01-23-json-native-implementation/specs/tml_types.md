# Specification: TML Standard Library Types

## 1. JsonNumber Type

```tml
/// Optimized number representation preserving integer vs float distinction
type JsonNumber {
    Int(I64),       // Integer value (most common)
    Uint(U64),      // Unsigned integer (for large positive values)
    Float(F64),     // Floating point
}

impl JsonNumber {
    /// Check if this is an integer (Int or Uint)
    func is_integer(this) -> Bool {
        when this {
            Int(_) | Uint(_) => true
            Float(_) => false
        }
    }

    /// Check if this is a float
    func is_float(this) -> Bool {
        when this { Float(_) => true, _ => false }
    }

    /// Get as I64, returns Nothing if it doesn't fit
    func as_i64(this) -> Maybe[I64] {
        when this {
            Int(v) => Just(v)
            Uint(v) if v <= I64::MAX as U64 => Just(v as I64)
            _ => Nothing
        }
    }

    /// Get as U64, returns Nothing if negative
    func as_u64(this) -> Maybe[U64] {
        when this {
            Int(v) if v >= 0 => Just(v as U64)
            Uint(v) => Just(v)
            _ => Nothing
        }
    }

    /// Always get as F64 (may lose precision for large integers)
    func as_f64(this) -> F64 {
        when this {
            Int(v) => v as F64
            Uint(v) => v as F64
            Float(v) => v
        }
    }
}
```

## 2. Json Enum

```tml
type Json {
    Null,
    Bool(Bool),
    Number(JsonNumber),   // Optimized number type
    String(Str),
    Array(List[Json]),
    Object(Map[Str, Json]),
}

impl Json {
    // Convenience constructors for numbers
    static func int(v: I64) -> Json { Json::Number(JsonNumber::Int(v)) }
    static func uint(v: U64) -> Json { Json::Number(JsonNumber::Uint(v)) }
    static func float(v: F64) -> Json { Json::Number(JsonNumber::Float(v)) }

    // Type queries
    func is_integer(this) -> Bool {
        when this { Number(n) => n.is_integer(), _ => false }
    }

    func is_float(this) -> Bool {
        when this { Number(n) => n.is_float(), _ => false }
    }

    // Safe accessors
    func as_i64(this) -> Maybe[I64] {
        when this { Number(n) => n.as_i64(), _ => Nothing }
    }

    func as_u64(this) -> Maybe[U64] {
        when this { Number(n) => n.as_u64(), _ => Nothing }
    }

    func as_f64(this) -> Maybe[F64] {
        when this { Number(n) => Just(n.as_f64()), _ => Nothing }
    }
}
```

## 3. ToJson Behavior

```tml
behavior ToJson {
    func to_json(this) -> Json
}

// Built-in implementations preserving integer types
impl ToJson for Bool {
    func to_json(this) -> Json { Json::Bool(this) }
}

impl ToJson for I8 {
    func to_json(this) -> Json { Json::int(this as I64) }
}

impl ToJson for I16 {
    func to_json(this) -> Json { Json::int(this as I64) }
}

impl ToJson for I32 {
    func to_json(this) -> Json { Json::int(this as I64) }
}

impl ToJson for I64 {
    func to_json(this) -> Json { Json::int(this) }
}

impl ToJson for U8 {
    func to_json(this) -> Json { Json::int(this as I64) }
}

impl ToJson for U16 {
    func to_json(this) -> Json { Json::int(this as I64) }
}

impl ToJson for U32 {
    func to_json(this) -> Json { Json::int(this as I64) }
}

impl ToJson for U64 {
    func to_json(this) -> Json {
        if this <= I64::MAX as U64 {
            Json::int(this as I64)
        } else {
            Json::uint(this)
        }
    }
}

impl ToJson for F32 {
    func to_json(this) -> Json { Json::float(this as F64) }
}

impl ToJson for F64 {
    func to_json(this) -> Json { Json::float(this) }
}

impl ToJson for Str {
    func to_json(this) -> Json { Json::String(this) }
}

impl[T: ToJson] ToJson for List[T] {
    func to_json(this) -> Json {
        Json::Array(this.iter().map(do(x) x.to_json()).collect())
    }
}

impl[T: ToJson] ToJson for Map[Str, T] {
    func to_json(this) -> Json {
        Json::Object(this.iter().map(do((k, v)) (k, v.to_json())).collect())
    }
}
```

## 4. FromJson Behavior

```tml
behavior FromJson {
    static func from_json(json: ref Json) -> Outcome[This, JsonError]
}

impl FromJson for Bool {
    static func from_json(json: ref Json) -> Outcome[Bool, JsonError] {
        when json {
            Json::Bool(b) => Ok(b)
            _ => Err(JsonError::type_mismatch("Bool", json.type_name()))
        }
    }
}

impl FromJson for I64 {
    static func from_json(json: ref Json) -> Outcome[I64, JsonError] {
        when json.as_i64() {
            Just(v) => Ok(v)
            Nothing => Err(JsonError::type_mismatch("I64", json.type_name()))
        }
    }
}

impl FromJson for U64 {
    static func from_json(json: ref Json) -> Outcome[U64, JsonError] {
        when json.as_u64() {
            Just(v) => Ok(v)
            Nothing => Err(JsonError::type_mismatch("U64", json.type_name()))
        }
    }
}

impl FromJson for F64 {
    static func from_json(json: ref Json) -> Outcome[F64, JsonError] {
        when json.as_f64() {
            Just(v) => Ok(v)
            Nothing => Err(JsonError::type_mismatch("F64", json.type_name()))
        }
    }
}

impl FromJson for Str {
    static func from_json(json: ref Json) -> Outcome[Str, JsonError] {
        when json {
            Json::String(s) => Ok(s.duplicate())
            _ => Err(JsonError::type_mismatch("Str", json.type_name()))
        }
    }
}
```

## 5. JsonBuilder in TML

```tml
class JsonBuilder {
    private stack: List[Json]
    private keys: List[Maybe[Str]]

    func new() -> JsonBuilder {
        JsonBuilder { stack: [], keys: [] }
    }

    func object(mut this) -> mut ref JsonBuilder {
        this.stack.push(Json::Object(Map::new()))
        this.keys.push(Nothing)
        this
    }

    func array(mut this) -> mut ref JsonBuilder {
        this.stack.push(Json::Array([]))
        this.keys.push(Nothing)
        this
    }

    func field(mut this, key: Str, value: Json) -> mut ref JsonBuilder {
        when this.stack.last_mut() {
            Just(Json::Object(obj)) => obj.insert(key, value)
            _ => panic("field() called outside object context")
        }
        this
    }

    func item(mut this, value: Json) -> mut ref JsonBuilder {
        when this.stack.last_mut() {
            Just(Json::Array(arr)) => arr.push(value)
            _ => panic("item() called outside array context")
        }
        this
    }

    func end(mut this) -> mut ref JsonBuilder {
        let value = this.stack.pop().unwrap()
        if this.stack.is_empty() {
            this.stack.push(value)
        } else {
            when this.keys.pop() {
                Just(Just(key)) => this.field(key, value)
                Just(Nothing) => this.item(value)
                _ => {}
            }
        }
        this
    }

    func build(mut this) -> Json {
        this.stack.pop().unwrap_or(Json::Null)
    }
}
```
