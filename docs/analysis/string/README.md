# String Performance Analysis

**Date**: 2026-04-17
**Status**: TML `Text` matches Rust's `String::push_str` at **1 ns/op**. `Str +=` is **13x** slower than Rust after phase 1k (was 1,098x); the remaining gap is structural — `Str` is a null-terminated `ptr`, so length/capacity live in heap memory (not SSA registers like Rust's `String`). `I64.to_string()` is **5.9x** slower (malloc + snprintf).

## Phase 1k update (2026-04-17)

`Str +=` dropped from phase1i's 462 ns/op to **27 ns/op** via a
length-prefix header on heap-allocated Str buffers.

**Layout**: every heap Str buffer is allocated as
`[magic:u64 | cap:i64 | len:i64 | data... | NUL]` with the pointer
handed out pointing at `data`. The 24-byte header lives at
`ptr[-24..0)`. Legacy raw-`malloc` Str (e.g. `i64_to_str` output) and
`.rdata` literals are transparently detected via a magic-sentinel
check (`0x314B7274735F4C4D` = "ML_strK1") that falls back to `strlen`.

**Hot-path IR** inside `str_append` (all inline, no FFI):
```llvm
%magic = load i64, ptr (a - 24)
%is_ours = icmp eq i64 %magic, 3552126293525794637
br i1 %is_ours, label %has_hdr, label %literal
has_hdr:
  %len_a = load i64, ptr (a - 8), !range !str_len_range
  %cap = load i64, ptr (a - 16), !range !str_len_range
  ; ... in-place path: memcpy + store i16 NUL + store new len
  store i64 %total, ptr (a - 8)
  ret a
```

With `alwaysinline` on `str_append` propagating this into the caller's
loop body, LLVM:
- SROA-promotes the `result` alloca into a phi node
- Sees the memcpy's destination (`a + len_a`) doesn't alias the header
  (`a - 8`), enabling store-to-load forwarding across iterations
- Constant-folds `strlen("ab") -> 2` via `readonly nounwind willreturn`
  on the `strlen` declaration

**Results (10K iters, release)**:

| Op | Pre-phase1i | Phase 1i | **Phase 1k** | Rust | Gap |
|----|-------------|----------|--------------|------|-----|
| Str += loop | 3,044 ns | 462 ns | **27 ns** | 2 ns | 13x |

**112x faster than the original baseline. 17x faster than phase 1i.**

The remaining 13x gap vs Rust is structural: Rust's `String` keeps
`{ptr, len, cap}` as three SSA values in registers across loop
iterations. TML's `Str = ptr` forces len/cap access to go through
heap memory on every iteration (one load, one store per call). To
close the last 25 ns either:

- **Use `Text`** — TML's `Text` is a `{ptr, len, cap}` struct, already
  matches Rust's `String::push_str` at 1 ns/op. The `Str +=` pattern
  is documented (see `Concat Loop (Str - O(n^2))` in
  `benchmarks/profile_tml/string_bench.tml`) as the slow case; the
  fast case is `Text::with_capacity(n) + push_str(...)`.
- **Promote `Str` to a fat pointer at the language level** — would
  change the ABI (`Str` no longer interops directly with C `char*`);
  deferred as a backward-incompatible change.
- **MIR-level rewrite of the `var s: Str = ""; loop { s = s + x }`
  pattern** — detect the accumulator pattern and transparently
  rewrite to a hidden `Text` builder. Feasible but complex AST
  analysis; deferred.

## Phase 1i update (2026-04-16)

`Str +=` (and any `ptr + ptr` Add where the pointers are `Str`) now uses
a new `str_append` runtime function with amortized O(1) growth:

1. **In-place append**: if the left operand is a heap buffer with
   enough slack (checked via `mem_usable_size` — `_msize` on Windows,
   `malloc_usable_size` on glibc, `malloc_size` on macOS), the right
   string is `memcpy`'d past the existing NUL and the same pointer is
   returned.
2. **Grow in place**: if the buffer exists but is too small, a
   `mem_realloc` doubles its capacity (exponential growth) and the
   right string is appended.
3. **Fresh allocation**: if the left operand is a `.rdata` literal or
   lives outside the C heap (Windows image-range check on module
   segments prevents `_msize` UB), a new buffer is allocated with
   `len(a) + len(b) + 1` bytes.

Benchmark: Str Naive Append 10K (100-byte pieces) dropped from
**3,044 ns/op → 462 ns/op (-85%)**. Gap vs Rust closed from 1,098x to
154x — the remaining gap is `strlen` calls and `tml_str_free` overhead,
not algorithmic. Both AST and MIR codegen emit the new `@str_append`
for `Str + Str` Add.

## Phase 1h update (2026-04-16)

`Text.push_str` literal handling improved from **4 ns → 2 ns** by
declaring `strlen` / `strcmp` / `memcmp` with
`readonly nounwind willreturn` attributes. LLVM's SimplifyLibCalls pass
now recognizes them as canonical libc functions and constant-folds
`strlen(string_literal)` at -O1+, eliminating the FFI call for every
`push_str("literal")` site (and for `Text.starts_with` / `ends_with` /
`contains` when the needle is a literal).

F-003 (FFI strlen overhead on literals) is **resolved** for the common
case. Non-literal `push_str(some_str)` still pays the strlen cost —
that's load-bearing because the length isn't known at compile time.

## Documents

| File | Description |
|------|-------------|
| [01-type-architecture.md](01-type-architecture.md) | Str vs Text vs Interned — type design and memory layout |
| [02-operation-costs.md](02-operation-costs.md) | Per-operation cost breakdown with allocation trace |
| [03-bottleneck-analysis.md](03-bottleneck-analysis.md) | Root causes with file:line evidence |
| [04-fix-proposals.md](04-fix-proposals.md) | Prioritized fixes with expected impact |

## Key Numbers

| Operation | TML | Rust | Ratio | Notes |
|-----------|-----|------|-------|-------|
| Concat Small (literals) | 0 ns | 180 ns | **TML wins** | Compile-time folding |
| Text push_str (100K) | 2 ns | 1 ns | 2x | phase1h LLVM libc attrs |
| Str += loop (10K) | **27 ns** | 2 ns | 13x | phase1k length-prefix header (112x faster than original) |
| Int to String | 41 ns | 7 ns | **5.9x** | malloc + snprintf vs stack |
| Log building Text (10K) | 60 ns | 52 ns | **1.15x** | Near parity |
| Log building Str (1K) | 4,155 ns | 93 ns | 44.7x | O(n^2) |

## Findings Summary

| ID | Finding | Impact | Effort |
|----|---------|--------|--------|
| F-001 | `I64.to_string()` = `malloc(24) + snprintf` | 5.9x gap | Low |
| F-002 | `Text.as_str()` heap-copies every call | Pervasive waste | Low |
| F-003 | `strlen` FFI for known-length literals in `push_str` | 4x gap | Medium |
| F-004 | `Str +=` was O(n^2) — FIXED in phase1i via amortized O(1) `str_append` | 1,098x → 154x | Shipped 0.3.36 |
| F-005 | `tml_str_free` does HeapValidate (~100 ns) on Windows | Minor | Low |
