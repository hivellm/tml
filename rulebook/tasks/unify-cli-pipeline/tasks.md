# Tasks: Unify CLI Pipeline Architecture

**Status**: Planning (0%)

## Phase 1: Low-Cost Improvements (5-10% effort, 80% value)

- [ ] 1.1 Add --backend support to test command (llvm, cranelift)
- [ ] 1.2 Add --emit-pipeline support to test command
- [ ] 1.3 Add --out-dir support to test command
- [ ] 1.4 Document differences in CLI help text (tml test --help)
- [ ] 1.5 Verify Phase 1 works: tml test lib/core/tests/str/ --backend=llvm --emit-pipeline

## Phase 2: Medium-Cost Refactoring (40-50% effort, additional 15% value)

- [ ] 2.1 Refactor test compilation to use QueryContext (instead of custom pipeline)
- [ ] 2.2 Consolidate coverage reporting architecture (use build.cpp's coverage model)
- [ ] 2.3 Verify incremental compilation works with new test pipeline
- [ ] 2.4 Benchmark: compare old vs new test execution time (full suite timing)
- [ ] 2.5 Add cross-command cache validation (verify build/run/test share same cache)

## Phase 3: Hard Fixes (40-50% effort, additional 5% value)

- [ ] 3.1 Fix suite merging codegen bug (symbol deduplication in codegen layer)
- [ ] 3.2 Implement DLL runtime initialization (test entry points)
- [ ] 3.3 Remove compiler tests individual-mode workaround (delete suite_execution.cpp lines 803-811)
- [ ] 3.4 Full regression testing (run all three commands, verify no regressions)

## Phase 4: Documentation & Cleanup

- [ ] 4.1 Update compiler/src/cli/README.md with unified pipeline diagram
- [ ] 4.2 Add design rationale comments to query_context.hpp (explain command-specific layers)
- [ ] 4.3 Document architectural decisions in CLAUDE.md or design document
- [ ] 4.4 Create summary of which command optimizations exist (compile_threads, cache, etc.)
