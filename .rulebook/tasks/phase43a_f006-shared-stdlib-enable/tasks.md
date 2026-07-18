## 1. Implementation
- [ ] 1.1 Reproduce the fast-path failure on a scratch re-enable of `build_stdlib_object` (`testing_compile_parallel.cpp`), capture each error class with IR/linker evidence to `.sandbox/` (mem::replace dangling, I32::duplicate, i64/i32 range width, phantom-pub-mod bootstrap abort). Confirm the four root causes hold against current source
- [ ] 1.2 Blocker 1 — generic free-function monomorphization (Fix B): in `call_user.cpp` after `free_func_type_subs`, when `func_sig->type_params` non-empty AND all concrete, `require_func_instantiation(bare, type_args)` + override the mangled callee (mirror `Type::method` at 595-632); OR fix upstream in `gen_call_generic_func`. Verify the `mem::replace[T]` dangling ref disappears
- [ ] 1.3 Blocker 2 — dedup keyspace: capture+restore `generated_impl_methods_output_` + `generated_impl_methods_` in `CodegenLibraryState` (`generate_support.cpp:622`, `generate.cpp:705-707`); verify I32::duplicate redefinition gone
- [ ] 1.4 Blocker 3 — range-iter width: resolve the index type from the registered signature, not the i32 `last_expr_type_` default; verify no i64/i32 mismatch
- [ ] 1.5 Blocker 4 — scoped bootstrap: build the stdlib object from the suite's TRANSITIVE module set (not eager all-stdlib) — dodges the phantom-`pub mod` imports, the >500s eager path, and mut-ref-broken std/http (no core suite imports it). Cap/guard runtime
- [ ] 1.6 Enable the fast-path + `library_decls_only=true` for test objects (F-007); rebuild
- [ ] 1.7 GATE: clean suites zero divergence (core/hash 14/14, compiler/borrow 12/12, std/json 23/23, core/alloc, core/str 33/33) — the fast-path must NOT regress them; stdlib emitted ONCE per run (verify via logs/IR, not assumption); per-EXE size drops from ~345 KB; cold-compile wall-clock before→after in `01-measurements.md`; determinism-gate.sh 10 sentinels at floor
- [ ] 1.8 If full enablement cannot pass zero-divergence, land the codegen sub-fixes (1.2-1.4) that ARE clean independently (they fix K001s regardless), keep the fast-path off with an updated evidence-based comment, and document the precise residual — do NOT force a regressing enable

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation — `04-test-framework-performance.md` F-006/F-007 status, memory file, CHANGELOG/VERSION bump + patch note
- [ ] 2.2 Write tests covering the new behavior — shared-stdlib link-path + decls-only + the scoped-bootstrap; the clean suites are the acceptance set
- [ ] 2.3 Run tests and confirm they pass — 1.7 gate + determinism
