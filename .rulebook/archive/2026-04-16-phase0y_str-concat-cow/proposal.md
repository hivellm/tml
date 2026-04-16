# Proposal: phase0y_str-concat-cow

## Why

Benchmark analysis (2026-04-14) shows `Str +=` in a loop is 1718x slower than
the Rust equivalent:

| Benchmark | TML | Rust | Ratio |
|-----------|-----|------|-------|
| Concat Loop Str+= (10K iter) | 3437 ns/op | 2 ns/op | **1718x** |
| Log Building Str (1K iter) | 4246 ns/op | 66 ns/op | **64x** |

Root cause: `str_concat_opt` (in `compiler/src/codegen/llvm/core/runtime.cpp`) always
does a full `malloc + memcpy`, regardless of whether the left operand could be extended
in-place. The comment in `binary_ops.cpp` claims "O(1) amortized" but `str_concat_opt`
allocates exactly `len_a + len_b + 1` bytes with zero extra capacity — it is O(n) per
call, O(n²) total in a loop.

```
str_concat_opt(ptr %a, ptr %b):
  len_a = strlen(%a)
  len_b = strlen(%b)
  buf   = mem_alloc(len_a + len_b + 1)   ← full alloc every time
  memcpy(buf, a, len_a)                  ← full copy every time
  memcpy(buf+len_a, b, len_b)
  ret buf                                 ← %a is freed by caller
```

For `result = result + "ab"` in a loop: iteration k copies k×2 bytes. Total bytes
copied = 2+4+6+…+20000 = O(n²). At 10K iterations this is ~100MB of memcpy.

The compiler already detects when the left operand is a heap-allocated temporary
(`is_heap_str_producer()` in `binary_ops.cpp`) and frees it after the call. That
consumed pointer can instead be **realloc'd with exponential growth**, making the
pattern O(1) amortized — exactly what Rust does with `String +=`.

## What Changes

### 1. Add `str_concat_reuse` to the inline IR catalog (`runtime.cpp`)

A new variant of `str_concat_opt` that uses `realloc` + exponential growth when
the left operand is a consumed heap pointer:

```llvm
define internal ptr @str_concat_reuse(ptr %a, ptr %b) {
entry:
  %len_a = call i64 @strlen(ptr %a)
  %len_b = call i64 @strlen(ptr %b)
  %needed = add i64 %len_a, %len_b
  ; exponential capacity: next_pow2(needed) or needed*2, whichever is larger
  %cap = add i64 %needed, 1         ; minimum: exact fit + NUL
  ; ... double until cap > needed or use realloc directly
  %buf = call ptr @realloc(ptr %a, i64 %cap)   ; extends in-place if possible
  %dst = getelementptr i8, ptr %buf, i64 %len_a
  call void @llvm.memcpy.p0.p0.i64(ptr %dst, ptr %b, i64 %len_b, i1 false)
  %end = getelementptr i8, ptr %buf, i64 %needed
  store i8 0, ptr %end
  ret ptr %buf
}
```

### 2. Use `str_concat_reuse` when left is heap-consumed (`binary_ops.cpp`)

```cpp
// In gen_binary_ops, BinOp::Add string path:
if (is_heap_str_producer(*bin.left)) {
    // Left is a heap temporary being consumed — reuse its buffer
    emit_line("  " + result + " = call ptr @str_concat_reuse(ptr " + left +
              ", ptr " + right + ")");
    // No tml_str_free for left — realloc consumed it
    // Right still freed if it's also a heap temp
} else {
    // Left is a literal/variable — must alloc fresh
    emit_line("  " + result + " = call ptr @str_concat_opt(ptr " + left +
              ", ptr " + right + ")");
    // Free temporaries as before
}
```

### 3. Update `realloc` declaration in the preamble

`realloc` must be declared (like `malloc`/`free`) in `emit_preamble()` in
`mir_codegen.cpp` and `generate_support.cpp`.

### Expected outcome

- `result = result + "ab"` loop: O(1) amortized (realloc extends in-place on most
  allocators when the block is at the heap frontier)
- Target: ≤ 10 ns/op for Concat Loop (vs 3437 ns/op baseline)
- Ratio vs Rust: <5x (Rust benefits from zero-cost string literal handling which
  TML cannot match without SSO)

## Impact

- Affected code:
  - `compiler/src/codegen/llvm/core/runtime.cpp` (add `str_concat_reuse`)
  - `compiler/src/codegen/llvm/expr/binary_ops.cpp` (select reuse vs opt)
  - `compiler/src/codegen/mir_codegen.cpp` (realloc declaration)
  - `compiler/src/codegen/llvm/core/generate_support.cpp` (realloc declaration)
- Breaking change: NO — purely an optimization; same semantics, faster execution
- User benefit: `Str +=` in loops becomes usable without forcing migration to `Text`
