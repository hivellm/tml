# Tasks: MIR Path Consolidation — Retire Legacy HIR→MIR Builder

**Status**: Planned (0/22)
**Depends on**: None (can start immediately)
**Blocks**: phase12e (AST serializers need stable MIR), phase12f (hybrid pipeline)
**Duration**: 4–6 weeks
**Risk**: Medium

---

## Phase 1: Audit Legacy Path (5 items)

- [ ] 1.1 `compiler/src/cli/dispatcher.cpp` — List all `--legacy` / `use_legacy` code paths
- [ ] 1.2 `compiler/src/mir/hir_mir_builder.cpp` — Document all public entry points called externally
- [ ] 1.3 `compiler/src/mir/builder/hir_*.cpp` — List all legacy-only files with LOC
- [ ] 1.4 Run full test suite with THIR-only path — record all failures in `.sandbox/thir-failures.log`
- [ ] 1.5 Compare: count tests passing on legacy vs THIR — identify the delta

## Phase 2: Achieve THIR Feature Parity (6 items)

- [ ] 2.1 Categorize each THIR failure: missing feature / codegen bug / test-specific issue
- [ ] 2.2 Fix THIR MIR builder for each missing construct — one commit per construct
- [ ] 2.3 Fix any THIR codegen bugs found — one commit per bug
- [ ] 2.4 Run full suite again — verify zero delta between legacy and THIR paths
- [ ] 2.5 IR-diff 50 representative test files: legacy IR vs THIR IR — document any semantic diffs
- [ ] 2.6 Fix any IR differences found in 2.5 until output is identical

## Phase 3: Remove Legacy Path (7 items)

- [ ] 3.1 Delete `compiler/src/mir/hir_mir_builder.cpp` (818 LOC)
- [ ] 3.2 Delete `compiler/src/mir/builder/hir_expr.cpp` (1,227 LOC)
- [ ] 3.3 Delete `compiler/src/mir/builder/hir_expr_control.cpp` (836 LOC)
- [ ] 3.4 Delete `compiler/src/mir/builder/hir_stmt.cpp` and `hir_pattern.cpp`
- [ ] 3.5 Delete `compiler/include/mir/hir_mir_builder.hpp`
- [ ] 3.6 `compiler/src/cli/dispatcher.cpp` — Remove `--legacy` flag and all conditional branching
- [ ] 3.7 `compiler/CMakeLists.txt` — Remove deleted source files from build targets

## Phase 4: Verify & Cleanup (4 items)

- [ ] 4.1 Build compiler — verify clean compilation with zero warnings
- [ ] 4.2 Run full test suite (1,700+ tests) — all must pass
- [ ] 4.3 Remove dead code in `compiler/src/codegen/` exposed by legacy removal
- [ ] 4.4 Update `CLAUDE.md` architecture map — remove dual-path MIR references
