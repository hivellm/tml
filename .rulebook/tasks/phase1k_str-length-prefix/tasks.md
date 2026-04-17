## 1. Runtime primitives (C)
- [ ] 1.1 Add `tml_str_alloc(len: i64)` in `compiler/runtime/memory/mem.c` — `[cap:i64 | len:i64 | data | NUL]` layout; returns pointer to `data`
- [ ] 1.2 Add `tml_str_alloc_with_cap(len: i64, cap: i64)` variant for builder patterns with pre-reserved slack
- [ ] 1.3 Add `tml_str_len(ptr)` — fast path reads `ptr[-8]`; literal fallback calls `strlen`
- [ ] 1.4 Add `tml_str_set_len(ptr, new_len)` — updates `ptr[-8]` for heap; no-op for literals
- [ ] 1.5 Add `tml_str_cap(ptr)` — reads `ptr[-16]`; returns 0 for literals
- [ ] 1.6 Update `tml_str_free` (`compiler/runtime/text/str_free.c`) to adjust by -16 before `mem_free`; preserve literal detection
- [ ] 1.7 Export all new symbols from `compiler/runtime/text/exports.def` and `compiler/runtime/memory/exports.def`

## 2. Codegen — LLVM declarations & catalog
- [ ] 2.1 Add `@tml_str_alloc`, `@tml_str_alloc_with_cap`, `@tml_str_len`, `@tml_str_set_len`, `@tml_str_cap` declarations to `runtime.cpp` catalog with `nounwind` attrs
- [ ] 2.2 Add same declarations to `mir_codegen.cpp` preamble
- [ ] 2.3 Rewrite `str_append` in `runtime.cpp`: replace `strlen(a)` with `tml_str_len(a)`; replace `mem_alloc` fresh path with `tml_str_alloc`; call `tml_str_set_len` on exit to sync header
- [ ] 2.4 Same rewrite for `str_append` in `mir_codegen.cpp` preamble
- [ ] 2.5 Rewrite `str_concat_reuse` and `str_concat_opt` to use `tml_str_len` + `tml_str_alloc`
- [ ] 2.6 Update `tml_N4core3I649to_stringE`, `tml_N4core3I329to_stringE` and all other inline `to_string` definitions in `runtime.cpp` to use `tml_str_alloc`
- [ ] 2.7 Audit `runtime.cpp` for every `call ptr @mem_alloc` that produces a `Str` and replace with `call ptr @tml_str_alloc`

## 3. Codegen — method dispatch
- [ ] 3.1 In `runtime_modules_library.cpp`, emit `@tml_str_len` catalog entry for `core::str::basic::len` and `Str::len` dispatch
- [ ] 3.2 Verify MIR path also routes `Str.len()` to `tml_str_len` (check `thir_mir_builder_expr.cpp` method dispatch)
- [ ] 3.3 Ensure `Str::is_empty()` uses `tml_str_len(s) == 0` instead of `strlen(s) == 0`

## 4. C runtime producers
- [ ] 4.1 Audit `compiler/runtime/text/*.c` for `mem_alloc`/`malloc` sites that return `Str` — list them in `.sandbox/str_producers.log`
- [ ] 4.2 Migrate `str_ops.c` producers (`str_split`, `str_substring`, `str_repeat`, etc.) to `tml_str_alloc`
- [ ] 4.3 Migrate `str_concat.c` / format helpers
- [ ] 4.4 Migrate `text_*.c` helpers that return `Str` (not `Text`)
- [ ] 4.5 Audit `compiler/runtime/format/*.c` — interpolation / formatting outputs

## 5. TML stdlib
- [ ] 5.1 Update `lib/core/src/str/basic.tml` — `len()` binds to `tml_str_len` via `@extern("c")`
- [ ] 5.2 Update `lib/core/src/str/basic.tml` — `is_empty()` uses `len() == 0`
- [ ] 5.3 Verify no `@extern("c")` bindings in stdlib directly call `strlen` on a `Str` — replace with `tml_str_len`

## 6. Validation
- [ ] 6.1 Build compiler; verify no symbol resolution errors
- [ ] 6.2 Run `benchmarks/profile_tml/string_bench.tml` — confirm `Str += "ab"` drops below 50 ns/op (from 462 ns/op baseline)
- [ ] 6.3 Run `core/str` test suite — all 32 tests pass
- [ ] 6.4 Run `std/json` test suite — all 23 tests pass
- [ ] 6.5 Run `std/text` test suite — 5/6 pass (pre-existing `text_search_transform` K001 unchanged)
- [ ] 6.6 Verify `Str` passed to `@extern("c")` C functions still works (e.g., `printf("%s", s)`) — NUL-termination preserved
- [ ] 6.7 Compare TML vs Rust IR for the Str concat loop — confirm `strlen` is gone from hot path

## 7. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 7.1 Update `docs/analysis/string/README.md` — F-004 finding resolved; new benchmark numbers; design doc for the length-prefix layout
- [ ] 7.2 Write regression tests: `compiler/tests/compiler/str_length_prefix.test.tml` covering empty-literal, literal→heap promotion, heap→heap concat, `tml_str_free` on literal, cross-FFI NUL safety
- [ ] 7.3 Run all tests and confirm they pass; bump VERSION and CHANGELOG
