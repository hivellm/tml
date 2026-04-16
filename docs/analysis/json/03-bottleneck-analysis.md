# 03 — Bottleneck Analysis

## Methodology

Measured: `tml_json_parse_fast` on 200-byte JSON = **11,175 ns/op** (100K iterations, release mode).
Rust serde_json on equivalent input = **820 ns/op**.
Gap = **13.6x**.

Breakdown estimated by allocation count × typical cost:

| Component | Est. Cost (ns) | % of Total | Evidence |
|-----------|----------------|------------|----------|
| std::map insertions (9 nodes) | 4,500 | 40% | 9 × ~500 ns (alloc + RB-rebalance) |
| String allocations (13 strings) | 2,600 | 23% | 13 × ~200 ns (malloc + memcpy) |
| Box<> allocations (3 wrappers) | 600 | 5% | 3 × ~200 ns (new + move) |
| Parser overhead (SIMD scan, control flow) | 1,500 | 13% | Measured ~1.5 us for tiny 27B input |
| Handle table (push_back, linear scan) | 200 | 2% | 2 allocs + O(n) scan |
| `std::variant` construction overhead | 500 | 5% | 13 × ~40 ns (variant ctor + move) |
| Misc (strlen, null checks, error paths) | 1,275 | 12% | Residual |
| **TOTAL** | **~11,175** | **100%** | |

## Bottleneck #1: `std::map<std::string, JsonValue>` (40% of cost)

**File**: `compiler/include/json/json_value.hpp:80`
```cpp
using JsonObject = std::map<std::string, JsonValue>;
```

**Why it's slow**:
- `std::map` is a red-black tree: each `emplace()` does:
  1. `new` a tree node (~48 bytes header + alignment)
  2. Move-construct key (std::string) and value (JsonValue) into the node
  3. Tree walk to find insertion point (O(log n) comparisons)
  4. Rebalance (pointer rotations)
- For 9 fields: 9 allocations + 9 tree balances
- Cache-hostile: nodes scattered across heap, pointer chasing on access

**Comparison with serde_json**:
- serde_json default: `Vec<(String, Value)>` — one contiguous allocation, linear scan for access
- For <20 fields, linear scan beats RB-tree due to cache locality
- serde_json with `preserve_order` feature: `IndexMap` — hash table + insertion-order vector

## Bottleneck #2: String Allocation Pattern (23% of cost)

**File**: `compiler/src/json/json_fast_parser.cpp:631-658`
```cpp
auto FastJsonParser::parse_string() -> Result<std::string, JsonError> {
    string_buffer_.clear();
    // ... append chars ...
    return std::move(string_buffer_);  // ← moves internal buffer OUT
}
```

`string_buffer_` is a member variable meant for reuse. But `std::move` transfers ownership of the internal buffer to the caller. On the next `parse_string()` call, `string_buffer_` is empty (moved-from state), and `.clear()` is a no-op on an empty string. The first `.append()` allocates a new buffer.

**Effect**: The "reuse" optimization is defeated. Each string parse allocates fresh.

**Why this matters**: 13 strings × ~200 ns each = **~2,600 ns** (23% of total).

**Fix**: Use `std::string_view` into the input buffer for unescaped strings (zero-copy). Only allocate for strings with escape sequences.

## Bottleneck #3: Arena Allocator Is Dead Code (0% impact — it's unused)

**File**: `compiler/src/json/json_allocator.cpp:25-36`
```cpp
auto JsonDocument::parse(std::string_view input, size_t arena_size) {
    JsonDocument doc(arena_size);
    auto result = fast::parse_json_fast(input);  // ← IGNORES the arena
    doc.set_root(std::move(unwrap(result)));
    return doc;
}
```

The `JsonArena` class (`json_allocator.hpp:194-306`) has:
- Bump allocation with 64KB blocks
- String interning with FNV-1a hash
- Pre-interned common JSON keys
- O(1) reset

But `parse_json_fast()` uses `std::string`, `std::map`, and `std::vector` which ALL use the default allocator. The arena is allocated, pre-interns keys, then sits idle while the parser mallocs independently.

**Fix**: Thread `JsonArena*` through `FastJsonParser`, use `arena->alloc_string()` for keys, `pmr::polymorphic_allocator` for map/vector.

## Bottleneck #4: Deep Clone on Access (n/a for parse, 2x for reads)

**File**: `compiler/src/json/json_runtime.cpp:468-469`
```cpp
// tml_json_object_get:
return alloc_json_handle(field->clone());
```

Every `json::object_get(h, "name")` from TML:
1. Traverses the RB-tree to find the key — O(log n) string comparisons
2. Calls `field->clone()` which deep-copies the entire subtree
3. Allocates a new handle in the global table

serde_json returns `&Value` (a reference). Zero allocations.

**Fix**: Return a sub-handle (index offset into the existing document) instead of cloning. The document stays alive until `json::free()` — child references are valid for its lifetime.

## Bottleneck #5: Box<> Wrapping in Variant (5% of cost)

**File**: `compiler/include/json/json_value.hpp:392-393`
```cpp
Box<JsonArray>,   // unique_ptr<vector<JsonValue>>
Box<JsonObject>   // unique_ptr<map<string, JsonValue>>
```

Each array or object in the JSON gets an extra `new` allocation to box it into the variant. This is because `JsonArray` and `JsonObject` are large types that would make the variant too big if stored inline.

**Fix**: Accept the larger variant size (store inline) or use a single arena allocation for the boxed value.
