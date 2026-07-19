# Proposal: phase26g_collection-mutator-mut-this

## Why

phase26e Cluster D landed the borrow-checker wiring that binds a `get_ref`
result to its container borrow and emits B009 on conflicts — with zero false
positives. But the headline invalidation case (`let r = list.get_ref(0);
list.push(x); use(*r)`) is NOT caught, because every collection mutator
(`push`, `insert`, `remove`, `set`, …) is declared `this`, not `mut this` —
the handle-based collections historically allow mutation through immutable
bindings. Empirically (measured 2026-07-19 during phase26e): flipping the
mutator surface to `mut this` breaks 20+ std tests with "not declared as
mutable" — idiomatic TML currently writes `let l = List::new(); l.push(x)`.
This is a language-ergonomics decision (immutable bindings that mutate vs
Rust-style `var` requirement), the same shape as the moves activation the
user chose to stage as a conscious milestone (decision #12) — it must not
ship as a side-effect of another task.

## What Changes

USER DECISION FIRST: adopt Rust-style mutability (`var l` required to call
`mut this` methods) for collections, or keep handle-mutability and find an
alternative invalidation signal (e.g. an `@invalidates_refs` method
attribute the checker treats as requiring unique access without the
type-level mutability demand). Then: migrate the mutator signatures (or add
the attribute) across List/HashMap/BTreeMap/Deque/HashSet/Buffer, migrate
the std/lib call sites (`let`→`var` where needed), and un-gate the already-
landed borrow wiring (it activates per-method automatically the moment a
mutator is resolvable as requiring unique access).

## Impact

- Affected specs: ownership/borrow spec; collections API docs.
- Affected code: `lib/std/src/collections/*.tml` signatures; widespread
  `let`→`var` call-site migration in lib/std + tests; zero borrow-checker
  changes needed (wiring already landed in phase26e).
- Breaking change: YES if `mut this` is chosen (source-level: immutable
  bindings can no longer call mutators). NO if the attribute route is chosen.
- User benefit: `get_ref`-then-invalidate becomes a compile error — the full
  memory-safety story for interior references (phase26e delivered the
  conflict detection; this delivers the invalidation detection).

## Source

- phase26e Cluster D report (2026-07-19): blast-radius measurement, the
  `ref_place != 0` interior-only design, and the ready-to-fire wiring.
- `.rulebook/tasks/phase26e_collection-borrow-accessors/specs/borrow-lifetime-design/spec.md`.
