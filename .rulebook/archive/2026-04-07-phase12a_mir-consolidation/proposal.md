# Proposal: MIR Path Consolidation — Retire Legacy HIR→MIR Builder

**Task**: phase12a_mir-consolidation
**Status**: Planned
**Priority**: P0
**Estimated effort**: 4–6 weeks
**Risk**: Medium

## Problem

The TML compiler maintains two parallel MIR builder paths: the legacy HIR→MIR path
(`hir_mir_builder.cpp` + `builder/hir_*.cpp`, ~3,471 LOC) and the newer THIR→MIR path
(`thir_mir_builder.cpp` + `thir_mir_builder_expr.cpp`). Both paths are active in production,
selected at runtime via the `--legacy` flag in `dispatcher.cpp`.

This duality creates three concrete problems. First, every bug fix and language feature addition
must be applied to both paths, or regressions appear on whichever path was omitted — this has
already happened repeatedly (see session notes on `call void` vs `call {}` unit fix applied only
to one path). Second, the legacy path operates on HIR directly, bypassing THIR's implicit
coercion insertion, method resolution via the trait solver, and operator desugaring — meaning the
two paths can silently diverge on edge cases that tests do not cover. Third, the ~3,471 LOC of
legacy builder files represent work that must be re-ported during self-hosting Phase 3. Retiring
the legacy path now eliminates that debt.

The path also blocks downstream tasks: phase12e (AST serializers) requires a stable, single MIR
representation to define the binary format, and phase12f (hybrid pipeline) cannot safely swap a
stage that has two competing implementations.

## Proposed Solution

Four-phase approach that achieves parity before any deletion occurs.

**Phase 1 — Audit**: Enumerate every `--legacy` code path in `dispatcher.cpp`. Document all
public entry points on `hir_mir_builder.cpp` that are called from outside the builder directory.
Run the full test suite (1,700+ tests) with THIR-only path active and capture all failures to
`.sandbox/thir-failures.log`. Count the exact delta between legacy and THIR pass totals.

**Phase 2 — Parity**: Categorize each THIR failure as a missing language construct, a codegen
bug, or a test-specific assumption baked into the legacy path. Implement missing constructs one
at a time — one commit per construct, never batched. Run the affected test suite after each fix
to confirm forward progress. When the full suite passes on THIR-only, run IR-diff on 50
representative test files (using the phase12d tool if available, otherwise textual diff on
normalized IR) to verify that legacy and THIR produce semantically identical LLVM IR output.
Fix any IR-level differences before proceeding to deletion.

**Phase 3 — Delete**: Remove the five legacy source files (`hir_mir_builder.cpp`,
`hir_expr.cpp`, `hir_expr_control.cpp`, `hir_stmt.cpp`, `hir_pattern.cpp`) and the header
`hir_mir_builder.hpp`. Remove the `--legacy` flag from `dispatcher.cpp` and all conditional
branching that selects the path. Update `CMakeLists.txt` to drop the deleted files from the
`target_sources` list.

**Phase 4 — Verify**: Build the compiler clean with zero warnings. Run the full 1,700+ test
suite. Remove any dead code in `compiler/src/codegen/` that was only reachable via the legacy
path. Update `CLAUDE.md`'s architecture map to remove all dual-path MIR references.

## Key Decisions

**Delete-after-parity, not before.** The legacy path stays active until THIR produces
byte-for-byte identical IR on all 1,700+ tests and the IR-diff step confirms parity. Premature
deletion would require reverting if a hidden construct surfaces during Phase 3.

**One commit per construct fix.** Batching multiple THIR parity fixes into one commit makes
regression bisection impossible. Each commit names the specific construct it adds (e.g.,
`thir-mir: implement loop-break-with-value`).

**IR-diff is the parity gate, not test pass count.** Two paths can both pass the same test
while producing different IR that coincidentally computes the same answer. The IR-diff step in
Phase 2 (item 2.5–2.6) is a mandatory gate before any legacy file deletion begins.

**No deprecation warning period.** `--legacy` is an internal development flag, not a user-facing
API. No users exist to warn; delete cleanly with no bridge.

## Files to Create/Modify

**Deleted**:
- `compiler/src/mir/hir_mir_builder.cpp` (818 LOC)
- `compiler/src/mir/builder/hir_expr.cpp` (1,227 LOC)
- `compiler/src/mir/builder/hir_expr_control.cpp` (836 LOC)
- `compiler/src/mir/builder/hir_stmt.cpp` (144 LOC)
- `compiler/src/mir/builder/hir_pattern.cpp` (446 LOC)
- `compiler/include/mir/hir_mir_builder.hpp`

**Modified**:
- `compiler/src/cli/dispatcher.cpp` — remove `--legacy` flag and all conditional dispatch
- `compiler/src/mir/thir_mir_builder.cpp` — constructs added during Phase 2 parity work
- `compiler/src/mir/thir_mir_builder_expr.cpp` — expression constructs added during Phase 2
- `compiler/CMakeLists.txt` — remove deleted files from `target_sources`
- `CLAUDE.md` — update architecture map, remove dual-path MIR references

**Temporary (not committed)**:
- `.sandbox/thir-failures.log` — audit output from Phase 1

## Success Criteria

- Full test suite (1,700+ tests) passes on THIR-only path before any legacy file is deleted
- IR-diff on 50 representative files reports zero semantic differences between the two paths
- Compiler builds clean with zero warnings after legacy deletion
- `tml build --legacy` produces an "unknown flag" error
- Net LOC reduction is >= 3,400 lines across deleted files

## Dependencies

**Blocks**: phase12e (AST serializers need stable, single MIR output to define binary format),
phase12f (hybrid pipeline cannot safely wrap a stage with two competing implementations).

**Depends on**: Nothing. The task can start immediately. The IR-diff steps in Phase 2 are
best-effort without phase12d — textual comparison on normalized IR is sufficient if the
IR-diff tool is not yet available.
