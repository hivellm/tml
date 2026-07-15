# 01 — String Type Architecture

## Three String Types

### `Str` — Raw Pointer (like C `const char*` / Rust `&str`)

```
Stack: 8 bytes (ptr)
       │
       ▼
Heap or .rdata: "hello\0"  (null-terminated bytes)
```

- **Size**: 8 bytes (single pointer)
- **Ownership**: none — may point to static .rdata, heap, or stack
- **Length**: unknown at zero cost — requires `strlen()` O(n) traversal
- **Capacity**: unknown — cannot extend in-place
- **Concatenation**: `str_concat_opt` — always allocates fresh buffer, copies both sides
- **Deallocation**: `tml_str_free` — PE image range check before `free()`

### `Text` — SSO Tagged Union (like Rust `String`)

Since v0.3.29, Text uses Small String Optimization:

```
Inline mode (_w2 < 0):            Heap mode (_w2 >= 0):
┌────────────────────────┐        ┌────────────────────────┐
│ _w0: bytes 0-7         │  8B    │ _w0: data_ptr (I64)    │  8B
│ _w1: bytes 8-15        │  8B    │ _w1: length (I64)      │  8B
│ _w2: bytes 16-22 | tag │  8B    │ _w2: capacity (I64)    │  8B
└────────────────────────┘        └────────────────────────┘
 tag = byte 23 = 0x80 | len        _w2 >= 0 → heap mode
 len ≤ 23: zero heap allocation     data_ptr → null-terminated buffer
```

- **Size**: 24 bytes (3 × I64)
- **Ownership**: owns the data (inline or heap)
- **Length**: O(1) — stored in tag byte (inline) or `_w1` (heap)
- **Capacity**: 23 bytes (inline) or `_w2` (heap)
- **Concatenation**: `push_str()` — amortized O(1), grows by 2x
- **Deallocation**: `drop()` — frees heap buffer; no-op for inline

### `InternTable` — Deduplicated String Pool

```
┌──────────────────────────────┐
│ HashMap[Str, I64]  (str→id)  │
│ List[Str]          (id→str)  │
└──────────────────────────────┘
```

- Used in the TML lexer for identifier deduplication
- `intern(s)` → returns integer ID; same string → same ID
- `lookup(id)` → returns the original Str
- Comparison: `id1 == id2` (O(1) integer) instead of `strcmp` (O(n))

## Type Comparison

| Property | Str | Text (inline) | Text (heap) | Rust &str | Rust String |
|----------|-----|---------------|-------------|-----------|-------------|
| Size | 8B | 24B | 24B | 16B (ptr+len) | 24B (ptr+len+cap) |
| Length cost | O(n) strlen | O(1) tag | O(1) field | O(1) field | O(1) field |
| Alloc on create | 0 or 1 | 0 | 1 | 0 | 1 |
| Append cost | O(n) full copy | O(1) amortized | O(1) amortized | N/A (immut) | O(1) amortized |
| Null-terminated | Yes | No (inline) | Yes (heap) | No | No |

## Key Difference vs Rust

Rust `&str` is a **fat pointer**: `(ptr, len)` = 16 bytes. Length is always known.

TML `Str` is a **thin pointer**: `ptr` = 8 bytes. Length requires `strlen()`.

This single difference explains why:
- `push_str(literal)` is 4x slower in TML — must call `strlen` via FFI
- `str_concat_opt` is fundamentally O(n) — must `strlen` both sides every time
- Any operation on Str that needs the length pays an O(n) tax

## Code Locations

| Component | File | Key Functions |
|-----------|------|---------------|
| Str type | Built-in (ptr) | — |
| Text type | `lib/std/src/text.tml` | `from()`, `push_str()`, `as_str()`, `drop()` |
| Text SSO helpers | `lib/std/src/text.tml:174-397` | `text_sso_len()`, `text_data_ptr()`, `text_from_raw()` |
| str_concat_opt | `compiler/src/codegen/llvm/core/runtime.cpp:501-520` | Inline IR |
| str_concat_reuse | `compiler/src/codegen/llvm/core/runtime.cpp:526-555` | Inline IR with realloc |
| I64.to_string | `compiler/src/codegen/mir_codegen.cpp:1078-1083` | malloc(24) + snprintf |
| tml_str_free | `compiler/runtime/memory/str_free.c:125-166` | PE image check + HeapValidate |
| InternTable | `compiler-tml/src/intern.tml` | `intern()`, `lookup()` |
| String concat codegen | `compiler/src/codegen/llvm/expr/binary_ops.cpp:755-777` | is_heap_str_producer |
| Augmented concat | `compiler/src/codegen/llvm/expr/binary.cpp:86-127` | holds_heap_str tracking |
