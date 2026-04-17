# Proposal: phase1k_str-length-prefix

## Why

`Str += "x"` in a loop is currently **154x slower than Rust** (462 ns/op vs
Rust's 2 ns/op for 10K iterations of 2-byte appends). Phase 1i got us from
1098x down to 154x by adding amortized O(1) `str_append` via
`mem_usable_size` + exponential growth, but the remaining gap is
**structural**, not algorithmic:

- **Rust `String`** = `{ptr, len, cap}` struct. `push_str` reads `len` from
  the struct (1 cycle), writes the bytes, and bumps `len`. The hot inner
  loop compiles to `store i16 25185; add len, 2; br` — no function calls.

- **TML `Str`** = bare `ptr` (null-terminated). Every `str_append` call must
  `strlen(accumulator)` to find the tail. As the accumulator grows to 20 KB,
  `strlen` takes ~400 ns via AVX — which matches our measured 462 ns/op
  almost exactly. No amount of inlining or LLVM attribute tuning can remove
  this cost; `strlen` is intrinsically O(n) on a null-terminated buffer.

To close the gap we need to **cache the length** somewhere we can read in
O(1). Changing `Str` to a fat pointer `{ptr, len, cap}` would break the
language ABI everywhere (every function taking/returning `Str`, every
extern binding, every test). The **length-prefix** approach preserves the
`Str = ptr` ABI: we allocate every heap-backed `Str` as
`[cap:i64 | len:i64 | data... | NUL]` and hand out a pointer to `data`. The
header lives at `ptr[-16]` and `ptr[-8]`. Literals in `.rdata` don't have
the header — we discriminate via a Windows image-range check (already
exists in `mem_usable_size`) and fall back to `strlen` for them.

Source: docs/analysis/string/README.md — phase1i analysis already
identified this as the next step.

## What Changes

### New runtime primitives (C)

- `tml_str_alloc(len: i64) -> ptr` — allocates `16 + len + 1` bytes, zeroes
  the header, returns a pointer 16 bytes into the allocation. The returned
  pointer is a valid null-terminated C string; `tml_str_free` knows to
  subtract 16. Writes `cap = len` and `len = 0` into the header.
- `tml_str_alloc_with_cap(len: i64, cap: i64) -> ptr` — same but with
  pre-reserved capacity for builder patterns.
- `tml_str_len(ptr) -> i64` — fast path: image-range check says heap →
  read `ptr[-8]`. Slow path: `strlen(ptr)` for literals.
- `tml_str_set_len(ptr, new_len: i64)` — update `ptr[-8]` for heap strings
  (no-op for literals, guarded).
- `tml_str_cap(ptr) -> i64` — read `ptr[-16]`; returns 0 for literals.
- `tml_str_free(ptr)` — adjusts to `ptr - 16` before calling `mem_free`.
  Preserves NULL safety and literal-detection (skip if not heap).

### Updated runtime primitives

- `mem_usable_size(ptr)` — already detects literals via image-range; extend
  so it also returns the `cap` field for prefixed heap strings.
- Every C producer of `Str` heap buffers must use `tml_str_alloc` instead
  of raw `mem_alloc`: `tml_I64_to_string`, `tml_I32_to_string`, etc.;
  format/interpolation helpers; `str_concat_opt`; `str_concat_reuse`;
  `str_append`; `str_append_known_len`; text/interp helpers that return
  `Str`.

### Updated codegen (LLVM IR emit)

- `str_append` / `str_append_known_len` in `runtime.cpp` and
  `mir_codegen.cpp`: replace `strlen(a)` with `tml_str_len(a)`.
- `str_concat_reuse`: same replacement.
- AST `to_string` inline definitions in `runtime.cpp` (`tml_N4core3I64…`,
  etc.): allocate via `tml_str_alloc`, write the length field.
- `tml_str_free` call sites: no source change needed — the function's
  internal behavior changes.
- `Str::len()` method dispatch in codegen: emit `tml_str_len` instead of
  `strlen` (O(1) for our heap strings).

### TML stdlib

- `core/str/basic.tml`: `len()` — change implementation to call
  `tml_str_len` via `@extern("c")` binding. Still returns identical result
  for literals (falls back to `strlen`), just O(1) for heap strings.
- No user-facing `Str` API changes. No `.tml` code needs to be edited.

## Impact

- **Affected specs**: `docs/specs/*` — none (internal ABI, no language
  surface change).
- **Affected code**:
  - `compiler/runtime/memory/mem.c` — add `tml_str_alloc`, `tml_str_len`,
    `tml_str_set_len`, `tml_str_cap`, update `tml_str_free`
    (`compiler/runtime/text/str_free.c`).
  - `compiler/runtime/text/*.c` — every producer using `mem_alloc` for a
    `Str` return must switch to `tml_str_alloc`. Audit: `str_ops.c`,
    `str_concat.c`, `text_*.c`, interpolation helpers.
  - `compiler/src/codegen/llvm/core/runtime.cpp` — update `str_append`,
    `str_concat_reuse`, `tml_N4core…to_string` inline definitions; add
    `tml_str_alloc`/`tml_str_len` declarations.
  - `compiler/src/codegen/mir_codegen.cpp` — same for MIR preamble.
  - `lib/core/src/str/basic.tml` — `len()` uses `tml_str_len`.
  - Regression tests covering: empty literal roundtrip, heap-to-literal
    concat, literal-to-heap concat, `tml_str_free` on literals (no-op),
    `Str` passed across FFI boundary (must still be NUL-terminated C
    string — verify `strlen` still returns correct value).
- **Breaking change**: NO at the source level. Binary ABI changes for heap
  `Str` buffers — incremental caches must be invalidated once on upgrade.
- **User benefit**: `Str += X` in loops drops from 462 ns/op to expected
  ~5–10 ns/op (within 3–5x of Rust; remaining gap is `memcpy` vs
  `store i16` for 2-byte literals). Unblocks the `std::json` string-heavy
  code paths and the self-hosting compiler's token/identifier assembly
  hot paths. `Str::len()` becomes O(1) for heap strings — removes a latent
  O(n²) class from every TML program that builds strings incrementally.
