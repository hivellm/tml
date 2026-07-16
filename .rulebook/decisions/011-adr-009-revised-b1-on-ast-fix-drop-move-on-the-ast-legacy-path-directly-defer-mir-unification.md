# 11. ADR-009 revised: B1-on-AST — fix drop/move on the AST-legacy path directly; defer MIR unification

**Status**: proposed
**Date**: 2026-07-15
**Related Tasks**: phase26b_memmodel-implementation

## Context

B3's premise was refuted: query_core.cpp:931 shows the test framework uses the IDENTICAL AST-vs-MIR gate as build.cpp — both route stdlib/generic programs to AST. The test framework does NOT use MIR for these programs; 'query pipeline' != 'MIR codegen'. So B3's Step 2 (unify onto MIR) is not routing (~5%) but a ~8,000-LOC MIR codegen project (imported-function emission + generic monomorphization worklist + derives + unions), making B3 MORE work than B1, with its 'avoid double implementation' justification void. Evidence: .rulebook/tasks/phase26b_memmodel-implementation/specs/step2-scoping/spec.md.

## Decision

Supersedes the same-day B3 acceptance. Implement real move/init-state + drop-flag elaboration on the AST-legacy codegen path (LLVMIRGen/drop.cpp/consumed_vars_) — the path 100% of real programs AND all tests run. Steps: (1) F-013 shared.tml fix [done v0.3.55]; (2) surface the borrow checker's OwnershipState/init-state facts (checker.hpp:799-816, currently discarded before codegen) into AST codegen, replacing name-based consumed_vars_ with per-place init tracking, extended below lowlevel; (3) make every container/smart-pointer read-out a balanced clone or tracked move (kill the bitwise-aggregate-alias fallthrough), make destroy run per-element Drop, then REMOVE the drop.cpp:460-471 leak special-case; (4) drop-flag elaboration for control-flow-dependent drops. MIR unification (old B3) deferred to phases 30-33 (frozen).

## Alternatives Considered

- B3 (unify onto MIR first, then drop-flags once): rejected — premise refuted, ~8000 LOC migration before any memory fix
- B2 ARC: rejected earlier

## Consequences

_No consequences documented._
