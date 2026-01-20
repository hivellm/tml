# Tasks: Native JSON Implementation for MCP Support

**Status**: C++ Complete (71%) - V8-optimized fast parser implemented, TML stdlib layer not started

**Priority**: High - Required for MCP integration

## Phase 1: JSON Value Types

### 1.1 Core Data Structures
- [x] 1.1.1 Create `JsonNumber` discriminated union (Int64/Uint64/Double)
- [x] 1.1.2 Create `JsonValue` variant type
- [x] 1.1.3 Create `JsonArray` type
- [x] 1.1.4 Create `JsonObject` type
- [x] 1.1.5 Use move semantics and `std::unique_ptr`
- [x] 1.1.6 Add type query methods (`is_integer()`, `is_float()`, etc.)

### 1.2 Value Accessors
- [x] 1.2.1 Implement `as_i64()`, `as_u64()`, `as_i32()`, `as_u32()` accessors
- [x] 1.2.2 Implement `as_f64()` accessor (always works for numbers)
- [x] 1.2.3 Implement `try_as_i64()`, `try_as_u64()` safe accessors
- [x] 1.2.4 Implement `as_bool()`, `as_string()` accessors
- [x] 1.2.5 Implement `as_array()` accessor
- [x] 1.2.6 Implement `as_object()` accessor
- [x] 1.2.7 Add `get(key)` for objects
- [x] 1.2.8 Add `operator[]` for arrays

### 1.3 Value Mutation
- [x] 1.3.1 Implement `set(key, value)` for objects
- [x] 1.3.2 Implement `push(value)` for arrays
- [x] 1.3.3 Implement `remove(key)` for objects
- [x] 1.3.4 Implement `clear()` for arrays and objects

## Phase 2: JSON Parser

### 2.1 Lexer
- [x] 2.1.1 Create `JsonToken` enum
- [x] 2.1.2 Implement `JsonLexer` class
- [x] 2.1.3 Parse string tokens with escapes
- [x] 2.1.4 Parse number tokens (detect integer vs float)
- [x] 2.1.5 Skip whitespace
- [x] 2.1.6 Detect number type (no decimal/exp = integer)

### 2.2 Parser
- [x] 2.2.1 Create `JsonParser` class
- [x] 2.2.2 Implement recursive descent parser
- [x] 2.2.3 Parse objects
- [x] 2.2.4 Parse arrays
- [x] 2.2.5 Parse primitives
- [x] 2.2.6 Handle nested structures
- [x] 2.2.7 Return `Result<JsonValue, JsonError>`

### 2.3 Error Handling
- [x] 2.3.1 Create `JsonError` struct
- [x] 2.3.2 Report unexpected token errors
- [x] 2.3.3 Report unterminated string errors
- [x] 2.3.4 Report invalid number errors
- [x] 2.3.5 Report nesting depth limit errors

## Phase 3: JSON Serializer

### 3.1 Basic Serialization
- [x] 3.1.1 Implement `to_string()`
- [x] 3.1.2 Serialize primitives
- [x] 3.1.3 Serialize numbers (preserve integer format)
- [x] 3.1.4 Serialize arrays
- [x] 3.1.5 Serialize objects
- [x] 3.1.6 Escape special characters

### 3.2 Pretty Printing
- [x] 3.2.1 Add `to_string_pretty()` with indentation
- [x] 3.2.2 Add newlines after elements
- [x] 3.2.3 Add configurable indent string

### 3.3 Streaming Output
- [x] 3.3.1 Implement `write_to(ostream)`
- [x] 3.3.2 Avoid intermediate allocations
- [ ] 3.3.3 Add buffer size hints

## Phase 4: JSON Builder API

### 4.1 Fluent Builder
- [x] 4.1.1 Create `JsonBuilder` class
- [x] 4.1.2 Implement `object()` method
- [x] 4.1.3 Implement `array()` method
- [x] 4.1.4 Implement `field(key, value)` method
- [x] 4.1.5 Implement `item(value)` method
- [x] 4.1.6 Implement `end()` method
- [x] 4.1.7 Implement `build()` method

### 4.2 Convenience Methods
- [x] 4.2.1 Add typed field methods
- [x] 4.2.2 Add typed item methods
- [x] 4.2.3 Add nested object field
- [x] 4.2.4 Add nested array field

