# phase27c — Flaky Heap-Corruption in Module-Path Resolution (nondeterministic typecheck)

> Discovered during phase26f 1.5/1.6 (2026-07-16, reported by the codegen agent).
> Some `tml run`/`tml build` invocations on files importing `core::alloc` + `std`
> modules **intermittently** fail typecheck with a nondeterministic
> `Module '::alloc::::' not found` — dropped/garbled path segments, and the error
> string VARIES run-to-run (classic uninitialized-read / use-after-free signature in
> the C++ module resolver, not a logic bug). **Reproduced on the pre-1.5 v0.3.63
> binary → pre-existing, NOT from the move-semantics work.** Low frequency (3/183
> files hit it during the 1.6 sweep; determinism canaries stayed 30/30), but it is
> memory corruption inside the compiler itself — it will bite harder as the corpus
> grows and undermines the determinism guarantee ERA 0 is built on.

## 1. Implementation
- [ ] 1.1 Build a reliable repro harness: identify the 3/183 sweep files that hit `corrupt_hits` (see phase26f `specs/blast-radius/spec.md` re-measurement table + the 1.6 sweep logs), loop each N=200 under the adversarial allocator env; record hit-rate per file and the full set of garbled error strings. If ASan-style tooling is impractical on the Zig CC build, use `TML_ALLOC_POISON`-equivalent instrumentation on the compiler-side allocations or targeted debug logging in the module resolver
- [ ] 1.2 Root-cause: trace the module-path string's lifetime through the resolver (import parse → path segment storage → lookup key construction). The varying error text means the bytes backing the path are freed or overwritten while still referenced — find the exact owner/borrower pair (research-first rule: no fixes before the mechanism is proven with file:line)
- [ ] 1.3 Fix at the root (ownership/lifetime of the path storage), not by re-ordering lookups to mask the read
- [ ] 1.4 Gate: the repro harness at 200/200 clean per affected file; determinism corpus adversarial at/above floors; no new K002; core/alloc + collections suites at baseline

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
