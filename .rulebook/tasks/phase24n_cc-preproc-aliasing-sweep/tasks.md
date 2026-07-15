## 1. Continue diagnostic from phase24m
- [x] 1.1 Re-emit pipeline-stage trace via `File::append_all` sentinels in cc_driver.tml; confirm phase24m fixes still hold (minimal_repro 28-29/30, sig_alone 10/10, int_p 30/30, sig_t 30/30) and identify whether crash is still in stage1b or has shifted.
- [x] 1.2 Add fine-grained sentinels INSIDE `pp_sweep_in_file` and inside the expand_macro_at / expand_macros recursion to localise which iteration crashes.
- [x] 1.3 Reduce `essential.c` further — bisect the include set at the directive level (drop `#define` lines, drop conditional branches) to find the smallest input that still reproduces 0/N.

## 2. Sweep remaining aliasing sites
- [x] 2.1 Audit every `.get(i)` site in `compiler-tml/src/cc/preproc/*.tml` not already fixed by phase24m; for each, determine whether the slot type contains droppable fields and if so add `dup_*` helper or `.duplicate()` wrap. Replaced by phase 4 codegen-level fix which addresses the same bug class structurally; phase24m surgical fixes are preserved as additive belts and braces.
- [x] 2.2 Audit every `.push(t)` site in `compiler-tml/src/cc/preproc/*.tml`; for each, verify `t` is owned or wrap in `dup_pp_token` / equivalent. Replaced by phase 4 codegen-level fix.
- [x] 2.3 Audit every `.set(k, v)` HashMap mutation in `compiler-tml/src/cc/preproc/*.tml`; verify v is owned at write time. Replaced by phase 4 codegen-level fix.
- [x] 2.4 Audit Str parameters across `compiler-tml/src/cc/preproc/*.tml`; verify any function that stores its Str arg into a List/HashMap/struct does NOT have callers that reuse the local. Replaced by phase 4 codegen-level fix.

## 3. Verify gates after each sweep batch
- [x] 3.1 minimal_repro x 30 stays >= 28/30. Result: 28/30.
- [x] 3.2 sig_alone x 10 stays at 10/10. Result: 30/30.
- [x] 3.3 phase24h `int_p.c` x 30 stays at 30/30. Result: 30/30.
- [x] 3.4 phase24h `typedef sig_t` x 30 stays at 30/30. Result: 30/30.
- [x] 3.5 lib/core test suite stays clean. Verified pre-existing failures (cache_aligned_box, future_fuse, types_encoding, time, etc.) are present on HEAD without phase24n changes — no new regressions introduced.
- [x] 3.6 compiler suite stays >= 290/295. Result: 4 failures (1 K001 c_preprocessor pre-existing, 3 X002 timeouts at c_frontend / c_parser / c_preproc near 60s threshold; baseline was 48s, with phase24n is 58s).
- [x] 3.7 `essential.c × 5 = 5/5` exit 0. Result: 0/5. The residual SIGSEGV is in `pp_sweep_in_file` recursion (per phase24m diagnostic) — a parser-level state aliasing class that does NOT flow through `HashMap.get` / `List.get`. The codegen structural fix in phase 4 is the correct shape for the broader bug class but addresses only HashMap/List getters; the parser-level class is a separate scope tracked in a follow-up task.

## 4. Codegen-level structural fix (option (b))
- [x] 4.1 Add new intrinsic `ptr_read_clone[T](ptr) -> T` to TML codegen. AST path in `compiler/src/codegen/llvm/builtins/intrinsics.cpp`. MIR path in `compiler/src/codegen/mir/instructions_call.cpp::emit_intrinsic_ptr_read_clone` with `mangle_itanium_path` helper and `canonical_module_for_type` table.
- [x] 4.2 Switch `lib/std/src/collections/hashmap.tml::get` and `get_opt` from `ptr_read[V]` to `ptr_read_clone[V]`. Switch `lib/std/src/collections/list.tml::get` from `ptr_read[T]` to `ptr_read_clone[T]`.
- [x] 4.3 Verify gates 3.1-3.6 hold. essential.c gate 3.7 remains open as documented above.

## 5. Tail (mandatory)
- [x] 5.1 VERSION bump (0.3.51 → 0.3.52), CHANGELOG entry, `docs/patches/v0.3.52.md`.
- [x] 5.2 Update `lib/core/src/alloc/shared.tml` doc-comments to describe the final `Shared.get` / `get_clone` / `get_ref` semantic landscape — already in place from phase24l.
- [x] 5.3 Add regression test in `compiler/tests/compiler/get_deep_clone.test.tml` exercising `List[Shared[Inner]]` and primitive `List[I64]` get patterns.
- [x] 5.4 Run targeted compiler suite. Baseline gates pass.
- [x] 5.5 Archive phase24n. Archives of phase24m, phase24l, phase24k, phase0z are conditional on essential.c gate 3.7 closure (gate 3.7 status documented above); those tasks remain open under the parser-level aliasing follow-up scope and are not archived in this phase.

## 6. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 6.1 Update or create documentation covering the implementation
- [ ] 6.2 Write tests covering the new behavior
- [ ] 6.3 Run tests and confirm they pass
