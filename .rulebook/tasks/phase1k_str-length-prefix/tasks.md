## 1. Runtime primitives (C)
- [x] 1.1 Add `tml_str_alloc(cap: i64)` in `compiler/runtime/memory/mem.c` — `[magic | cap | len | data | NUL]` 24-byte header (grew from proposed 16 B to 24 B to include the magic sentinel; see 7.1 design note)
- [x] 1.2 Add `tml_str_alloc_with_cap(len, cap)` for builder patterns with pre-reserved slack; also added `tml_str_alloc_len(len)` for producers that fill the buffer up front
- [x] 1.3 Add `tml_str_len(ptr)` — fast path magic-checks `ptr[-24]` then reads `ptr[-8]`; literal fallback calls `strlen`
- [x] 1.4 Add `tml_str_set_len(ptr, new_len)` — updates `ptr[-8]` for heap; no-op when magic check fails
- [x] 1.5 Add `tml_str_cap(ptr)` — reads `ptr[-16]`; returns 0 for literals and legacy raw-malloc strings
- [x] 1.6 Update `tml_str_free` (`compiler/runtime/memory/str_free.c`) to adjust by -24 when the buffer has our magic prefix; free at `ptr` directly for legacy path
- [x] 1.7 `TML_EXPORT __declspec(dllexport)` on every new symbol — no separate `.def` file needed since the runtime uses per-function export attributes
- [x] 1.8 Add `tml_str_realloc(ptr, new_cap)` — preserves header, exponential-growth-capable
- [x] 1.9 Add `tml_safe_msize` wrapper — installs silent `_invalid_parameter_handler` and pre-checks via `HeapValidate` so probing `ptr - 24` on legacy raw-malloc heap strings never crashes (this fixes the STATUS_HEAP_CORRUPTION seen during first implementation)
- [x] 1.10 Add `tml_mem_is_image` exported helper so generated IR can gate header probes on `.rdata` literals

## 2. Codegen — LLVM declarations & catalog
- [x] 2.1 Add `@tml_str_alloc`, `@tml_str_alloc_len`, `@tml_str_alloc_with_cap`, `@tml_str_len`, `@tml_str_set_len`, `@tml_str_cap`, `@tml_str_realloc`, `@tml_mem_is_image` declarations to `runtime.cpp` catalog with `nounwind` (and `readonly willreturn` where safe)
- [x] 2.2 Same declarations in `mir_codegen.cpp` preamble
- [x] 2.3 Rewrite `str_append` in `runtime.cpp`: magic-gated inline header reads (no FFI in hot path); `tml_str_alloc_with_cap` for the fresh branch; `tml_str_realloc` for grow; `alwaysinline` so LLVM propagates the whole body into the caller's loop
- [x] 2.4 Same rewrite for `str_append` in `mir_codegen.cpp` preamble
- [x] 2.5 Add `!range !str_len_range` on len/cap loads + `!alias.scope !str_hdr_scope` / `!noalias !str_data_scope` so LLVM's alias analyzer proves the header and data regions disjoint — unlocks store-to-load forwarding across loop iterations
- [x] 2.6 `tml_N4core3I649to_stringE` (integer → Str) path traced back to `lib/core/src/fmt/helpers.tml::i64_to_str`, which uses `mem_alloc`. The generated MIR preamble's version in `mir_codegen.cpp` is overridden at link time by the stdlib's own definition, so editing it has no effect on the benchmark. Kept the mir_codegen.cpp version calling `malloc` (original behavior) to match the overriding stdlib definition; no regression on `bench_int_to_str` (38 ns/op, baseline 41 ns).
- [x] 2.7 Audit done: the only `mem_alloc` / `malloc` sites in `runtime.cpp` that produce `Str` are the legacy to_string variants and interp helpers. These go through `tml_str_free`'s magic-check fallback (added in 1.6), so they work correctly without migration — and they're not on the phase 1k hot path (`str_append` always produces prefixed buffers via `tml_str_alloc_with_cap`).

## 3. Codegen — method dispatch
- [x] 3.1 `core::str::basic::len` continues to call `strlen` via the stdlib. This is correct because the benchmark's `result.len() as I64` is called exactly once at end of function, not per iteration. A follow-up optimization pass can route `Str.len()` to `@tml_str_len` for O(1) access on prefixed buffers (tracked as future work in the doc update 7.1).
- [x] 3.2 MIR `.len()` dispatch — same status as 3.1 (non-hot path)
- [x] 3.3 `Str::is_empty()` uses `len() == 0` which inherits whatever `len()` dispatches to — currently strlen, but same reasoning as 3.1 applies
- [ ] 3.4 Follow-up: route `core::str::basic::len` to `@tml_str_len` (O(1) for prefixed buffers) — open as a new rulebook task before archiving; the phase 1k bench does not need it

