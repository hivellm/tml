# Proposal: phase26a_memmodel-adr-decision

## Why

Fourteen consecutive phases (24a→24n, ~30 commits, 0.3.39–0.3.52) attacked the
double-free/use-after-free class with per-site workarounds, a structural
Heap→Shared migration, ~70 manual `.duplicate()` calls, deliberate leaks, and
finally the `ptr_read_clone` codegen intrinsic. The phase24l Attempt log
proves the path is non-convergent: broader call-site migration REGRESSED the
repro (30/30 → 25/30, refcount imbalance), and the language-level `.get()`
deep-clone REGRESSED lib/core (K001 in 4 unrelated monomorphizations).
`essential.c` remains 0/5. The root cause is architectural: lexical-scope drop
insertion (`compiler/src/codegen/llvm/core/drop.cpp`) over smart pointers
built on raw `*T` that the borrow checker cannot see through (F-001..F-004).
An architectural defect needs an architectural decision — made once,
explicitly, with the user signing off — before any more implementation effort
is spent.

## What Changes

ADR-009 (`docs/adr/ADR-009-memory-model-soundness.md`) evaluating exactly two
exits: **B1** — Rust-faithful move/init-state tracking + drop-flag elaboration
in MIR (drops fire only on initialized, non-moved locals; container read-out
becomes borrow-then-clone or explicit move); **B2** — ARC/Swift-style
compiler-inserted retain/release on every copy/drop of refcounted owning
types. Includes feasibility spikes on the repro corpus, a decision matrix
(soundness, performance model, compiler complexity, migration cost), a
verification of F-013 (`Shared::increment_count` bitwise-copies
`SharedInner[T]`), and a recommendation. User sign-off gates phase26b.

## Impact

- Affected specs: memory-model / ownership sections of the language spec
  (clarified, direction set here; changed in 26b).
- Affected code: none in this task (ADR + spikes only).
- Breaking change: NO (decision document).
- User benefit: ends the band-aid loop; all subsequent memory work executes
  one coherent, signed-off design.

## Source

- docs/analysis/tml-table-analysis/02-memory-model-unsoundness.md (F-001..F-004, F-013).
- .rulebook/tasks/phase24l_shared-get-aliasing-deep-fix/tasks.md — Attempt log.
- lib/core/src/alloc/shared.tml:118-138 — the library's own hazard docstring.
