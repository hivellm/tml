# phase26g — Collection Mutators: unique-access requirement (unlocks get_ref-then-push detection)

> Prerequisite context: phase26e Cluster D landed the interior-ref borrow
> wiring (zero false positives); it cannot see push-class invalidation while
> mutators are `this`. Blast radius of a naive `mut this` flip measured at
> 20+ std tests ("not declared as mutable"). USER DECISION REQUIRED before
> implementation (see proposal): `mut this` migration vs `@invalidates_refs`
> attribute route.

## 1. Implementation
- [ ] 1.1 Present the decision to the user with the measured blast radius and a small side-by-side (ergonomics + safety) — record outcome as a rulebook decision
- [ ] 1.2 Implement the chosen route across List/HashMap/BTreeMap/Deque/HashSet/Buffer mutators
- [ ] 1.3 Migrate lib/std + test call sites as required by the chosen route
- [ ] 1.4 Enable/verify the invalidation error: `let r = c.get_ref(i); c.push(x); use(*r)` = compile error on all four collections; NLL-legal shapes stay green
- [ ] 1.5 Blast-radius sweep at zero unexpected new errors; determinism gate; suites at baseline

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
