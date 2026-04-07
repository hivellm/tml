# Tasks: Close Type Checker Invariant Audit Gaps

**Status**: Planned (0/12)
**Depends on**: phase12c (archived) — provides the base document to extend
**Blocks**: phase12 Era 1 Phase 2 (TML-written type checker — needs full consumer contract)
**Duration**: 1 week (read-only audit)
**Risk**: Low — documentation only
**Deliverable**: Extended `docs/specs/typechecker-invariants.md` with 6 gap sections closed

---

## Gap 1 — HIR Builder TypeEnv Consumption

- [ ] G1.1 Read `compiler/src/hir/hir_builder.cpp` and `hir_builder_expr.cpp`, `hir_builder_stmt.cpp`.
- [ ] G1.2 Enumerate every TypeEnv field read. Document the assumed state of each at the time of read.
- [ ] G1.3 Write "HIR-Consumer Contract" subsection for `docs/specs/typechecker-invariants.md`.

## Gap 2 — THIR Lowerer TypeEnv Interactions

- [ ] G2.1 Read `compiler/src/thir/thir_lower.cpp`.
- [ ] G2.2 Document method resolution, operator desugaring, and coercion insertion paths and their TypeEnv reads.
- [ ] G2.3 Write "THIR-Consumer Contract" subsection.

## Gap 3 — Borrow Checker TypeEnv Usage

- [ ] G3.1 Read `compiler/src/borrow/checker.cpp`.
- [ ] G3.2 Document type ownership, reference validity, lifetime checks, and their TypeEnv dependencies.
- [ ] G3.3 Write "Borrow-Checker-Consumer Contract" subsection.

## Gap 4 — Codegen TypeEnv Re-Use

- [ ] G4.1 Grep `compiler/src/codegen/` for `TypeEnv`, `module_registry`, `lookup_struct`, `lookup_enum`, `lookup_behavior`.
- [ ] G4.2 Document every codegen site that re-runs module loading or type resolution.
- [ ] G4.3 Write "Codegen-Consumer Contract" subsection.

## Gap 5 — `when` Arm Type Mismatch Verification

- [ ] G5.1 Write a targeted test that puts mismatched types in `when` arms.
- [ ] G5.2 Run via `mcp__tml__test debug_layers=true`. Observe actual behavior.
- [ ] G5.3 Update CC-15 in `docs/specs/typechecker-invariants.md` Section 5 with verified behavior.

## Gap 6 — @derive Method Signatures

- [ ] G6.1 For each supported @derive target (Display, Debug, Clone, Eq, Ord, Hash, PartialEq, PartialOrd, Default), read `decl_struct.cpp` to find the generated method signature.
- [ ] G6.2 Produce a table: target → method name → (params, return type).
- [ ] G6.3 Add table as new subsection in `docs/specs/typechecker-invariants.md`.

## Documentation Finalization

- [ ] D.1 Update `docs/specs/typechecker-invariants.md` Appendix C: remove all 6 gaps once each is closed.
- [ ] D.2 Update Section 6 contract: add consumer-side invariants derived from Gaps 1-4.
- [ ] D.3 Commit with conventional message: `docs(specs): close phase12c typechecker invariant audit gaps (phase12e)`.
