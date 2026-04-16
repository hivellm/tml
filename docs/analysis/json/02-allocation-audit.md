# 02 — Allocation Audit

## Test Input

200-byte JSON (the "small" benchmark input):
```json
{"name":"John Doe","age":30,"active":true,"email":"john@example.com",
 "scores":[95,87,92,88,91],
 "address":{"street":"123 Main St","city":"New York","zip":"10001"}}
```

Structure: 1 root object (6 fields), 1 nested object (3 fields), 1 array (5 ints), 4 string values.

## Allocation Trace

### Phase 1: Parser Construction

| # | Site | Type | Size | File:Line |
|---|------|------|------|-----------|
| A1 | `FastJsonParser()` | `string_buffer_.reserve(256)` | 256 B | json_fast_parser.hpp:267 |

### Phase 2: Parsing Strings (4 string values + 9 keys = 13 strings)

Each `parse_string()` call:
1. Clears `string_buffer_` (no alloc)
2. Appends chars (may grow if >256 bytes, but our strings are short)
3. Returns `std::move(string_buffer_)` — **moves the internal buffer out**
4. Next `parse_string()` call: `string_buffer_` is now empty, `.clear()` is no-op, first `.append()` does a **NEW allocation**

| # | Site | Type | Size | File:Line |
|---|------|------|------|-----------|
| A2-A14 | `string_buffer_.append()` after move | `std::string` internal alloc | 15-32 B each | json_fast_parser.cpp:643 |

**Total**: 13 string allocations (one per key + one per string value).

### Phase 3: Building Objects (std::map nodes)

Root object: 6 fields → 6 `obj.emplace()` calls.
Each `emplace` allocates a **red-black tree node**.

| # | Site | Type | Size | File:Line |
|---|------|------|------|-----------|
| A15-A20 | `obj.emplace(key, value)` × 6 | RB-tree node | ~80 B each | json_fast_parser.cpp:914 |

Nested "address" object: 3 fields → 3 more nodes.

| # | Site | Type | Size | File:Line |
|---|------|------|------|-----------|
| A21-A23 | `obj.emplace(key, value)` × 3 | RB-tree node | ~80 B each | json_fast_parser.cpp:914 |

**Total**: 9 tree node allocations = **~720 bytes**.

### Phase 4: Building Arrays

`scores` array: `arr.reserve(8)` → 1 allocation.

| # | Site | Type | Size | File:Line |
|---|------|------|------|-----------|
| A24 | `arr.reserve(8)` | `std::vector` buffer | 320 B (8 × 40) | json_fast_parser.cpp:949 |

5 `push_back()` calls fit within the reserved 8 slots — no additional allocs.

### Phase 5: Box<> Wrapping

When a `JsonValue` stores an array or object, it wraps in `Box<>` (= `unique_ptr`), which does a `new` allocation.

| # | Site | Type | Size | File:Line |
|---|------|------|------|-----------|
| A25 | Root object → `Box<JsonObject>` | `new JsonObject` (move) | ~48 B | json_value.hpp:392-393 |
| A26 | Nested object → `Box<JsonObject>` | `new JsonObject` (move) | ~48 B | json_value.hpp:392-393 |
| A27 | Array → `Box<JsonArray>` | `new JsonArray` (move) | ~48 B | json_value.hpp:392-393 |

**Total**: 3 Box allocations = **~144 bytes**.

### Phase 6: FFI Handle Registration

| # | Site | Type | Size | File:Line |
|---|------|------|------|-----------|
| A28 | `json_values.push_back(value)` | vector growth | ~40 B | json_runtime.cpp:107 |
| A29 | `json_values_free.push_back(false)` | vector growth | 1 B | json_runtime.cpp:108 |

## Grand Total

| Category | Count | Bytes |
|----------|-------|-------|
| String allocations (keys + values) | 13 | ~350 B |
| RB-tree nodes (map entries) | 9 | ~720 B |
| Vector buffers (arrays) | 1 | ~320 B |
| Box<> wrappers | 3 | ~144 B |
| Handle table | 2 | ~41 B |
| Parser string_buffer | 1 | ~256 B |
| **TOTAL** | **29** | **~1,831 B** |

## Per-Access Allocations

Each `tml_json_object_get(handle, key)` call (`json_runtime.cpp:450-470`):
1. `std::map::find(key)` — tree traversal, O(log n) string comparisons
2. `field->clone()` — **DEEP COPY** of the entire value (strings, nested objects, arrays)
3. `alloc_json_handle(clone)` — register clone in global table

For accessing "name" (a string field):
- Clone copies the `std::string` → 1 allocation
- alloc_json_handle → 1 allocation
- **Total: 2 allocations per field access**

For the benchmark "Field Access" (parse + access 3 fields):
- Parse: 29 allocations
- Access 3 fields: 6 allocations
- **Total: 35 allocations** for reading 3 fields from a 200-byte JSON.

## Comparison: serde_json

serde_json for the same input:
- `Value::Object` = `Map<String, Value>` backed by `BTreeMap` or `Vec<(String, Value)>`
- Keys stored inline in the map entries (no separate node allocation)
- `serde_json::from_str()` allocates ~5-8 times total (vector for object, strings for values)
- Field access: returns a **reference** (`&Value`), zero allocations

| | TML | serde_json |
|---|-----|------------|
| Allocations per parse | **29** | **~5-8** |
| Allocations per field access | **2** | **0** |
| Object lookup complexity | O(log n) tree | O(n) linear or O(log n) btree |
| String storage | Owned `std::string` per key | Borrowed `&str` when possible |
