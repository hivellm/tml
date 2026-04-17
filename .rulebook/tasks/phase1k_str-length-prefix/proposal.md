# Proposal: phase1k_str-length-prefix

## Status: SHIPPED (27–28 ns/op, 112x baseline, 14x Rust)

Delivered across commits c7fe204d, 83571879, fa45034d, 187b1870,
e99c55e2, 543d2ca2, f8ce905b on `feat/self-hosting-compiler`.

## Why

`Str += "x"` in a loop was **154x slower than Rust** (462 ns/op vs Rust's
2 ns/op for 10K iterations of 2-byte appends) at the start of this phase.
Phase 1i got us from 1098x down to 154x by adding amortized O(1)
`str_append` via `mem_usable_size` + exponential growth, but the remaining
gap was **structural**, not algorithmic:

- **Rust `String`** = `{ptr, len, cap}` struct. `push_str` reads `len` from
  the struct (1 cycle), writes the bytes, and bumps `len`. The hot inner
  loop compiles to `store i16 25185; add len, 2; br` — no function calls.

- **TML `Str`** = bare `ptr` (null-terminated). Every `str_append` call had
  to `strlen(accumulator)` to find the tail. As the accumulator grows to
  20 KB, `strlen` takes ~400 ns via AVX — which matched the measured
  462 ns/op almost exactly. No amount of inlining or LLVM attribute tuning
  could remove this cost; `strlen` is intrinsically O(n) on a
  null-terminated buffer.

To close the gap we **cache the length** in a header. Changing `Str` to a
fat pointer `{ptr, len, cap}` would break the language ABI everywhere
(every function taking/returning `Str`, every extern binding, every test).
The **length-prefix** approach preserves the `Str = ptr` ABI: every
heap-backed `Str` is allocated as
`[magic:u64 | cap:i64 | len:i64 | data... | NUL]` and the pointer handed
out points at `data`. The header lives at `ptr[-24..0)`. Literals and
legacy raw-malloc heap strings are transparently detected via a magic
sentinel check (`0x314B7274735F4C4D` = "ML_strK1") that falls back to
`strlen` + free-at-ptr.

Source: docs/analysis/string/README.md — phase1i analysis already
identified this as the next step.

## What Shipped

### Runtime primitives (C) — `compiler/runtime/memory/mem.c`

- `tml_str_alloc(cap: i64) -> ptr` — allocates `24 + cap + 1` bytes,
  writes `[magic | cap | len=0]` into the header, returns a pointer
  `+24` into the allocation. The returned pointer is a valid
  null-terminated C string; `tml_str_free` subtracts 24 back to the
  malloc root.
- `tml_str_alloc_len(len)` — used by producers that fill the buffer up
  front; returns `[magic | len | len]` with NUL at `data[len]`.
- `tml_str_alloc_with_cap(len, cap)` — builder pattern; pre-reserves
  `cap` bytes with initial `len`.
- `tml_str_len(ptr) -> i64` — magic-check at `ptr[-24]`; if match,
  return `ptr[-8]` (O(1)); else fall back to `strlen(ptr)`.
- `tml_str_set_len(ptr, new_len)` — update `ptr[-8]` when magic gates
  accept; no-op for literals / legacy raw-malloc strings.
- `tml_str_cap(ptr) -> i64` — read `ptr[-16]` when magic matches,
  returns 0 for literals and legacy strings.
- `tml_str_realloc(ptr, new_cap) -> ptr` — preserves header, supports
  exponential growth; promotes non-prefixed inputs into a fresh
  prefixed buffer.
- `tml_str_free(ptr)` — magic-gated: free at `ptr - 24` for prefixed,
  at `ptr` for legacy; `.rdata` literals bypass free entirely.
- `tml_safe_msize(ptr)` — wraps `_msize` with `HeapValidate` pre-check
  and a silent `_invalid_parameter_handler` so probing `ptr - 24` on
  legacy heap pointers never trips the CRT's abort path (fixes
  STATUS_HEAP_CORRUPTION observed in the first implementation).
- `tml_mem_is_image(ptr)` — exported image-range probe for use from
  generated IR.

### Codegen — `runtime.cpp` (AST) + `mir_codegen.cpp` (MIR)

- `str_append` rewritten: magic-gated inline header reads (no FFI in
  hot path), `tml_str_alloc_with_cap` for fresh path, `tml_str_realloc`
  for grow, `alwaysinline` so LLVM propagates the whole body into the
  caller's loop.
- `!range !str_len_range` metadata on len/cap loads + `!alias.scope`
  separating header region from data region — unlocks store-to-load
  forwarding across loop iterations.
- `str_append_tracked` variant emitted (takes a shadow `len_slot`
  pointer) — foundation for a future MIR-level accumulator rewrite.

### Runtime — `compiler/runtime/memory/str_free.c`

- `tml_str_free` updated to magic-gate the -24 offset so it correctly
  handles a mix of prefixed and legacy heap buffers within the same
  program.

## Impact

- **Affected specs**: none (internal ABI, no language surface change).
- **Breaking change**: none at the source level. Binary ABI changes
  for heap `Str` buffers require a one-time incremental cache
  invalidation on upgrade.
- **Measured perf**:
  - `Str += loop (10K)`: 3,044 ns (pre-phase1i) → 462 ns (phase1i) →
    **27–28 ns** (phase1k) — **112x** vs original baseline,
    **14x vs Rust** (was 1,098x at baseline).
  - `Int to String`, `Log Building`, `String Compare` benchmarks
    unchanged.
  - `Text push_str` unchanged at 4 ns (already Rust-parity class).
  - Full `string_bench.tml` completes exit 0 across 3 consecutive runs.
- **Commits**: c7fe204d (WIP), 83571879 (crash fix), fa45034d
  (magic-only fast path), 187b1870 (inline header reads), e99c55e2
  (!range metadata), 543d2ca2 (docs), f8ce905b (str_append_tracked
  scaffolding).
