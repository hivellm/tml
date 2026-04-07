# Proposal: Close Type Checker Invariant Audit Gaps

## Why

The `phase12c` audit produced `docs/specs/typechecker-invariants.md` covering 38 type checker source files with 176 invariants. Its Appendix C documents 6 gaps — areas that were not covered because they fall outside the strict "type checker" boundary but are essential for the self-hosting contract to be complete.

Without closing these gaps, a TML-written type checker has no written specification for how its TypeEnv output will be consumed by the HIR builder, THIR lowerer, borrow checker, and legacy codegen. The self-hosting contract in Section 6 is incomplete until the consumer-side invariants are documented.

## What Changes

One additional audit pass over the 6 gap areas, producing a new section (or appendix) in `docs/specs/typechecker-invariants.md`.

**Gap 1 — HIR builder TypeEnv assumptions**: Audit `compiler/src/hir/hir_builder.cpp`, `hir_builder_expr.cpp`, `hir_builder_stmt.cpp`. Document every TypeEnv field read and what the HIR builder assumes about its state. Produce an "HIR-consumer contract" subsection.

**Gap 2 — THIR lowerer TypeEnv interactions**: Audit `compiler/src/thir/thir_lower.cpp`. Document method resolution, operator desugaring, and coercion insertion — each reads TypeEnv in specific ways.

**Gap 3 — Borrow checker TypeEnv usage**: Audit `compiler/src/borrow/checker.cpp`. Document type ownership, reference validity, and lifetime checks that read TypeEnv.

**Gap 4 — Codegen TypeEnv re-use**: Audit which parts of `compiler/src/codegen/llvm/` and `compiler/src/codegen/mir/` re-run module loading or type resolution during codegen. Document the exact fields consumed.

**Gap 5 — when arm type mismatch exact behavior**: Write a targeted test and verify `check_when`'s behavior against CC-15. Update the invariant doc if the claim turns out to be wrong.

**Gap 6 — @derive method signatures**: Enumerate the exact method signatures (parameter types, return types) produced by each supported @derive target (Display, Debug, Clone, Eq, Ord, Hash, PartialEq, PartialOrd, Default). A self-hosted @derive must produce identical signatures for parity.

## Impact

- **Affected specs**: `docs/specs/typechecker-invariants.md` (new Section 7 or expanded Section 6 + updated Appendix C)
- **Affected code**: None — read-only audit
- **Breaking change**: NO
- **User benefit**: Completes the self-hosting contract. Removes the last blocker before Phase 2 of self-hosting (TML-written type checker) can begin.
