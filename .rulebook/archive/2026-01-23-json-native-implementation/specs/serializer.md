# Specification: JSON Serializer

## 1. Interface

```cpp
// Compact output
auto json_to_string(const JsonValue& value) -> std::string;

// Pretty printed
auto json_to_string_pretty(const JsonValue& value, int indent = 2) -> std::string;

// Streaming output
void json_write(std::ostream& out, const JsonValue& value);
void json_write_pretty(std::ostream& out, const JsonValue& value, int indent = 2);
```

## 2. String Escaping

Characters that must be escaped in output:
- `"` -> `\"`
- `\` -> `\\`
- Control characters (0x00-0x1F) -> `\uXXXX` or named escapes

## 3. Number Serialization

Integer types are serialized without decimal point:
- `Int64` -> `"123"`, `"-456"`
- `Uint64` -> `"18446744073709551615"`
- `Double` -> `"1.5"`, `"1e10"`, `"-0.0"`

For doubles, use shortest representation that round-trips.
