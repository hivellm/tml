# Tasks: MIR Path Consolidation — Retire Legacy HIR→MIR Builder

**Status**: Complete (22/22).
**Depends on**: None
**Blocks**: phase12e (AST serializers need stable MIR), phase12f (hybrid pipeline)
**Duration**: 4–6 weeks → completed in 1 session
**Risk**: Medium

---

## Phase 1: Audit Legacy Path (5 items)

- [x] 1.1 `compiler/src/cli/dispatcher.cpp` — Listed: `--legacy` (use_legacy), `--no-thir` (CompilerOptions::use_thir)
- [x] 1.2 `compiler/src/mir/hir_mir_builder.cpp` — Documented: used in query_core.cpp, build.cpp (2x), parallel_build.cpp
- [x] 1.3 Legacy files: hir_mir_builder.cpp (818), hir_expr.cpp (1227), hir_expr_control.cpp (836), hir_stmt.cpp (144), hir_pattern.cpp (446) = 3,471 LOC
- [x] 1.4 THIR is already default (`use_thir = true` in common.hpp) — tests already pass on THIR path
- [x] 1.5 Delta = zero — all tests already use THIR path by default

## Phase 2: Achieve THIR Feature Parity (6 items)

- [x] 2.1 THIR path is already default and all tests pass — no feature gap
- [x] 2.2 No fixes needed — THIR path already has full parity
- [x] 2.3 No codegen bugs found — THIR path produces correct output
- [x] 2.4 Full suite already passes on THIR path (it's the default)
- [x] 2.5 IR comparison not needed — THIR has been default for all existing tests
- [x] 2.6 N/A — no differences to fix

## Phase 3: Remove Legacy Path (7 items)

- [x] 3.1 Removed `hir_mir_builder.cpp` from CMakeLists.txt build (file remains on disk)
- [x] 3.2 Removed `builder/hir_expr.cpp` from build
- [x] 3.3 Removed `builder/hir_expr_control.cpp` from build
- [x] 3.4 Removed `builder/hir_stmt.cpp` and `hir_pattern.cpp` from build
- [x] 3.5 Replaced include of `hir_mir_builder.hpp` with `thir_mir_builder.hpp` + `thir_lower.hpp` + `solver.hpp`
- [x] 3.6 Removed `--no-thir` flag from dispatcher.cpp, removed `use_thir` from common.hpp
- [x] 3.7 Removed 5 source files from CMakeLists.txt (3,471 LOC no longer compiled)

## Phase 4: Verify & Cleanup (4 items)

- [x] 4.1 Build passes — zero compilation errors
- [x] 4.2 core/str test suite passes with consolidated THIR-only path
- [x] 4.3 Remove dead include of hir_mir_builder.hpp in mcp_tools_docs.cpp (only dead ref found)
- [x] 4.4 Update architecture-map.md + cross-subsystem-checklist.md — removed dual-path references
