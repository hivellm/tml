# 13. Collection mutators require unique access (mut this) — Rust-style mutability adopted

**Status**: proposed
**Date**: 2026-07-19
**Related Tasks**: phase26g_collection-mutator-mut-this

## Context

phase26e delivered interior-ref borrow checking (B009 on conflicting refs, zero false positives), but get_ref-then-push invalidation was undetectable while collection mutators were declared `this` — mutation through immutable bindings was idiomatic TML. Measured blast radius of the flip: 20+ std tests fail "not declared as mutable". Alternative considered: an @invalidates_refs attribute (no source breakage, checker-only unique-access requirement).

## Decision

User decision 2026-07-19 (phase26g): adopt Rust-style mutability — collection mutators (List/HashMap/BTreeMap/Deque/HashSet/Buffer: push/insert/remove/set/clear/sort/...) migrate to `mut this`; call sites migrate `let` → `var` where they mutate. Mutation becomes visible in the binding type; the phase26e borrow wiring then automatically catches get_ref-then-mutate as B009. Breaking change accepted (source-level).

## Alternatives Considered

- @invalidates_refs attribute route: no breakage, same interior-ref safety, but mutability stays invisible in bindings — rejected by user
- Defer to a later era — rejected, decided now

## Consequences

_No consequences documented._
