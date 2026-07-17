# Proposal: phase26b_memmodel-implementation

## Why

Implements the model chosen in ADR-009 (phase26a). This is THE milestone of
the stabilization era: every real-application failure mode traced in the
analysis (UzDB abandonment, essential.c 0/5, the entire phase24 grind)
reduces to unsound drop insertion over aliased owned handles. No downstream
work — codegen polish, self-hosting, feature phases — has value until a
value read out of a container cannot free storage the container still owns.

## What Changes

Per ADR-009, either: **B1** — MIR gains per-local init/moved-state tracking
through the CFG with drop-flag-guarded conditional drops, and container
read-out lowers to borrow-then-clone or explicit move; or **B2** — codegen
auto-inserts retain/release on every copy/drop of refcounted owning types.
In both cases: `Shared/Heap/HashMap/List/BTreeMap.get` becomes sound by
construction (superseding `ptr_read_clone`'s conservative detection),
`Shared::increment_count`/`decrement_count` stop bitwise-copying
`SharedInner[T]` (F-013), and the stdlib's `lowlevel` internals come under
the model (F-004).

## Impact

- Affected specs: memory-model/ownership language spec sections.
- Affected code: `compiler/src/mir/` (thir_mir_builder*, drop elaboration),
  `compiler/src/codegen/llvm/core/drop.cpp`, borrow-checker integration
  (`compiler/src/borrow/`), `lib/core/src/alloc/{heap,shared}.tml`,
  collections in `lib/std/src/collections/`.
- Breaking change: POTENTIALLY — copy/clone semantics of container getters
  become well-defined; code relying on accidental aliasing changes behavior.
  Documented in the ADR + migration notes.
- User benefit: the double-free class ceases to exist; gates go from 28/30
  and 0/5 to 100/100 under the adversarial allocator.

## Source

- ADR-009 (phase26a). Analysis: docs/analysis/tml-table-analysis/02 + 06.
