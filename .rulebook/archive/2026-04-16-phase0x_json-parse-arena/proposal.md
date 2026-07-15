# Proposal: phase0x_json-parse-arena

## Why

Benchmark analysis (2026-04-14) revealed that TML's `std::json` module is **10–14x slower
than a hand-rolled unoptimized Rust parser** (no serde_json, stdlib only):

| Operation            | TML (parse_fast) | Rust (hand-rolled) | Ratio  |
|----------------------|------------------|--------------------|--------|
| Parse Tiny (27 bytes)  | 2640 ns/op       | 193 ns/op          | 13.7x  |
| Parse Small (200 bytes)| 11412 ns/op      | 972 ns/op          | 11.7x  |
| Parse Medium (500 bytes)| 27573 ns/op     | 2631 ns/op         | 10.5x  |
| Field Access           | 11560 ns/op      | 966 ns/op          | 12.0x  |
| Array Iteration        | 12973 ns/op      | 992 ns/op          | 13.1x  |

Root cause identified: **every `json::parse_fast` + `json::free` call triggers a
malloc+free cycle for the entire JSON node tree**. simdjson itself is fast (~1 ns/byte),
but TML measures ~98 ns/byte for 27 bytes — physically impossible for simdjson alone.
The overhead is the allocator.

Evidence: `parse_fast` (simdjson) = 11412 ns vs `parse_standard` (non-SIMD) = 14660 ns
for the same 200-byte document. The simdjson speedup is only ~3250 ns. The remaining
~11000 ns is malloc+free overhead present in both paths.

## What Changes

Add a **thread-local bump/arena allocator** to the JSON C runtime. A `JsonArena` is a
pre-allocated memory block; all JSON node allocations are bump-allocated into it.
`reset()` rewinds the pointer to the start in O(1) — no per-node `free`. The existing
`json::parse_fast` / `json::free` API is kept unchanged (additive change).

### C runtime additions (`lib/std/runtime/json/`)

```c
// New: json_arena.c
void*  tml_json_arena_create(size_t initial_capacity);
void   tml_json_arena_reset(void* arena);
void   tml_json_arena_destroy(void* arena);
int64_t tml_json_parse_arena(const char* src, size_t len, void* arena);
```

### TML binding (`lib/std/src/json/arena.tml`)

```tml
type JsonArena { handle: I64 }

impl JsonArena {
    pub func new(capacity: I64) -> JsonArena
    pub func parse(self, json: Str) -> I64   // returns JsonHandle
    pub func reset(self)                      // O(1) — no free needed
    pub func drop(self)
}
```

### Before vs After pattern

```tml
// BEFORE — malloc/free per call (~11000 ns overhead)
let h: I64 = json::parse_fast(json_str)
let age: I64 = json::object_get_i64(h, "age")
json::free(h)

// AFTER — arena reset (O(1), cache-friendly)
let arena: JsonArena = JsonArena::new(256 * 1024)
let h: I64 = arena.parse(json_str)
let age: I64 = json::object_get_i64(h, "age")
arena.reset()
```

## Impact

- Affected code: `lib/std/runtime/json/` (new C file), `lib/std/src/json/` (new TML module)
- Breaking change: NO — purely additive
- User benefit: 5–10x speedup for JSON-heavy workloads (REST handlers, config loading,
  parsers) with no changes to existing code. Target: <2x vs Rust for all operations.
