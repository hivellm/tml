# 10. ADR-009: B3 — unify codegen paths on query/MIR pipeline, then drop-flag elaboration once

**Status**: proposed
**Date**: 2026-07-15
**Related Tasks**: phase26b_memmodel-implementation

## Context

Spikes proved: F-013 refcount bleed is real on the user path (tml run: nested count 2->1->-1, silent UAF), and the test framework compiles via query/MIR (no bleed) while user builds take the AST-legacy fallback (bleeds) — the 12k-test suite validates a path real programs never run. Full evidence: docs/adr/ADR-009-memory-model-soundness.md + phase26a groundwork spec.

## Decision

Memory-model soundness strategy: Option B3. (1) Immediate F-013 mitigation in shared.tml (read counters via field pointer, never copy SharedInner[T]); (2) close MIR imported-module + generic-instantiation gaps and flip tml build/run to the query/MIR pipeline the test framework already uses; (3) retire the AST-legacy emission path; (4) implement move/init-state + drop-flag elaboration ONCE in MIR (first-class DropInst, wire mark_moved, port borrow-checker init-state dataflow down). phase26c then reverts all phase24 band-aids. Gates: determinism corpus x100 adversarial + full suite + K002 verifier at every step.

## Alternatives Considered

- B1: drop-flags implemented on both paths (rejected: 2x work, divergence remains)
- B2: ARC-style auto retain/release (rejected: predicate from scratch, unsound primitives first, perf model change)

## Consequences

_No consequences documented._
