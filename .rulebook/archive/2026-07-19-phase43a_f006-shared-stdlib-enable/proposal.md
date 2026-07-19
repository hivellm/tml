# Proposal: phase43a_f006-shared-stdlib-enable

## Why
F-006/F-007 are the biggest remaining tooling-performance lever: every test file re-emits the entire stdlib during codegen (~5000 functions / 287 modules), which dominates the COLD compile that phase41a's aggregation and phase41c's result cache do NOT touch. The `build_stdlib_object` fast-path (emit stdlib once, test objects reference it as decls) is disabled at `compiler/src/testing/testing_compile_parallel.cpp` because enabling it regresses clean suites.

phase41b + codegen-debugger root-caused the four blockers (evidence in `.claude/agent-memory/codegen-debugger/shared-stdlib-fastpath-dedup-gap.md` and `docs/analysis/tooling-performance/04-test-framework-performance.md` F-006):
1. **Generic free-function monomorphization gap** (the GENUINE one, distinct from phase27e's collision fix): `call_user.cpp:452` fallback emits an un-monomorphized decl-mangled callee (`mem::replace[T]`) with no queued define when `gen_call_generic_func` misses under the partial bootstrap. The `Type::method` path (`call_user.cpp:595-632`) has the re-mangle+queue branch; free functions don't. Only reproducible under the fast-path.
2. **Dedup keyspace gap:** `CodegenLibraryState` captures only `generated_functions_`, not `generated_impl_methods_output_`/`generated_impl_methods_` → I32::duplicate redefinition (`generate_support.cpp:622`, restore `generate.cpp:705-707`).
3. **i64/i32 range-iterator width** (K001 Class 2): local range desugaring re-infers the index type as i32 (default) vs the cached i64 instance → width mismatch.
4. **Bootstrap:** the monolithic `test_bootstrap.tml` imports ~12 phantom `pub mod` modules with no source file → typecheck aborts; eager all-stdlib codegen times out >500s.

## What Changes
Enable the shared-stdlib fast-path via a **per-suite-scoped bootstrap** (compile only the modules the suite transitively imports — NOT eager all-stdlib), fixing the four root causes:
1. Add the free-function re-mangle+queue branch (Fix B) in `call_user.cpp` — guarded on all-type-params-concrete — OR resolve the miss upstream in `gen_call_generic_func`.
2. Capture+restore `generated_impl_methods_output_`/`generated_impl_methods_` in `CodegenLibraryState`.
3. Resolve range/iterator index width from the registered signature, not the i32 `last_expr_type_` default.
4. Scope `build_stdlib_object` to the suite's transitive module set (avoids the phantom-`pub mod` bootstrap AND the >500s eager path AND the mut-ref-broken std/http, which no core suite imports).

Then flip `library_decls_only=true` for test objects so they reference the shared stdlib instead of embedding it (F-007).

## Impact
- Affected specs: none (ADR-002/005 impl notes)
- Affected code: `compiler/src/codegen/llvm/expr/call_user.cpp`, `compiler/src/codegen/llvm/generate*.cpp` (CodegenLibraryState capture/restore), range-iter codegen, `compiler/src/testing/testing_compile_parallel.cpp` + `testing_compile.cpp` (scoped bootstrap, library_decls_only)
- Breaking change: NO (identical test results required)
- User benefit: eliminates the ~5000-function redundant stdlib codegen per test file — the cold-compile win aggregation/caching couldn't reach; per-EXE size drops from ~345 KB
