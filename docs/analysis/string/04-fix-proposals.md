# 04 — Fix Proposals

## Priority Matrix

```
                    Low Effort ◄──────────► High Effort
                    │                              │
High Impact   ┌─────┤ F-001 (to_string)            │
              │     │ F-002 (as_ptr)               │
              │     │                  F-003 (strlen│
              │     │                    propagate) │
              ├─────┤                              │
Low Impact    │     │ F-005 (HeapValid)             │
              │     │              F-004 (lint rule)│
              └─────┤                              │
```

---

## F-001: Replace `malloc+snprintf` with stack buffer + digit loop (P0)

**File**: `compiler/src/codegen/mir_codegen.cpp:1078-1083`

**Current** (41 ns, 1 heap alloc per call):
```llvm
%buf = call ptr @malloc(i64 24)
call i32 @snprintf(ptr %buf, i64 24, ptr @.fmt.i64, i64 %v)
ret ptr %buf
```

**Proposed** (target ~5-8 ns, 0 heap alloc):
```llvm
define internal ptr @tml_N4core3I649to_stringE(i64 %v) {
entry:
  %buf = alloca [24 x i8], align 1          ; STACK buffer, not heap
  ; Manual digit extraction (no snprintf):
  ; 1. Handle sign
  ; 2. Extract digits via udivrem loop
  ; 3. Write digits in reverse order
  ; 4. Copy result to a minimal heap buffer (or return stack ptr if lifetime allows)
  ...
}
```

Or even better — use the existing `text_i64_write_at` function from `lib/std/src/text.tml:134-172` which already does manual digit extraction without snprintf. Wire it into the MIR codegen path.

**Alternative quick fix**: Replace `snprintf` with a hand-written C function:
```c
// In compiler/runtime/core/essential.c
TML_EXPORT char* i64_to_string_fast(int64_t v) {
    static __thread char buf[24];  // thread-local, zero alloc
    char* end = buf + 23;
    *end = '\0';
    char* p = end;
    uint64_t abs = v < 0 ? (uint64_t)(-v) : (uint64_t)v;
    do { *--p = '0' + (abs % 10); abs /= 10; } while (abs);
    if (v < 0) *--p = '-';
    // Must return heap copy (caller may store the Str)
    size_t len = end - p;
    char* result = (char*)mem_alloc(len + 1);
    memcpy(result, p, len + 1);
    return result;
}
```

This eliminates snprintf overhead (~20 ns) but still needs a heap copy for the return. Net: ~15-20 ns (from 41 ns).

For zero-alloc: requires changing the return convention to use caller-provided buffer, which is a larger codegen change.

**Expected impact**: 41 ns → 15-20 ns (quick fix) or 5-8 ns (stack buffer).

---

## F-002: Add `Text.as_ptr()` that returns direct pointer (P0)

**File**: `lib/std/src/text.tml` — add new method

**Current `as_str()`** (allocates every call):
```tml
pub func as_str(this) -> Str {
    let buf: *U8 = lowlevel { mem_alloc(slen + 1) } as *U8  // ALLOC
    lowlevel { copy_nonoverlapping(data, buf, slen) }         // COPY
    return buf as Str
}
```

**Proposed `as_ptr()`** (zero alloc for heap mode):
```tml
/// Returns a direct pointer to the string data.
/// For heap mode: returns the data buffer pointer (already null-terminated).
/// For inline mode: returns pointer into the struct (valid while Text is alive).
/// WARNING: The returned pointer is only valid while the Text is not modified or dropped.
pub func as_ptr(this) -> Str {
    let self_ptr: *Unit = lowlevel { this as *Unit }
    let self_addr: I64 = self_ptr as I64
    let w2: I64 = this._w2
    if w2 >= 0 {
        // Heap: data buffer is already null-terminated
        return this._w0 as Str
    }
    // Inline: need to null-terminate in the struct
    // We can write \0 at byte position `len` (which is within the 24-byte struct)
    let slen: I64 = (w2 >> 56) & 127
    let end_ptr: *U8 = (self_addr + slen) as *U8
    lowlevel { ptr_write[U8](end_ptr, 0 as U8) }
    return self_addr as Str
}
```

