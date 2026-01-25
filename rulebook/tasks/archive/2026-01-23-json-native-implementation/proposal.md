# Proposal: Native JSON Implementation for MCP Support

## Summary

Implement high-performance JSON serialization/deserialization natively in the TML compiler (C++) with a high-level TML standard library wrapper. This is a prerequisite for MCP (Model Context Protocol) integration.

## Motivation

1. **MCP Integration**: MCP uses JSON-RPC for communication. Native JSON support is essential.
2. **Performance**: External JSON libraries add dependencies and may not be optimized for our use case.
3. **Zero-Copy Parsing**: Using `std::string_view` enables efficient parsing without allocations.
4. **Full Control**: Native implementation allows future optimizations (SIMD, memory pools).
5. **Two-Layer Design**: C++ core for speed, TML wrapper for ergonomics.

## Goals

- Parse JSON in < 10ms/MB
- Serialize JSON in < 5ms/MB
- Full RFC 8259 compliance
- Zero-copy where possible
- Thread-safe (no global state)
- Ergonomic TML API with pattern matching

## Non-Goals

- JSON Schema validation (may add later)
- Streaming parser for very large files (initial version loads full document)
- Binary JSON formats (BSON, MessagePack)

## Design Overview

### C++ Layer (compiler/include/json/)

```cpp
// Core value type
class JsonValue {
    std::variant<
        std::nullptr_t,              // null
        bool,                        // bool
        double,                      // number
        std::string,                 // string
        std::vector<std::unique_ptr<JsonValue>>,  // array
        std::unordered_map<std::string, std::unique_ptr<JsonValue>>  // object
    > data_;
};

// Zero-copy parser
class JsonParser {
    std::string_view input_;
public:
    static auto parse(std::string_view json) -> Result<std::unique_ptr<JsonValue>, JsonError>;
};

// Fluent builder
class JsonBuilder {
public:
    auto object() -> JsonBuilder&;
    auto array() -> JsonBuilder&;
    auto field(std::string_view key, auto value) -> JsonBuilder&;
    auto end() -> JsonBuilder&;
    auto build() -> std::unique_ptr<JsonValue>;
};
```

### TML Layer (lib/std/src/json/)

```tml
// Enum for pattern matching
type Json {
    Null,
    Bool(Bool),
    Number(F64),
    String(Str),
    Array(List[Json]),
    Object(Map[Str, Json]),
}

// Behaviors for custom types
behavior ToJson {
    func to_json(this) -> Json
}

behavior FromJson {
    static func from_json(json: ref Json) -> Outcome[This, JsonError]
}
```

## Alternatives Considered

1. **nlohmann/json**: Popular, but adds external dependency and is not optimized for our needs.
2. **RapidJSON**: Very fast, but complex API and SAX-style parsing.
3. **simdjson**: Extremely fast, but read-only and requires AVX2/NEON.

Native implementation gives us full control and avoids dependency management issues.

## Implementation Plan

1. **Phase 1-3**: C++ core (JsonValue, Parser, Serializer)
2. **Phase 4**: C++ Builder API
3. **Phase 5**: Performance optimizations
4. **Phase 6**: JSON-RPC helpers for MCP
5. **Phase 7**: Unit tests
6. **Phase 8**: TML stdlib wrapper

## Success Criteria

- [ ] All JSON spec test cases pass
- [ ] Performance targets met (< 10ms/MB parse)
- [ ] MCP can send/receive JSON-RPC messages
- [ ] TML code can serialize/deserialize custom classes

## Timeline

Not estimated - implementation order defined in tasks.md.

## References

- [RFC 8259 - JSON](https://tools.ietf.org/html/rfc8259)
- [MCP Specification](https://spec.modelcontextprotocol.io/)
- [JSON-RPC 2.0](https://www.jsonrpc.org/specification)
