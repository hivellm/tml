# 21 — Deep Analysis: JSON Parse & String Operations

Date: 2026-04-16. Based on source code reading + benchmark measurements.

---

## Part I — JSON Parse (13.6x vs serde_json)

### Measured

| Operation | TML (ns/op) | Rust serde_json (ns/op) | Ratio |
|-----------|-------------|------------------------|-------|
| Parse Small (200B) | 11,175 | 820 | **13.6x** |
| Parse Tiny (27B) | 2,558 | — | — |
| Parse Medium (500B) | 29,958 | — | — |
| Field Access | 15,320 | 7,100 | 2.2x |

### Root Cause: `std::map` + per-node allocation

**Finding F-001**: `JsonObject = std::map<std::string, JsonValue>` at [json_value.hpp:80](compiler/include/json/json_value.hpp#L80).

`std::map` is a red-black tree. Each field insertion does:
1. Allocate RB-tree node (~48 bytes header + key + value)
2. Rebalance tree (pointer chasing, cache-hostile)
3. Copy key string into node

For a 6-field object: **6 tree node allocations + 6 string copies**. A nested object adds another 3. Total for the 200B JSON: **~9 tree node allocs**.

serde_json uses `Vec<(String, Value)>` (flat vector) or `BTreeMap` depending on feature flags. The default is a **flat vector with linear scan** — zero node overhead, one allocation for the vector.

**Fix**: Replace `std::map<std::string, JsonValue>` with `std::vector<std::pair<std::string, JsonValue>>` (flat representation). Linear scan is faster than RB-tree for <20 fields (cache locality dominates).

**Expected impact**: Eliminates 9 allocations per parse → **3-5x improvement**.

---

**Finding F-002**: `JsonValue` is a `std::variant` with `Box<JsonArray>` and `Box<JsonObject>` at [json_value.hpp:388-393](compiler/include/json/json_value.hpp#L388).

```cpp
using ValueVariant = std::variant<Null, bool, JsonNumber, std::string,
                                  Box<JsonArray>,   // unique_ptr<vector<JsonValue>>
                                  Box<JsonObject>>;  // unique_ptr<map<string,JsonValue>>
```

Every array/object value does a `unique_ptr` allocation. The variant size is `max(monostate, bool, JsonNumber, string, unique_ptr, unique_ptr)` = `sizeof(std::string)` = 32 bytes on MSVC. Total `JsonValue` = ~40 bytes (variant + discriminant).

serde_json's `Value` is also a tagged enum but stores vectors/maps inline (no extra indirection).

**Fix**: Remove `Box<>` wrapping. Store `JsonArray` and `JsonObject` directly in the variant. Increases `JsonValue` size but eliminates 2 allocations per nested structure.

**Expected impact**: 2 fewer allocs per array/object → **1.5-2x improvement**.

---

**Finding F-003**: `parse_json_fast()` does NOT use `JsonArena` at [json_allocator.cpp:29](compiler/src/json/json_allocator.cpp#L29).

```cpp
auto JsonDocument::parse(std::string_view input, size_t arena_size) {
    JsonDocument doc(arena_size);
    auto result = fast::parse_json_fast(input);  // ← standard allocator!
    doc.set_root(std::move(unwrap(result)));
    return doc;
}
```

The `JsonArena` class at [json_allocator.hpp:194-306](compiler/include/json/json_allocator.hpp#L194) has full bump allocation, string interning, and reset(). But `parse_json_fast()` at [json_fast_parser.cpp](compiler/src/json/json_fast_parser.cpp) allocates with `std::string()`, `std::vector()`, and `std::map()` — all using the default allocator.

**Fix**: Thread a `JsonArena*` through the fast parser. Use `arena->alloc_string()` for keys, polymorphic allocator for `std::vector`/`std::map`.

**Expected impact**: Eliminates ALL per-parse allocations → **5-10x improvement**. Combined with F-001 (flat objects), target is **<2x vs serde_json**.

---

**Finding F-004**: FFI handle system adds overhead at [json_runtime.cpp:91-111](compiler/src/json/json_runtime.cpp#L91).

```cpp
static std::vector<JsonValue> json_values;
static std::vector<bool> json_values_free;

static int64_t alloc_json_handle(JsonValue&& value) {
    for (size_t i = json_values_next_free; i < json_values_free.size(); ++i) {
        if (json_values_free[i]) { /* reuse slot */ }
    }
    json_values.push_back(std::move(value));  // may reallocate entire vector
}
```

Linear scan for free slots + potential vector reallocation. Not the primary bottleneck (~100 ns) but avoidable.

**Fix**: Use the arena-based document pattern instead of global handle table. The `JsonDocument` owns parsed data; TML gets an opaque handle to the document.

---

**Finding F-005**: `tml_json_array_get()` CLONES values at [json_runtime.cpp:~423](compiler/src/json/json_runtime.cpp).

When TML accesses `json::array_get(h, i)`, the C++ FFI clones the `JsonValue` (including deep-copying strings and sub-objects). This multiplies allocations for field access.

**Fix**: Return a reference handle (index into the existing document) instead of cloning.

---

### JSON Summary: Fix Priority

| # | Fix | Files | Allocs Eliminated | Expected Speedup |
|---|-----|-------|-------------------|-----------------|
| F-001 | `std::map` → `std::vector<pair>` | json_value.hpp | 9/parse | 3-5x |
| F-003 | Wire arena into fast parser | json_fast_parser.cpp | ALL | 5-10x |
| F-002 | Remove `Box<>` on array/object | json_value.hpp | 2/nested | 1.5-2x |
| F-005 | Return refs, don't clone | json_runtime.cpp | N/access | 2x for access |
| F-004 | Arena-based document handle | json_runtime.cpp | 2/parse | minor |

**Combined target**: 11,175 ns → **<1,500 ns** (~7.5x improvement, <2x vs serde_json).

---

## Part II — String Operations

### Measured

| Operation | TML (ns/op) | Rust (ns/op) | Ratio |
|-----------|-------------|-------------|-------|
| Concat Small (literals) | 0 | 180 | **TML wins** |
| Text push_str (100K, reserved) | 4 | 1 | 4x |
| Str += loop (10K) | 3,293 | 3 | **1,098x** |
| Int to String | 41 | 7 | **5.9x** |
| Log building Text (10K) | 60 | 52 | **1.15x** |
| Log building Str += (1K) | 4,155 | 93 | 44.7x |

### Finding F-006: `I64.to_string()` uses `malloc(24) + snprintf` at [mir_codegen.cpp:1078-1083](compiler/src/codegen/mir_codegen.cpp#L1078)

```llvm
define internal ptr @tml_N4core3I649to_stringE(i64 %v) {
  %buf = call ptr @malloc(i64 24)           ; ← HEAP ALLOC every call
  call i32 @snprintf(ptr %buf, i64 24, ptr @.fmt.i64, i64 %v)  ; ← libc snprintf
  ret ptr %buf
}
```

Every `i.to_string()` does:
1. `malloc(24)` — ~10-15 ns (allocator overhead)
2. `snprintf("%ld", v)` — ~15-20 ns (format parsing + division loop)
3. Return heap pointer that must be freed

Rust's `itoa` crate uses stack buffer + branchless two-digit lookup table. **Zero allocation.**

**Fix (P0)**: Use stack buffer + manual digit extraction (already exists in [text.tml:134-172](lib/std/src/text.tml#L134) as `text_i64_write_at`). Emit inline IR that:
1. Uses a 24-byte stack alloca (not malloc)
2. Writes digits via division loop
3. Returns pointer to stack (valid for the expression lifetime)

Or better: use TML's existing `text_i64_write_at` which avoids snprintf entirely.

**Expected impact**: 41 ns → **5-8 ns** (eliminate malloc + snprintf overhead).

---

### Finding F-007: `Text.push_str` cost is 4 ns/op due to strlen FFI

In [text.tml:610-614](lib/std/src/text.tml#L610):
```tml
pub func push_str(this, s: Str) {
    let slen: I64 = text_str_len(s)    // ← calls C strlen via FFI
    if slen <= 0 { return }
    let self_ptr: *Unit = lowlevel { this as *Unit }
    text_push_str_ptr(self_ptr, s, slen)
}
```

`text_str_len` calls `@extern("strlen")` at [text.tml:61](lib/std/src/text.tml#L61). For `push_str("ab")`, strlen("ab") is trivial but the **FFI call overhead** is ~3-5 ns on Windows (DLL export lookup + calling convention switch).

Rust's `push_str` knows the length at compile time for string literals (it's encoded in the `&str` fat pointer).

**Fix**: For constant string arguments, the compiler should propagate the known length and avoid strlen entirely. This requires a codegen optimization that detects `push_str(<literal>)` and emits `text_push_str_ptr(self, literal, KNOWN_LEN)` directly.

**Expected impact**: 4 ns → **1-2 ns** (eliminate strlen FFI for literals).

---

### Finding F-008: `Str +=` is fundamentally O(n²) — no fix possible

`Str` is a raw `ptr` (null-terminated C string). It has:
- No length field (must strlen every time)
- No capacity field (can't extend in-place)
- No ownership tracking (can't safely realloc)

The `str_concat_reuse` optimization ([runtime.cpp:526-555](compiler/src/codegen/llvm/core/runtime.cpp#L526)) uses `mem_realloc` with 2x growth, but `strlen(a)` is still called every iteration → O(n) per call → O(n²) total.

**This is unfixable without changing the Str type.** The correct answer is: **use Text for string building.** Text has length tracking, capacity, SSO, and amortized O(1) append.

The benchmark proves this: `Text push_str = 4 ns/op` vs `Str += = 3,293 ns/op`.

**Recommendation**: Document that `Str +=` in loops is an anti-pattern. Lint rule: warn when `var s: Str = ...; s = s + ...` appears in a loop body.

---

### Finding F-009: `Text.as_str()` allocates a COPY every time at [text.tml:482-493](lib/std/src/text.tml#L482)

```tml
pub func as_str(this) -> Str {
    ...
    let buf: *U8 = lowlevel { mem_alloc(slen + 1) } as *U8    // ← HEAP ALLOC
    lowlevel { copy_nonoverlapping(data, buf, slen) }           // ← FULL COPY
    lowlevel { ptr_write[U8]((buf as I64 + slen) as *U8, 0 as U8) }
    return buf as Str
}
```

Every `text.as_str()` does malloc + memcpy. For SSO inline strings (≤23 chars), this is particularly wasteful — the data is RIGHT THERE in the struct but gets copied to a new heap buffer.

**Fix**: Add `as_ptr() -> Str` that returns a direct pointer to the inline/heap data WITHOUT copying. For heap mode: return `_w0 as Str` (null-terminated). For inline mode: return a pointer into the struct (requires the struct to stay alive — borrow semantics).

Alternatively: for heap mode, the data buffer IS already null-terminated. Just return the pointer.

**Expected impact**: Eliminates allocation in most `as_str()` uses → significant reduction in log building and template literal overhead.

---

### Finding F-010: `str_free` does PE image binary search at [str_free.c](compiler/runtime/text/str_free.c)

Before freeing a string, `tml_str_free` checks if the pointer is in the PE image range (to avoid freeing string literals). This involves:
1. Get PE image base + size (cached)
2. Compare pointer range
3. If outside: call `mem_free`

Cost: ~3 ns for the range check + ~20-50 ns for `mem_free`.

This is correct but adds overhead to every string deallocation. Not a primary bottleneck.

---

## Part III — Combined Impact Estimate

### If ALL fixes applied:

| Operation | Current | Target | Improvement |
|-----------|---------|--------|-------------|
| JSON Parse Small | 11,175 ns | <1,500 ns | 7.5x |
| Int to String | 41 ns | 5-8 ns | 5-8x |
| Text.push_str | 4 ns | 1-2 ns | 2-4x |
| Text.as_str | ~10 ns (alloc) | 0 ns (ptr) | ∞ |
| Text Log Building | 60 ns | 20-30 ns | 2-3x |

### Priority ranking:

| # | Finding | Impact | Effort | Priority |
|---|---------|--------|--------|----------|
| F-006 | I64.to_string malloc+snprintf | 5.9x gap → 1x | Low (inline IR) | **P0** |
| F-001 | JSON std::map → vector | 3-5x improvement | Low (typedef) | **P0** |
| F-009 | Text.as_str heap copy | pervasive waste | Low (add as_ptr) | **P1** |
| F-003 | JSON arena in fast parser | 5-10x improvement | Medium (thread arena) | **P1** |
| F-007 | strlen FFI for known literals | 4x→2x | Medium (codegen opt) | **P1** |
| F-002 | JSON Box<> indirection | 1.5-2x | Low (remove Box) | **P2** |
| F-005 | JSON clone on access | 2x for reads | Medium | **P2** |
| F-008 | Str += O(n²) | unfixable | — | **Lint rule only** |