For inline mode, this writes a null terminator at `struct_addr + len`. Since `len ≤ 23` and the struct is 24 bytes, byte 23 is the tag. If `len < 23`, byte `len` is within the data area (bytes 0-22) and can safely be set to `\0`. If `len == 23`, byte 23 is the tag byte — we can't overwrite it without losing the inline flag. In that case, fall back to `as_str()` (alloc copy).

**For `println` and `print`**: change `Text.println()` and `Text.print()` to use `as_ptr()` instead of `as_str()`. This eliminates the allocation for the most common use case.

**Expected impact**: Eliminates 1 allocation per `text.println()` / `text.print()` call. Pervasive improvement.

---

## F-003: Propagate known string lengths for literals (P1)

**File**: `compiler/src/codegen/llvm/expr/binary_ops.cpp` and method dispatch

**Current**: `push_str("ab")` calls `text_str_len("ab")` which calls `strlen("ab")` via FFI.

**Proposed**: When the argument to `push_str` is a string literal, the compiler knows its length at compile time. Emit `text_push_str_ptr(self, "ab", 2)` directly, bypassing `strlen`.

**Implementation**:
1. In the codegen for method calls, check if the argument is a `StringLiteral`
2. If so, compute the length at compile time
3. Emit `text_push_str_ptr(self_ptr, literal_ptr, KNOWN_LENGTH)` instead of `text.push_str(s)` → `text_str_len(s)` → `text_push_str_ptr(...)`

This is a targeted codegen optimization for the hot path. Does NOT require changing the TML source.

**Expected impact**: 4 ns → 1-2 ns for `push_str(literal)`.

---

## F-004: Lint rule for `Str +=` in loops (P2)

**File**: Linter / diagnostic system

The `Str +=` pattern is unfixable at the runtime level. The correct fix is to educate users:

```
warning[PERF001]: Str concatenation in loop is O(n^2)
  --> src/main.tml:42:9
   |
42 |     result = result + "ab"
   |     ^^^^^^^^^^^^^^^^^^^^^^
   |
   = help: use `Text::with_capacity(N)` and `push_str()` for O(n) string building
   = note: Str has no length tracking; every concatenation scans the entire string
```

**Expected impact**: Zero runtime cost. Prevents users from writing O(n^2) string code.

---

## F-005: Bypass HeapValidate in tml_str_free (P2)

**File**: `compiler/runtime/memory/str_free.c:146-148`

**Current**: After the PE image range check, calls `HeapValidate(heap, 0, ptr)` (~100 ns).

**Proposed**: Since the codegen tracks `holds_heap_str` and `is_heap_str_producer`, `tml_str_free` is only called on pointers the codegen KNOWS are heap-allocated. The HeapValidate check is redundant.

Change `tml_str_free` to skip HeapValidate and call `mem_free` directly:
```c
TML_EXPORT void tml_str_free(void* ptr) {
    if (!ptr) return;
    if (tml_is_image_ptr((uintptr_t)ptr)) return;  // constant check (~3 ns)
    mem_free(ptr);                                   // direct free (~20 ns)
}
```

**Risk**: If a non-heap pointer passes the image range check (e.g., stack pointer), `free()` would crash. But with the codegen tracking (`is_heap_str_producer`), this shouldn't happen for correctly compiled code.

**Expected impact**: ~100 ns → ~25 ns per string deallocation.

---

## Implementation Order

| Phase | Fix | Effort | Cumulative Impact |
|-------|-----|--------|-------------------|
| 1 | F-001 (to_string fast) | 2-4h | 41 ns → 15-20 ns |
| 2 | F-002 (as_ptr) | 1-2h | Eliminates alloc in println/print |
| 3 | F-005 (skip HeapValidate) | 30min | 100 ns → 25 ns per free |
| 4 | F-003 (strlen propagation) | 4-8h | push_str 4 ns → 1-2 ns |
| 5 | F-004 (lint rule) | 2-4h | Prevents O(n^2) patterns |

**Overall target**: Close the 5.9x `to_string` gap and the 4x `push_str` gap. `Text` log building already at 1.15x Rust — maintain parity.
