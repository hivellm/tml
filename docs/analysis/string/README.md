# String Performance Analysis

**Date**: 2026-04-16
**Status**: TML `Text` is **1.15x** Rust for log building (near parity). `Str +=` is **154x** slower (phase1i amortized O(1) append; was 1,098x). `I64.to_string()` is **5.9x** slower (malloc + snprintf).

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
| Str += loop (10K) | 462 ns | 3 ns | 154x | phase1i amortized append (was 3,293 ns / 1,098x) |
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
