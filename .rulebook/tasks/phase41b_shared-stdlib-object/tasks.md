## 1. Implementation
- [ ] 1.1 Reproduce the historical failure: re-enable `build_stdlib_object` (`testing_compile_parallel.cpp:41-43`) on a scratch branch of the code (not committed), run a small suite, capture the exact errors (`I32::duplicate` redefinition, i64/i32 mismatches, LLD multiple-definition) with full IR/linker evidence into `.sandbox/`
- [ ] 1.2 Root-cause each failure class: where does the duplicate emission/linkage decision come from (shared codegen state vs per-file emission, internal vs external linkage of stdlib symbols, generic instantiation ownership)? Document with file:line evidence — no guessing (research-first rule)
- [ ] 1.3 Fix the root cause(s) in codegen/linkage so stdlib symbols are emitted once with external linkage in the shared object and test objects reference them as declarations (`library_decls_only=true`)
- [ ] 1.4 Re-enable the stdlib fast-path permanently; wire `compile_suite` to use `g_stdlib_codegen_state` (no `incremental=true` fallback re-emitting the stdlib)
- [ ] 1.5 F-012: hoist `LLVMBackend` construction/initialization out of `compile_ir_string_to_object` (`object_compiler.cpp:269-274`) — one initialized backend/TargetMachine per worker thread
- [ ] 1.6 Rebuild + verify correctness: representative suites (core/hash, compiler/borrow, std/json, core/str) produce per-test results identical to before; per-EXE size drops from ~345 KB; per-file compile time drop measured
- [ ] 1.7 GATE: full non-coverage `tml test` wall-clock before/after recorded in `01-measurements.md`; zero result divergence; stdlib emitted once per run (verify via logs/IR, not assumption)

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation: `04-test-framework-performance.md` (F-006/F-007/F-012 resolved with numbers), README lever #2; CHANGELOG/VERSION bump
- [ ] 2.2 Write tests covering the new behavior (shared-stdlib link path, decls-only test objects, backend reuse)
- [ ] 2.3 Run tests and confirm they pass
