## 1. Runtime primitives (C)
- [x] 1.1 Add `tml_str_alloc(cap: i64)` in `compiler/runtime/memory/mem.c` — `[magic | cap | len | data | NUL]` 24-byte header
- [x] 1.2 Add `tml_str_alloc_with_cap(len, cap)` + `tml_str_alloc_len(len)` variants
- [x] 1.3 Add `tml_str_len(ptr)` — magic-gated O(1) read from `ptr[-8]`; literal fallback calls `strlen`
- [x] 1.4 Add `tml_str_set_len(ptr, new_len)` — updates `ptr[-8]`; no-op when magic check fails
- [x] 1.5 Add `tml_str_cap(ptr)` — reads `ptr[-16]`; returns 0 for literals and legacy raw-malloc strings
- [x] 1.6 Update `tml_str_free` (`compiler/runtime/memory/str_free.c`) to adjust by -24 for prefixed buffers; free at `ptr` for legacy
- [x] 1.7 `TML_EXPORT __declspec(dllexport)` on every new symbol — per-function attributes, no `.def` file needed
- [x] 1.8 Add `tml_str_realloc(ptr, new_cap)` — preserves header, exponential-growth-capable
- [x] 1.9 Add `tml_safe_msize` wrapper — installs silent `_invalid_parameter_handler` and pre-checks via `HeapValidate` so probing `ptr - 24` never crashes
- [x] 1.10 Add `tml_mem_is_image` exported helper so generated IR can gate header probes on `.rdata` literals

## 2. Codegen — LLVM declarations & catalog
- [x] 2.1 Add all `@tml_str_*` + `@tml_mem_is_image` declarations to `runtime.cpp` catalog with `nounwind` (and `readonly willreturn` where safe)
- [x] 2.2 Same declarations in `mir_codegen.cpp` preamble
- [x] 2.3 Rewrite `str_append` in `runtime.cpp`: magic-gated inline header reads, `tml_str_alloc_with_cap` fresh path, `tml_str_realloc` grow, `alwaysinline`
- [x] 2.4 Same rewrite for `str_append` in `mir_codegen.cpp` preamble
- [x] 2.5 Add `!range !str_len_range` metadata on len/cap loads for LLVM AA
- [x] 2.6 `tml_N4core3I649to_stringE` bypassed at MIR preamble — overridden by the stdlib's own `i64_to_str` definition at link time. Migrated the stdlib's definition instead (item 4.6).
- [x] 2.7 Audited `runtime.cpp` — the only `mem_alloc` / `malloc` sites that produce `Str` are covered by `tml_str_free`'s magic-gated fallback. Hot-path producers all use `tml_str_alloc_with_cap` via `str_append`.

## 3. Codegen — method dispatch
- [x] 3.1 `core::str::basic::len` rebound to `@extern("tml_str_len")` — Str.len() is now O(1) for prefixed heap buffers, identical result for literals via `strlen` fallback
- [x] 3.2 MIR `.len()` dispatch inherits the stdlib binding
- [x] 3.3 `Str::is_empty()` calls `len()` which now uses `tml_str_len` — O(1) in the heap case
- [x] 3.4 Wired — `lib/core/src/str/basic.tml::len` binds `c_tml_str_len` via `@extern("tml_str_len")`

## 4. C runtime producers
- [x] 4.1 Audit done: legacy raw-malloc producers are covered by `tml_str_free`'s magic-gated fallback for correctness; hot-path producers migrated inline in the TML stdlib (item 4.6).
- [x] 4.2 Stdlib-side `str_split`, `substring`, `substring_raw` migrated in item 4.6
- [x] 4.3 `str_concat.c` / format helpers are no longer referenced from hot paths; str_append uses inline IR
- [x] 4.4 `text_*.c` returns `Text` structs, orthogonal to this phase
- [x] 4.5 `compiler/runtime/format/*.c` verified unused from hot paths
- [x] 4.6 Migrated all stdlib Str producers to `tml_str_alloc_len`:
  - `lib/core/src/fmt/helpers.tml`: `i64_to_str`, `u64_to_str`, `u64_to_binary_str`, `u64_to_octal_str`, `u64_to_hex_str`, `u128_to_str`, `char_to_str` (7 functions)
  - `lib/core/src/str/basic.tml`: `substring`
  - `lib/core/src/str/transform.tml`: `to_uppercase`, `to_lowercase` (scalar paths)
  - `lib/core/src/str/convert.tml`: `repeat`, `join_list`
  - `lib/core/src/str/replace.tml`: `replace`, `replace_first`, `replacen`
  - `lib/core/src/str/simd.tml`: `to_uppercase_simd`, `to_lowercase_simd`, `substring_raw`

## 5. TML stdlib
- [x] 5.1 `lib/core/src/str/basic.tml::len()` rebound to `@extern("tml_str_len")` — O(1) for prefixed buffers
- [x] 5.2 `is_empty()` benefits transparently via the new `len()`
- [x] 5.3 Verified: no stdlib `@extern("c")` bindings call raw `strlen` on a `Str` directly — all routes go through `core::str::basic::len` which now calls `tml_str_len`
- [x] 5.4 Rebinding done in 5.1

## 6. Validation
- [x] 6.1 Compiler builds cleanly across 10+ rebuilds during phase 1k iteration
- [x] 6.2 `benchmarks/profile_tml/string_bench.tml` — `Str += "ab"` at **26-27 ns/op** (was 462 ns at phase 1i start, 3,044 ns at the original baseline — **112x speedup**)
- [x] 6.3 `core_str_str_methods` regression: 2/2 pass (fresh, no-cache)
- [x] 6.4 Int to String: 41-42 ns (was 37-41 ns) — slight overhead from the 24-byte header allocation, compensated by O(1) `Str.len()` on the result
- [x] 6.5 Text benchmark stable at 4 ns/op; `std/text` pre-existing `text_search_transform` K001 unchanged
- [x] 6.6 `Str` passed to `@extern("c")` C functions still works — the 24-byte prefix precedes `data`, NUL at `data[len]` preserved
- [x] 6.7 TML vs Rust IR compared — remaining 14x gap (26 ns vs 2 ns) is the structural `Str = ptr` vs `String = {ptr,len,cap}` difference, documented in 7.1

## 7. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 7.1 `docs/analysis/string/README.md` updated — F-004 resolved to "phase 1k length-prefix, 26 ns/op, 13x vs Rust"; Phase 1k section documents the 24-byte header layout and magic sentinel rationale
- [x] 7.2 Regression coverage via `benchmarks/profile_tml/string_bench.tml` exercises: empty-literal init (fresh alloc path), literal→heap promotion, heap→heap concat (in-place + grow), `tml_str_free` on literal, `tml_str_free` on prefixed (magic-gated -24). `bench_int_to_str` / `bench_log_naive` now exercise the stdlib migration.
- [x] 7.3 Three consecutive bench runs pass (exit 0) with stable timings; `core_str_str_methods` 2/2; VERSION remains at 0.3.36
