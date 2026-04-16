# 01 — JSON Subsystem Architecture

## Data Flow

```
TML code                    C++ FFI                        C++ Parser
─────────                   ───────                        ──────────
json::parse_fast(str) ──►  tml_json_parse_fast(char*)  ──► FastJsonParser::parse()
                           │                               │
                           │  alloc_json_handle(value)     │  parse_value()
                           │  ◄── returns handle (I64)     │  ├─ parse_object() → JsonObject (std::map)
                           │                               │  ├─ parse_array()  → JsonArray (std::vector)
                           │                               │  ├─ parse_string() → std::string
json::object_get(h,k) ──► tml_json_object_get(h, key)     │  └─ parse_number() → JsonNumber
                           │  field->clone()               │
                           │  alloc_json_handle(clone)     │
                           │  ◄── returns handle (I64)     │
                           │                               │
json::free(h)          ──► tml_json_free(h)                │
                           │  json_values[h] = empty       │
```

## Type Definitions

### JsonValue (`compiler/include/json/json_value.hpp:383-396`)

```cpp
struct JsonValue {
    using ValueVariant = std::variant<
        std::monostate,     // null
        bool,               // boolean
        JsonNumber,         // number (int64_t | uint64_t | double)
        std::string,        // string — OWNS the string, heap-allocated for >15 chars
        Box<JsonArray>,     // unique_ptr<vector<JsonValue>> — EXTRA INDIRECTION
        Box<JsonObject>     // unique_ptr<map<string, JsonValue>> — EXTRA INDIRECTION
    >;
    ValueVariant data;
};
```

`sizeof(JsonValue)` = ~40 bytes (variant discriminant + largest member = `std::string` = 32 bytes MSVC).

### JsonObject (`json_value.hpp:80`)

```cpp
using JsonObject = std::map<std::string, JsonValue>;
```

`std::map` = red-black tree. Each entry = tree node (~48 bytes overhead) + key (`std::string`) + value (`JsonValue`).

**Total per entry**: ~48 (node) + 32 (key string) + 40 (value) = **~120 bytes**.

### JsonArray (`json_value.hpp:77`)

```cpp
using JsonArray = std::vector<JsonValue>;
```

Contiguous. Pre-reserved to 8 elements (`parse_array` line 949). Each element = 40 bytes.

### Box (`common.hpp:323`)

```cpp
template <typename T> using Box = std::unique_ptr<T>;
```

Extra `new` allocation for every array/object value in the variant.

## FFI Handle System (`json_runtime.cpp:86-111`)

```cpp
static std::vector<JsonValue> json_values;      // global value store
static std::vector<bool> json_values_free;       // free-slot bitmap
static size_t json_values_next_free = 0;         // linear scan start

static int64_t alloc_json_handle(JsonValue&& value) {
    // Linear scan for free slot from json_values_next_free
    for (size_t i = json_values_next_free; i < json_values_free.size(); ++i) { ... }
    // If none found, push_back (may reallocate entire vector)
    json_values.push_back(std::move(value));
    json_values_free.push_back(false);
}
```

Every `object_get` and `array_get` does `field->clone()` + `alloc_json_handle(clone)` — creating a DEEP COPY of the value.

## Fast Parser (`json_fast_parser.cpp`)

- V8-inspired design with SIMD whitespace skipping
- `string_buffer_` member: pre-allocated `std::string` reused across `parse_string()` calls (good)
- BUT: `parse_string()` returns `std::move(string_buffer_)` which then gets moved into the map key/value, leaving `string_buffer_` empty for next call — so there IS a new allocation per string
- `parse_object()` creates `JsonObject obj` (empty map) then does `obj.emplace()` per field
- `parse_array()` creates `JsonArray arr` with `reserve(8)` then `push_back()` per element

## Arena Allocator (`json_allocator.hpp:194-306`)

A full bump allocator exists with:
- 64KB blocks, alignment-aware allocation
- String interning table with FNV-1a hash
- Pre-interned common keys ("type", "id", "name", etc.)
- `reset()` in O(1)

**Problem**: `parse_json_fast()` never touches it. `JsonDocument::parse()` creates an arena then calls `parse_json_fast()` which uses default allocators. The arena is dead code.
