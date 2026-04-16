# 03 — Bottleneck Analysis

## Bottleneck #1: `I64.to_string()` = malloc(24) + snprintf (5.9x vs Rust)

**File**: `compiler/src/codegen/mir_codegen.cpp:1078-1083`

```cpp
emitln("define internal ptr @tml_N4core3I649to_stringE(i64 %v) {");
emitln("entry:");
emitln("  %buf = call ptr @malloc(i64 24)");          // ← HEAP ALLOC
emitln("  call i32 (ptr, i64, ptr, ...) @snprintf(ptr %buf, i64 24, ptr @.fmt.i64, i64 %v)");
emitln("  ret ptr %buf");
emitln("}");
```

**Cost breakdown**:
- `malloc(24)`: ~15 ns (allocator overhead for a 24-byte buffer)
- `snprintf("%ld", v)`: ~20 ns (format string parsing + integer division loop)
- Total: ~35-41 ns

**Rust equivalent**: `itoa` crate uses a stack-local `[u8; 20]` buffer + two-digit lookup table (`DIGITS_LUT`). No heap allocation. No format string parsing. ~7 ns.

**Why snprintf is slow**:
1. Parses format string `"%ld"` character by character
2. Handles locale, padding, precision (none needed here)
3. Converts integer via repeated `% 10` + `/ 10` (not optimized)
4. Returns `int` (number of chars written) — extra work

**Root cause**: This inline IR was a quick implementation. TML already has a fast digit-writing function in `text.tml` (`text_i64_write_at`, lines 134-172) that uses manual digit extraction with no format string. But the MIR codegen path doesn't use it.

## Bottleneck #2: `Text.as_str()` heap-copies every call

**File**: `lib/std/src/text.tml:482-493`

```tml
pub func as_str(this) -> Str {
    let data: *U8 = text_data_ptr(self_addr, this._w0, w2)
    let buf: *U8 = lowlevel { mem_alloc(slen + 1) } as *U8     // ALLOC
    lowlevel { copy_nonoverlapping(data, buf, slen) }            // COPY
    lowlevel { ptr_write[U8]((buf as I64 + slen) as *U8, 0 as U8) }
    return buf as Str
}
```

Every call to `as_str()` allocates a NEW buffer and copies ALL the string data. For an inline SSO string of 5 bytes, this allocates 6 bytes on the heap and copies 5 bytes — completely defeating the point of SSO.

**Usage sites that multiply the cost**:
- `text.println()` → calls `as_str()` → allocates + copies → prints → leaks the copy
- `text.print()` → same
- Template literals that get printed → `Text::from(...)` (inline, 0 alloc) → `println(text)` → `as_str()` (1 alloc + copy)
- Any code that needs to pass a Text value to a function expecting Str

**Why this happens**: TML `Str` is a null-terminated `ptr`. Text's inline data is NOT null-terminated (byte 23 is the tag byte, not `\0`). So a copy is needed to create a null-terminated Str. For heap mode, the data buffer IS null-terminated, but `as_str()` still copies.

## Bottleneck #3: `strlen` FFI for known-length literals

**File**: `lib/std/src/text.tml:610-614`

```tml
pub func push_str(this, s: Str) {
    let slen: I64 = text_str_len(s)    // ← FFI call to C strlen
    ...
}
```

`text_str_len` calls `@extern("strlen")` (line 61-62). For `push_str("ab")`, the length is known at compile time (2 bytes). But the codegen doesn't propagate this — every call goes through the FFI boundary.

**Cost**: ~3-5 ns per call for the FFI transition (Windows DLL export resolution + calling convention switch). For a loop with 100K iterations, this adds ~300-500 us of pure overhead.

**Rust comparison**: `push_str("ab")` receives `&str` which is `(ptr, len)` — length is already encoded. No strlen needed.

## Bottleneck #4: `Str +=` is O(n^2) — structural limitation

**Root cause**: `Str` is a raw pointer with no length or capacity tracking.

`result = result + "ab"` at iteration N:
1. `strlen(result)` = O(N) — scans the entire string to find its length
2. `mem_realloc(result, (N+2)*2+1)` — may copy the entire string
3. `memcpy(buf+N, "ab", 2)` — append 2 bytes

Total work per iteration: O(N). Total for K iterations: O(K^2).

The `str_concat_reuse` optimization (using realloc with 2x growth) helps with allocation count but CANNOT eliminate the `strlen` cost. `strlen` is fundamental — without a stored length, the only way to know where the string ends is to scan for `\0`.

**This gap is unfixable without changing the Str type to carry a length.**

Text already solves this: `push_str` knows the current length (stored in `_w1` or the tag byte) and appends in O(1) amortized.

## Bottleneck #5: `tml_str_free` HeapValidate on Windows

**File**: `compiler/runtime/memory/str_free.c:146-148`

```c
HANDLE heap = GetProcessHeap();
if (HeapValidate(heap, 0, ptr)) {    // ← ~100 ns Windows kernel call
    mem_free(ptr);
}
```

After the PE image range check (~3-5 ns), if the pointer is NOT in any loaded module's image range, `HeapValidate` is called to verify it's a valid heap allocation. This is a Windows kernel call costing ~50-100 ns.

**Why it exists**: TML `Str` has no ownership model. A `ptr` might be:
- A string literal in .rdata (cannot free)
- A heap-allocated buffer from `str_concat_opt` (must free)
- A stack pointer (cannot free)
- A dangling pointer (must not free)

`HeapValidate` is the only safe way to distinguish these at runtime.

**Mitigation**: The codegen already avoids `tml_str_free` for known constants (via `is_heap_str_producer`). The HeapValidate path only runs for genuine heap strings. But it still adds ~100 ns to every string deallocation.