### 4.3 From Text
- [x] 4.3.1 Add `parse()` static method
- [x] 4.3.2 Add `merge()` method
- [x] 4.3.3 Add `extend()` method

## Phase 5: Performance Optimization (V8-inspired)

### 5.1 Fast Parser Core (COMPLETE)
- [x] 5.1.1 Create `FastJsonParser` class with V8-style optimizations
- [x] 5.1.2 O(1) character lookup tables (256-entry)
- [x] 5.1.3 Single-pass parsing (merged lexer+parser)
- [x] 5.1.4 Pre-allocated string buffers

### 5.2 SIMD Parsing (COMPLETE)
- [x] 5.2.1 Use SIMD (SSE2) for whitespace skipping
- [x] 5.2.2 Use SIMD for string scanning (quotes/escapes)
- [x] 5.2.3 SWAR hex digit parsing for \uXXXX escapes
- [ ] 5.2.4 Benchmark against simdjson

### 5.3 Number Parsing
- [x] 5.3.1 SMI fast path for small integers
- [x] 5.3.2 Avoid string allocation for number parsing
- [ ] 5.3.3 SIMD number parsing (future)

### 5.4 Memory Pool (Future)
- [ ] 5.4.1 Create `JsonAllocator` arena
- [ ] 5.4.2 Pool small string allocations
- [ ] 5.4.3 Reduce `unique_ptr` overhead
- [ ] 5.4.4 Add `JsonDocument` wrapper

### 5.5 String Interning (Future)
- [ ] 5.5.1 Intern common JSON keys
- [ ] 5.5.2 Use `string_view` when possible
- [ ] 5.5.3 Add copy-on-write for strings

## Phase 6: MCP Integration Prep

### 6.1 JSON-RPC Support
- [x] 6.1.1 Create `JsonRpcRequest` struct
- [x] 6.1.2 Create `JsonRpcResponse` struct
- [x] 6.1.3 Create `JsonRpcError` struct
- [x] 6.1.4 Implement request parsing helpers
- [x] 6.1.5 Implement response building helpers

### 6.2 Schema Validation
- [x] 6.2.1 Basic type validation
- [x] 6.2.2 Required field validation
- [x] 6.2.3 Array element type validation

## Phase 7: Testing

### 7.1 Unit Tests
- [x] 7.1.1 Test parsing primitives
- [x] 7.1.2 Test parsing nested objects
- [x] 7.1.3 Test parsing nested arrays
- [x] 7.1.4 Test parsing mixed structures
- [x] 7.1.5 Test string escape sequences
- [x] 7.1.6 Test number formats
- [x] 7.1.7 Test error handling

### 7.2 Roundtrip Tests
- [x] 7.2.1 Parse -> Serialize -> Parse roundtrip
- [x] 7.2.2 Builder -> Serialize -> Parse roundtrip

### 7.3 Performance Tests
- [x] 7.3.1 Benchmark parsing 1MB JSON
- [x] 7.3.2 Benchmark serialization 1MB JSON
- [x] 7.3.3 Compare with other implementations (Python, Node.js, Rust serde_json)

## Phase 8: TML Standard Library Layer

### 8.1 Core Types
- [ ] 8.1.1 Create `JsonNumber` enum (Int/Uint/Float)
- [ ] 8.1.2 Create `Json` enum type with `Number(JsonNumber)`
- [ ] 8.1.3 Create `JsonArray` type alias
- [ ] 8.1.4 Create `JsonObject` type alias
- [ ] 8.1.5 Add convenience constructors (`Json::int()`, `Json::float()`)
- [ ] 8.1.6 Implement `Json::parse()`
- [ ] 8.1.7 Implement `Json::to_string()`

### 8.2 Pattern Matching Support
- [ ] 8.2.1 Enable pattern matching on Json
- [ ] 8.2.2 Add type check methods
- [ ] 8.2.3 Add safe accessor methods
- [ ] 8.2.4 Add unwrap methods with panic

