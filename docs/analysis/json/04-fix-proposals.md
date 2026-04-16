# 04 — Fix Proposals

## Priority Matrix

```
                    Low Effort ◄──────────► High Effort
                    │                              │
High Impact   ┌─────┤ F-001 (flat object)          │
              │     │                  F-003 (arena)│
              │     │                  F-004 (refs) │
              ├─────┤                              │
Low Impact    │     │ F-002 (unbox)    F-005 (table)│
              └─────┤                              │
```

---

## F-001: Replace `std::map` with flat vector (P0)

**File**: `compiler/include/json/json_value.hpp:80`

**Current**:
```cpp
using JsonObject = std::map<std::string, JsonValue>;
```

**Proposed**:
```cpp
using JsonObject = std::vector<std::pair<std::string, JsonValue>>;
```

**Changes needed**:
1. Change the typedef (1 line)
2. Update `json_value.hpp` accessor methods that use `map::find()` → linear scan
3. Update `json_runtime.cpp` FFI functions that iterate objects
4. Update `json_fast_parser.cpp` `parse_object()` — replace `emplace()` with `push_back()`
5. Add `reserve()` call in `parse_object()` (estimate field count from input size)

**Impact**:
- Eliminates 9 RB-tree node allocations per parse (~4,500 ns saved)
- Reduces memory usage by ~720 bytes per parse
- Object access becomes O(n) linear scan but faster due to cache locality (for <20 fields)
- `reserve()` pre-allocates all entries in one shot

**Risk**: Low. JSON objects rarely have >20 fields. Linear scan is faster than tree traversal for small n.

**Expected**: 11,175 ns → **~6,500 ns** (~1.7x improvement).

---

## F-002: Remove Box<> wrapping (P2)

**File**: `compiler/include/json/json_value.hpp:392-393`

**Current**:
```cpp
Box<JsonArray>,   // unique_ptr<vector<JsonValue>>
Box<JsonObject>   // unique_ptr<map<string, JsonValue>>
```

**Proposed**:
```cpp
JsonArray,    // vector<JsonValue> stored inline in variant
JsonObject    // vector<pair<string, JsonValue>> stored inline
```

**Impact**: Eliminates 3 `unique_ptr::new` allocations per parse (~600 ns saved). Increases `sizeof(JsonValue)` from ~40 to ~72 bytes (inline vector).

**Risk**: Medium. Larger variant affects all value copies and moves. May hurt cache performance for deeply nested JSON.

**Expected**: ~600 ns saved.

---

## F-003: Wire arena into fast parser (P1)

**Files**:
- `compiler/include/json/json_fast_parser.hpp` — add `JsonArena*` member
- `compiler/src/json/json_fast_parser.cpp` — use arena for string/object/array allocation
- `compiler/src/json/json_runtime.cpp` — create arena in `tml_json_parse_fast`

**Current**: `parse_json_fast()` uses default `std::string`/`std::vector`/`std::map` allocators.

**Proposed**:
1. Add `JsonArena* arena_` to `FastJsonParser`
2. `parse_string()`: use `arena_->alloc_string(sv)` → returns `std::string_view` into arena
3. `parse_object()`: use `pmr::vector` with arena as backing allocator
4. `parse_array()`: use `pmr::vector` with arena as backing allocator
5. `tml_json_parse_fast()`: create thread-local arena, parse, keep alive until `tml_json_free()`

**Impact**: ALL per-parse allocations go through bump allocator (~2 ns each instead of ~200 ns). Reset is O(1).

**Risk**: Medium. Requires `std::string` → `std::string_view` migration for string keys (lifetime management). Requires `pmr` allocator support in MSVC/Clang.

**Expected**: 11,175 ns → **~2,000-3,000 ns** (4-6x improvement).

---

## F-004: Return references instead of clones (P1)

**File**: `compiler/src/json/json_runtime.cpp:468-469`

**Current**:
```cpp
return alloc_json_handle(field->clone());  // DEEP COPY
```

**Proposed**: 
```cpp
// Return a "sub-handle" that references the existing field in-place
// Encode as: parent_handle | (field_index << 32) or similar scheme
```

Or simpler:
```cpp
// Store a pointer to the original field, mark as "borrowed" (don't free)
static int64_t alloc_borrowed_handle(const JsonValue* field) {
    // ... store pointer, mark as non-owning ...
}
```

**Impact**: Every `object_get`, `array_get` becomes O(1) with zero allocation (currently 2 allocs per access).

**Risk**: Medium. Need to ensure the parent document outlives child handles. Add reference counting or document-scoped handles.

**Expected**: Field Access 15,320 ns → **~1,000 ns** (~15x improvement).

---

## F-005: Fix string_buffer reuse defeat (P1)

**File**: `compiler/src/json/json_fast_parser.cpp:657`

**Current**:
```cpp
return std::move(string_buffer_);  // moves buffer out, next call allocates fresh
```

**Proposed**:
```cpp
// Don't move — copy the result and keep the buffer for reuse
return std::string(string_buffer_);  // copy, keep buffer allocated
```

Or better: use `std::string_view` into the input buffer for unescaped strings:
```cpp
// Fast path: no escape sequences → zero-copy view into input
if (!has_escapes) {
    return std::string_view(start, length);  // ZERO ALLOCATION
}
// Slow path: escapes → use string_buffer
return std::string(string_buffer_);
```

**Impact**: 13 strings → 0-4 allocations (only escaped strings need allocation).

**Expected**: ~2,600 ns saved for parse.

---

## Implementation Order

| Phase | Fix | Effort | Cumulative Target |
|-------|-----|--------|-------------------|
| 1 | F-001 (flat object) | 2-4h | 11,175 → ~6,500 ns |
| 2 | F-005 (string reuse) | 2-3h | ~6,500 → ~4,000 ns |
| 3 | F-004 (refs not clones) | 4-6h | Field Access 15,320 → ~1,000 ns |
| 4 | F-003 (arena integration) | 1-2d | ~4,000 → ~1,500 ns |
| 5 | F-002 (unbox) | 1-2h | ~1,500 → ~1,200 ns |

**Final target**: **<1,500 ns/op** for Parse Small. Ratio vs serde_json: **<2x** (from 13.6x).
