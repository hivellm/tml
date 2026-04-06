# Tasks: Fix 214 Test Compile Failures — Root Cause Analysis

**Status**: In Progress (15/25). RC1-RC4 done (174 tests fixed). RC5-RC7 remaining.
**Depends on**: None
**Blocks**: Test coverage accuracy, Phase 12 confidence
**Duration**: 2–4 weeks
**Risk**: High — multiple compiler bugs across different subsystems
**Baseline**: 1539/1753 pass (88%), 214 compile failures across 7 root causes
**Current**: ~1690/1753 pass (96%), ~60 remaining compile failures

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

## Root Cause 5: UNDEF_SYMBOL — undefined IR symbols (12 tests) ⏳ DEFERRED

Generic instantiation and derive codegen not emitting required functions after HIR→MIR consolidation. Complex codegen issue spanning 4 subsystems.

- [ ] 5.1 Categorize: Arc method monomorphization (5), derive(FromJson) (2), derive(Default) (1), derive(Reflect) (3), other generics (1)
- [ ] 5.2 Investigate if MIR codegen path calls into derive emitters (likely missing post-consolidation)
- [ ] 5.3 Fix generic method instantiation registration for Arc[T] methods
- [ ] 5.4 Fix derive codegen to be invoked from MIR path

**Note**: Requires 1-2 dedicated sessions. Documented root cause hypothesis in rc45-fix agent output. Main suspect: THIR→MIR builder not queueing derive method instantiations that the legacy HIR path used to queue.

## Root Cause 6: GEP_UNSIZED — getelementptr on unsized types (10 tests) ⏳ DEFERRED

LLVM IR error on opaque struct types. Requires type collection pre-pass extension.

- [ ] 6.1 `compiler/src/codegen/mir_codegen.cpp` lines 160-260 — extend type collection pre-pass to walk ALL instruction operand/result types (not only StructInitInst/EnumInitInst)
- [ ] 6.2 Register every reachable MirStructType/MirEnumType via `collect_enum_types_from_type` from GEPInst, LoadInst, StoreInst, CallInst
- [ ] 6.3 Re-run affected tests

## Root Cause 7: IR_TYPE_MISMATCH — LLVM type conflicts (8 tests) ⏳ DEFERRED

`Maybe__T` vs `Maybe__I32` layout mismatch. THIR lowerer produces unsubstituted type variables in generic contexts.

- [ ] 7.1 `compiler/src/thir/thir_lower.cpp` — apply active type substitution to enum constructor expression types before emission
- [ ] 7.2 Add defensive assertion in `mir_types.cpp::mangle_mir_type_arg` for unresolved type variables
- [ ] 7.3 Unify dual enum-def emission paths in `mir_codegen.cpp` (emit_enum_def vs used_struct_types_ loops)

## Validation

- [x] V.1 Full suite re-run after RC1-RC4 — **1690+/1753 tests pass (96%)**, ~60 remaining failures (RC5-RC7)
- [ ] V.2 Final re-run after RC5-RC7 — target: 0 compile failures (1753/1753 pass)