### 8.3 Object Navigation
- [ ] 8.3.1 Implement `get(key)` method
- [ ] 8.3.2 Implement `get_path()` method
- [ ] 8.3.3 Implement `operator[]` for keys
- [ ] 8.3.4 Implement `operator[]` for indices
- [ ] 8.3.5 Add `keys()` and `values()` methods

### 8.4 Fluent Builder in TML
- [ ] 8.4.1 Create `JsonBuilder` class
- [ ] 8.4.2 Implement nesting methods
- [ ] 8.4.3 Implement field methods
- [ ] 8.4.4 Implement item methods
- [ ] 8.4.5 Add `build()` method

### 8.5 Serialization Behavior
- [ ] 8.5.1 Create `ToJson` behavior
- [ ] 8.5.2 Create `FromJson` behavior
- [ ] 8.5.3 Implement for primitives
- [ ] 8.5.4 Implement for `List[T]`
- [ ] 8.5.5 Implement for `Map[Str, T]`
- [ ] 8.5.6 Add `@derive` macro support

### 8.6 Pretty Printing
- [ ] 8.6.1 Implement `to_string_pretty()`
- [ ] 8.6.2 Implement `Display` behavior
- [ ] 8.6.3 Add colored output option

### 8.7 FFI Bridge
- [ ] 8.7.1 Create `@extern` bindings for parser
- [ ] 8.7.2 Create `@extern` bindings for builder
- [ ] 8.7.3 Handle memory ownership
- [ ] 8.7.4 Zero-copy string passing

## Summary

| Phase | Description | Status | Progress |
|-------|-------------|--------|----------|
| 1 | JSON Value Types | **Complete** | 18/18 |
| 2 | JSON Parser | **Complete** | 18/18 |
| 3 | JSON Serializer | **Complete** | 11/12 |
| 4 | JSON Builder | **Complete** | 14/14 |
| 5 | Performance (V8-inspired) | **In Progress** | 10/17 |
| 6 | MCP Integration | **Complete** | 8/8 |
| 7 | Testing | **Complete** | 12/12 |
| 8 | TML stdlib | Not Started | 0/29 |
| **Total** | | **In Progress** | **91/128** |

## Implemented Files

### Headers (`compiler/include/json/`)
- `json.hpp` - Main public header
- `json_error.hpp` - Error struct with line/column
- `json_value.hpp` - JsonNumber, JsonValue, JsonArray, JsonObject
- `json_parser.hpp` - JsonLexer, JsonParser (original implementation)
- `json_fast_parser.hpp` - FastJsonParser with V8 optimizations (SSE2, SWAR, lookup tables)
- `json_builder.hpp` - Fluent builder API
- `json_rpc.hpp` - JSON-RPC 2.0 structs
- `json_schema.hpp` - Schema validation

### Sources (`compiler/src/json/`)
- `json_value.cpp` - Value type implementations
- `json_parser.cpp` - Original Lexer + Parser
- `json_fast_parser.cpp` - V8-optimized parser (SIMD, lookup tables, SWAR)
- `json_serializer.cpp` - to_string(), to_string_pretty(), write_to()
- `json_builder.cpp` - Builder implementation
- `json_rpc.cpp` - JSON-RPC helpers
- `json_schema.cpp` - Schema validation

### Tests (`compiler/tests/`)
- `json_test.cpp` - 92 comprehensive tests (all passing)

### Benchmarks (`benchmarks/`)
- `cpp/json_bench.cpp` - TML C++ native JSON benchmark
- `python/json_bench.py` - Python json module benchmark
- `node/json_bench.js` - Node.js JSON benchmark
- `rust/json_bench/` - Rust serde_json benchmark
- `results/json_benchmark_comparison.md` - Full comparison results

## Validation

- [ ] V.1 Parse 1MB JSON in < 10ms
- [ ] V.2 Serialize 1MB JSON in < 5ms
- [x] V.3 Zero-copy parsing where possible (uses string_view)
- [x] V.4 No memory leaks (move semantics, RAII)
- [x] V.5 Thread-safe parsing (stateless parser)
- [x] V.6 Full RFC 8259 compliance

## Dependencies

### External
- None (self-contained implementation)

### Internal
- `core::result` - Outcome type for error handling
- `core::option` - Maybe type
- `core::string` - String operations
- `std::collections` - Map type for JsonObject
