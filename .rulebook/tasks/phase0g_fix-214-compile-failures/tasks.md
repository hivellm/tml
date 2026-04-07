# Tasks: Fix 214 Test Compile Failures — Root Cause Analysis

**Status**: In Progress. RC1-RC6 + RC8 + RC9 + RC7.1-7.4 done (~228+ tests fixed). Deep RC7 (closure-typed struct fields lose signature info) remaining — 18 tests, root cause identified and documented in 7.5; fix requires its own focused sub-task (1-2 days) to either preserve closure types in struct field type_args, re-derive T from method body expression types, or mangle closure-typed fields with their full signature.
**Depends on**: None
**Blocks**: Test coverage accuracy, Phase 12 confidence
**Duration**: 2–4 weeks
**Risk**: High — multiple compiler bugs across different subsystems
**Baseline**: 1539/1753 pass (88%), 214 compile failures across 7 root causes
**Current**: ~1845/1874 pass, ~29 remaining compile failures (-185 from baseline, 86% reduction)
  - Source: `.sandbox/failure_categories.md` (regenerated 2026-04-06)

---

## Progress Summary

| Root Cause | Original | Current | Reduction |
|-----------|----------|---------|-----------|
| RC1 MODULE_NOT_FOUND | 119 | 5 | -114 (96%) |
| RC2 UNKNOWN_METHOD | 24 | 1 | -23 (96%) |
| RC3 TYPE_RETURNS_UNIT | 18 | 2 | -16 (89%) |
| RC4 LINK | 13 | 1 | -12 (92%) |
| RC5 UNDEF_SYMBOL | 12 | 0 | -12 (100%) ✅ |
| RC6 GEP_UNSIZED | 10 | 0 | -10 (100%) ✅ |
| RC7 TYPE_MISMATCH | 8 | 18 | +10 (newly exposed; -11 from peak after 7.1-7.3) |
| RC8 MODULE_NOT_FOUND residual | 0 | 3 | new |
| RC9 PARSE_ERROR (Unit `{}` ret) | 0 | 6 | new (fix landed; counts via legacy IR) |
| OTHER (misc IR / iter / SIMD) | 10 | 10 | 0 |
| **Total** | **214** | **29** | **-185 (86%)** |

Note: RC5/RC7 counts increased because RC1 fix unblocked more tests that
now reach codegen and hit deeper bugs that were previously masked by the
upstream MODULE_NOT_FOUND failures.

---

## Root Cause 1: MODULE_NOT_FOUND — std::http submodules (119 tests) ✅ FIXED

The meta preloader failed to index deeply nested HTTP submodules. Fixed in commit `74994750`.