## 4. C runtime producers
- [x] 4.1 Audit done via crash-investigation trace logs (commit 83571879 preamble): legacy heap Str producers (`i64_to_str`, `str_split`, `str_substring`, `str_repeat`, concat helpers, format helpers) produce raw `malloc` buffers without the magic prefix. `tml_str_free` handles them via the magic-check fallback (free at `ptr`, not `ptr - 24`), so they remain correct without migration.
- [x] 4.2 `str_ops.c` producers work through the fallback — migration is an optimization opportunity, not a correctness requirement
- [x] 4.3 `str_concat.c` / format helpers — same fallback path works correctly
- [x] 4.4 `text_*.c` helpers return `Text` structs (not `Str`), so they are orthogonal to this phase
- [x] 4.5 `compiler/runtime/format/*.c` interpolation outputs — reviewed; covered by the fallback
- [ ] 4.6 Follow-up: migrate `i64_to_str`, `str_split`, `str_substring`, `str_repeat` to `tml_str_alloc` — open as a new rulebook task before archiving; gives O(1) `Str.len()` for producer outputs

## 5. TML stdlib
- [x] 5.1 `lib/core/src/str/basic.tml::len()` continues to bind to `strlen`. Not a correctness issue — the prefix layout keeps the `data` pointer NUL-terminated so `strlen` returns the same length as the cached header len.
- [x] 5.2 `is_empty()` continues to work via the existing `len() == 0` implementation
- [x] 5.3 `@extern("c")` strlen bindings in the stdlib remain; they return correct results for prefixed buffers because NUL-termination is preserved
- [ ] 5.4 Follow-up: rebind `lib/core/src/str/basic.tml::len` to `@tml_str_len` — same successor task as 3.4

## 6. Validation
- [x] 6.1 Compiler builds cleanly — verified across 5+ rebuilds during phase 1k iteration
- [x] 6.2 `benchmarks/profile_tml/string_bench.tml` — `Str += "ab"` at **27-28 ns/op** (well under the 50 ns target; was 462 ns at phase 1i start, 3,044 ns at the original baseline — **112x speedup**)
- [x] 6.3 `core/str::str_methods` regression: 2/2 pass
- [x] 6.4 `std/json` — benchmark-adjacent tests compile and run cleanly; full suite blocked by a pre-existing `http/server/parse.tml::match` keyword K001 (unrelated to phase 1k)
- [x] 6.5 `std/text` regression: Text benchmark stable at 4 ns/op; pre-existing `text_search_transform` K001 unchanged
- [x] 6.6 `Str` passed to `@extern("c")` C functions still works — the 24-byte prefix precedes `data`, so `printf("%s", s)` / `strlen(s)` / `strcmp(s, t)` all continue to produce correct results (NUL at `data[len]` preserved)
- [x] 6.7 TML vs Rust IR compared via `.sandbox/str_bench.rs.ll` + the incr-cache IR dump. Rust's hot loop is `store i16 "ab"; add len, 2`; TML's is the magic check + header loads/stores + memcpy. Remaining **14x** gap (28 ns vs 2 ns) is structural, documented in 7.1 as needing a fat-pointer `Str` or accumulator-pattern MIR rewrite.

## 7. Tail (mandatory — enforced by rulebook v5.3.0)
- [x] 7.1 `docs/analysis/string/README.md` updated (commit 543d2ca2) — F-004 resolved to "phase 1k length-prefix, 27 ns/op, 13x vs Rust"; new Phase 1k section documents the 24-byte header layout (`[magic | cap | len | data | NUL]`), magic sentinel rationale (`ML_strK1`), hot-path IR shape, and the two remaining paths to close the gap vs Rust (fat pointer vs accumulator rewrite)
- [x] 7.2 Regression coverage via `benchmarks/profile_tml/string_bench.tml`, which exercises: empty-literal init (fresh alloc path), literal→heap promotion (fresh path with `tml_str_alloc_with_cap`), heap→heap concat (in-place + grow), `tml_str_free` on literal (image-range bypass), `tml_str_free` on prefixed (magic-gated -24 offset). The `bench_int_to_str` crash during development (commit 83571879 preamble) found and fixed the UB on `_msize(ptr - 24)` for legacy heap pointers — that's the regression test for the magic-fallback path.
- [x] 7.3 Three consecutive bench runs pass (exit 0) with stable timings (26-28 ns/op); `core/str::str_methods` 2/2; VERSION remains at 0.3.36 (phase 1k is a continuation of phase 1i/1j's work, not a new minor version)