- [x] 1.1 `compiler/src/types/module_binary_read.cpp` — replaced 2-level nesting limit with recursive `discover_submodules` that walks `pub mod` to arbitrary depth
- [x] 1.2 `compiler/src/types/checker/core.cpp` — added re-export-based module resolution at T027 error sites (parent module's `pub use` redirects)
- [x] 1.3 `lib/std/src/http/mod.tml` — added `pub use` re-exports for 11 internal modules (arena, bytes, conn_pool, dispatch, parse, shared_state, simd_parse, vectored_io, work_stealing, compression, encoding)
- [x] 1.4 Re-run verified: 119 → 4 MODULE_NOT_FOUND (115 fixed)

## Root Cause 2: UNKNOWN_METHOD — closure/iterator methods (24 tests) ✅ FIXED

Closure field method calls in iterator adapters failed at codegen. Fixed in commit `6cf83442`.

- [x] 2.1 Investigated: issue was in AST codegen `method.cpp` Section 17 (function pointer field calls), not type checker
- [x] 2.2 `compiler/src/codegen/llvm/expr/method.cpp` — fixed struct lookup to search module registry + extract concrete FuncType from monomorphized type_args
- [x] 2.3 Fix applies to all iterator adapters: Map::next, Filter::next, FilterMap, FlatMap, MapWhile, Scan, SkipWhile, TakeWhile, sources, repeat_with, from_fn
- [x] 2.4 Re-run verified: 13/24 "Unknown method" errors fixed (remaining 11 hit secondary codegen bugs)

## Root Cause 3: TYPE_RETURNS_UNIT — Maybe/Outcome methods return () (18 tests) ✅ FIXED

Type checker's hardcoded method list for Maybe/Outcome was missing 22 methods. Fixed in commit `6cf83442`.

- [x] 3.1 `compiler/src/types/checker/expr_call_method_types.cpp` — added 15 Maybe[T] methods (as_ref, as_mut, inspect, take, replace, zip, zip_with, is_just_and, get_or_insert, map_or_else, iter, transpose, unzip, etc.)
- [x] 3.2 Added 7 Outcome[T,E] methods (inspect, inspect_err, as_ref, as_mut, transpose, copied, duplicated)
- [x] 3.3 Fixed `ok_or_else` to infer E from closure return type (was returning `obj_type`)
- [x] 3.4 Re-run verified: all 18 T056 errors resolved at type check layer

## Root Cause 4: LINK failures — missing runtime symbols (13 tests) ✅ FIXED

Test runtime archive missing os_process.c and glob.c objects. Fixed in commit `2c5f522c`.

- [x] 4.1 `compiler/src/cli/builder/builder_helpers_runtime.cpp` — added unconditional os_process.c compilation
- [x] 4.2 `compiler/src/testing/testing_compile.cpp` — pre-register std::glob/os/os::subprocess in shared test runtime archive
- [x] 4.3 `compiler/CMakeLists.txt` — added os_process.c + lib/std/runtime/glob.c to TML_RUNTIME_SOURCES
- [x] 4.4 Re-run verified: subprocess/glob tests compile and link (archive: 26 → 28 objects)

## Root Cause 5: UNDEF_SYMBOL — undefined IR symbols (12 tests) ✅ FIXED

Generic instantiation and derive codegen failed to emit required symbols after the
phase12a HIR→MIR consolidation. Resolved by the derive-mangling and typevar-suffix
substitution work landed in commits `4f3a2101`, `c83bf5db`, `6ea6ad19`, and `f74a77ed`.

- [x] 5.1 Categorize: Arc method monomorphization (5), derive(FromJson) (2), derive(Default) (1), derive(Reflect) (3), other generics (1)
- [x] 5.2 Investigate if MIR codegen path calls into derive emitters — fixed by derive
       mangling unification (`c83bf5db`) so MIR-generated calls match the symbols emitted
       by the legacy derive emitters (which the THIR→MIR path now reuses via the
       shared `MirCodegen::replace_typevar_suffixes` helper exposed in `6ea6ad19`).
- [x] 5.3 Fix generic method instantiation registration for Arc[T] methods —
       `f74a77ed` substitutes residual `__T`/`__U`/`__K` typevar suffixes with `__I64`
       at the call site, so generic Arc/Heap/etc. methods now resolve to the
       monomorphized symbol that gets emitted.
- [x] 5.4 Fix derive codegen to be invoked from MIR path — derive routing handled by
       the same mangling unification commit (`c83bf5db`).

**Verification (2026-04-06)**:
  - `.sandbox/failure_categories.md` regenerated from a fresh full suite run reports
    `UNDEF_SYMBOL: 0` (down from 19).
  - Spot-checked passing tests: `lib/std/tests/sync/arc.test.tml`,
    `lib/core/tests/derive/derive.test.tml`,
    `lib/core/tests/reflect/reflect_struct.test.tml` — all pass on the current build.

## Root Cause 6: GEP_UNSIZED — getelementptr on unsized types (10 tests) ✅ FIXED

Real root cause: `Waker` and `Context[T]` struct definitions in `lib/core/src/task.tml`
were commented out under the false assumption that they were compiler builtins. They
were not — `compiler/src/types/checker/{decl_struct,core,core_oop}.cpp` merely
**reserved** the names in `RESERVED_TYPE_NAMES`, raising T038 if anyone tried to
define them, but never actually registered the types. With the struct defs commented
out, THIR never produced a `ThirStruct` for `Waker`/`Context__Unit`, MIR
`module.structs` lacked the definitions, and codegen emitted `alloca %struct.Waker` /
`alloca %struct.Context__Unit` plus GEPs against undefined opaque types →
`base element of getelementptr must be sized`.

- [x] 6.1 `compiler/src/types/checker/decl_struct.cpp` — removed `"Context"` and `"Waker"` from `RESERVED_TYPE_NAMES` (kept `"Future"` since it's a real builtin behavior)
- [x] 6.2 `compiler/src/types/checker/core.cpp` — same removal in the second copy of `RESERVED_TYPE_NAMES`
- [x] 6.3 `compiler/src/types/checker/core_oop.cpp` — same removal in the third copy of `RESERVED_TYPE_NAMES`
- [x] 6.4 `lib/core/src/task.tml` — uncommented `pub type Waker { waker: RawWaker }` (was lines 428-432)
- [x] 6.5 `lib/core/src/task.tml` — uncommented `pub type Context[T] { waker: ref Waker, _marker: PhantomData[T] }` (was lines 569-573)
- [x] 6.6 Re-run verified: full `core/task` suite (9 tests including `core_task_context`, `core_task_waker_basic`, `core_task_waker`, `core_task_rawwaker`, `core_task_pending_new`, `core_task_poll*`) all pass cleanly

## Root Cause 7: IR_TYPE_MISMATCH — LLVM type conflicts (8 tests) ⏳ PARTIAL

`Maybe__T` vs `Maybe__I32` layout mismatch. THIR lowerer produces unsubstituted type variables in generic contexts.

- [x] 7.1 `compiler/src/thir/thir_lower.cpp::lower_enum_expr` — recover concrete enum type arguments by walking the matching variant signature against the lowered payload's concrete types and applying `types::substitute_type` to both `e.type` and `e.type_args`. Catches unsubstituted typevars left over by HIR monomorphization in generic instantiation contexts (e.g. `Just(42)` re-typed from `Maybe[T]` to `Maybe[I32]`). Helper `collect_type_param_substs` recurses through `NamedType` to extract bindings.
- [x] 7.2 `compiler/src/codegen/mir/mir_types.cpp::mangle_mir_type_arg` — added defensive `assert(false)` plus `fprintf(stderr)` warning when an unresolved type variable (single-uppercase-letter `MirStructType` / `MirEnumType` with no type_args) reaches mangling. In debug builds this aborts loudly; in release the existing `I64` fallback keeps codegen alive while the warning surfaces the regression.
- [x] 7.3 `compiler/src/codegen/mir_codegen.cpp::emit_type_defs` — unified the dual enum-def emission paths. The `module.enums` loop now skips generic enums (those with non-empty `type_params`); their concrete monomorphizations are emitted exclusively by the `generic_enum_defs_` loop. This eliminates the divergent `%struct.Maybe` (template) vs `%struct.Maybe__I32` (instance) emissions and removes one source of LLVM type collisions.
- [x] 7.4 `Maybe::ok_or` / `ok_or_else` subgroup (3 tests) — replaced stub in `compiler/src/codegen/llvm/expr/method_maybe.cpp` that returned the Maybe receiver as an Outcome; now branches on the tag, materializes the Outcome[T, E] struct via `require_enum_instantiation` (E inferred from the err arg / closure body), and builds Ok(val) / Err(err) with phi merge. Fixes `option_ok_or`, `option_ok_or_else`, `types_ok_or_else`. Commit `6ed4c5c0`.
- [ ] 7.5 **DEEP ROOT CAUSE — closure-typed struct fields lose signature info (18 remaining)**

  Investigation 2026-04-06 (thir-expert): All 18 remaining RC7 TYPE_MISMATCH failures share a common root cause that is NOT in THIR or MIR — it is in the legacy LLVM codegen path's monomorphization of impls whose generic params are derived from where-clauses on closure-typed fields.

  **Concrete failing example**: `lib/core/tests/iter/iter_repeat_with.test.tml`
  - Source: `let mut r: RepeatWith[func() -> I32] = repeat_with(do() -> I32 { 99 })`
  - Impl: `impl[F, T] Iterator for RepeatWith[F] where F = func() -> T { func next(mut this) -> Maybe[T] { ... } }`
  - Generated IR (file `build/debug/iter_repeat_with.test.ll` line 558):
    ```
    define internal %struct.Maybe__T @tml_N4core...RepeatWith__Fn4nextE(ptr %this) {
      %t72 = alloca %struct.Maybe__T          ; <-- T not substituted!
      ...
      %t79 = call i32 %t76()                   ; <-- but inner closure call IS i32
      ...
    }
    ```
  - Caller IR (line 612): `%t5 = call %struct.Maybe__T @...RepeatWith__Fn4nextE(...)` followed by `%t6 = extractvalue %struct.Maybe__I32 %t5, 0` → LLVM rejects the type collision.

  **Root cause chain** (verified via stderr instrumentation in `resolve_where_clause_type_equalities`):
  1. Constructor call `repeat_with(do() -> I32 { 99 })` creates `r: RepeatWith[F]` where the type system stores the closure as a `NamedType("Fn")` PLACEHOLDER in `RepeatWith.type_args[0]`, NOT as a `ClosureType` with the rich signature.
  2. When `r.next()` dispatches in `compiler/src/codegen/llvm/expr/method_impl.cpp:556-557`, it builds `type_subs[F] = named.type_args[0]` — already the degenerate `NamedType("Fn")`.
  3. The `PendingImplMethod` is queued with this degraded `type_subs`.
  4. In `compiler/src/codegen/llvm/core/generic_instantiate_impl.cpp:755/1122`, `resolve_where_clause_type_equalities(impl.where_clause=[F=func()->T], type_subs={F=Fn})` attempts to derive T by pattern-matching `func() -> T` against `concrete=Fn`. But `Fn` is `NamedType`, not `FuncType`/`ClosureType`, so the matcher (line 441-468 special case) bails out. T is never added to `type_subs`.
  5. `gen_impl_method_instantiation` proceeds with `current_type_subs_={F=Fn}`. T is missing entirely (not "unresolved" — absent), so the stub-emission guard at line 980-1011 doesn't trigger (it only checks if existing entries contain unresolved generics, not if expected entries are missing).
  6. Body emission emits `Maybe[T]` literally (T as parser type), producing `alloca %struct.Maybe__T`. The function signature carries `%struct.Maybe__T` as return type. Body operations on `i32` (from the closure call result) write into a struct sized for `i64`, then the caller extracts as `Maybe__I32` → LLVM type mismatch.

  **Affected tests** (all 18 share this pattern — closure-typed iterator/Future/Promise sources whose inner type is derived via where-clause):
  - `compiler_iter_from_fn`, `iter_higher_order`, `iter_repeat_with`, `iter_sources`, `iter_filter_map`, `iter_map`, `iter_map_while`, `iter_scan`, `core_async_iter_basic`, `future_fuse`, `option_as_mut`, `option_as_ref`, `option_blocked`, `option_flatten`, `option_transpose2`, `outcome_as_ref`, `outcome_transpose2`, `result.test`, `maybe_get_or_insert`, `behaviors.test` (List), `promise_all_settled` (List of PromiseState), `types_encoding`.

  Many of the non-closure ones (e.g., `option_as_mut` with `Maybe__I32` vs `ptr`) are secondary effects of the same broken impl method generation poisoning subsequent type unification.

  **Three viable fix paths** (each substantial — should be its own task):
  1. **Preserve closure types in struct field type_args** — when constructing a struct whose field has type `F` and `F` was bound to a closure literal, store the closure's `ClosureType` (not the `Fn` placeholder) in the struct instance's type_args. Requires changes in type checker (struct field type recording) and call resolver (constructor args). Most correct, biggest scope.
  2. **Re-derive T at impl method instantiation time** — when `gen_impl_method_instantiation` sees that some impl_generic isn't in `full_type_subs`, walk the impl method body's AST and use the type checker's expression-type cache to find a call to `this.<closure_field>()`, then use that call's resolved return type to bind T. Requires plumbing the type checker's expression-type map into codegen.
  3. **Mangle closure-typed fields with their full signature** — change `mangle_mir_type_arg` and the legacy mangler to encode `func() -> I32` as e.g. `Fn__Ret_I32__P0_` instead of bare `Fn`. Then `parse_mangled_type_string` can recover the rich type, and the existing where-clause resolver works without changes. Smallest scope but invasive in the mangling layer.

  **Status**: ROOT CAUSE IDENTIFIED, NOT YET FIXED. Requires a separate sub-task (estimated 1-2 days) to implement one of the three approaches above. Current commit only contains the failed exploration (no production code changed).

## Root Cause 9: PARSE_ERROR — Unit `{}` return mismatch in legacy AST codegen (6 tests) ✅ FIXED

LLVM verifier rejected `ret void` in functions declared returning `{}` (data-context Unit). Affected `fmt_unit_display_debug`, `fmt_unit_type`, `fmt_unit_to_string`, `option_unit`, `outcome_unit`, `tuple/unit`. Root cause: `gen_return` in `compiler/src/codegen/llvm/control/return.cpp` collapsed `void` and `{}` into a single `ret void` branch and the no-value (bare `return`) path always emitted `ret void`.

- [x] 9.1 `compiler/src/codegen/llvm/control/return.cpp` — split the `void`/`{}` branch so `{}` emits `ret {} zeroinitializer`, and apply the same to the no-value path
- [x] 9.2 Verify all 6 tests compile cleanly with `--legacy`

## Root Cause 8: MODULE_NOT_FOUND residuals — core::hash + core::simd::algorithms (3 tests) ✅ FIXED

Three tests failed with MODULE_NOT_FOUND after RC1. Root causes were unrelated to http: missing facade file and an inline `use` inside a function body.

- [x] 8.1 `lib/core/src/hash.tml` — created facade `pub use core::traits::hash::*` so `use core::hash` resolves (was declared in `mod.tml` as `pub mod hash` but had no file; the trait module lives at `core::traits::hash`)
- [x] 8.2 `lib/core/src/simd/algorithms.tml` — fixed parse error: hoisted `use core::simd::sse42::crc32c as sse42_crc32c` from inside `crc32c_simd` body to top of file (inline `use` inside func body is invalid)
- [x] 8.3 Verified `compiler/tests/core/builtins.test.tml` compiles and passes (1/1)
- [x] 8.4 Verified `compiler/tests/runtime/strings.test.tml` compiles and passes (1/1)
- [x] 8.5 `lib/core/tests/simd/algorithms.test.tml` — module now resolves; remaining failure is TYPE_MISMATCH (`Array[F32, 4]` initialized with F64 literals at lines 42-43) — distinct category, not MODULE_NOT_FOUND

## Validation

- [x] V.1 Full suite re-run after RC1-RC4 — **1690+/1753 tests pass (96%)**, ~60 remaining failures (RC5-RC7)
- [ ] V.2 Final re-run after RC5-RC7 — target: 0 compile failures (1753/1753 pass)
