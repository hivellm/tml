# phase26c — Revert phase24 Band-Aids + Close the Bug Class (Stabilization ERA 0, Phase B gate)

> The proof that phase26b actually fixed the model is that the workarounds can
> ALL be deleted and everything still passes. Analysis:
> `docs/analysis/tml-table-analysis/04-self-hosting-and-strategy.md` (collateral tech debt).

> **RE-SCOPED 2026-07-17** (task predates two standing decisions): (a) `compiler-tml/`
> is **FROZEN** (user rule, 2026-07-16 — never anchor work or gates on cc_driver), so
> the ~70 `.duplicate()` partial-move band-aids in `compiler-tml/src/cc/` and the
> `essential.c --emit=ast` 100/100 gate are OUT of this task — phase26f 1.7 proved
> essential.c's 0% floor is a segfault in the frozen C frontend (pre-IR, not K001/K002);
> those reverts belong to the self-hosting era (phases 30–33) when the freeze lifts.
> (b) The `std/collections K001-free` gate DEPENDS on phase27a (the `{i64,i64}`-vs-`i32`
> re-inference family) — so phase27a executes FIRST; 26c closes the era-0 gate after.
> In-scope here: band-aids in the C++ compiler + `lib/core`/`lib/std` + the API-surface
> decision (1.3).

## 1. Implementation
- [ ] 1.1 Inventory every phase24c–24n workaround still in tree WITHIN the non-frozen surface (C++ compiler, `lib/core`, `lib/std`, tests): `into_raw()`/`from_raw()` drop-suppression chains, deliberate-leak sites, `get_clone` call-site migrations forced by the old bug class, hazard docstrings. (compiler-tml/src/cc/ items: enumerate for the record, do NOT touch — frozen)
- [ ] 1.2 Revert the in-scope band-aids in dependency order, re-running the determinism corpus (adversarial) after each cluster
- [ ] 1.3 Simplify the library API surface: decide fate of `Shared.get_clone`/`get_ref` and `ptr_read_clone` (keep as documented API or fold into now-sound `get`); remove the hazard docstrings that describe the old bug class
- [ ] 1.4 Final gates (re-anchored per the freeze rule): pure-TML determinism corpus adversarial at/above floors ×100; `std/collections` (btreemap/btreeset/arraylist) K001-free (**after phase27a**); compiler + core + std suites at or above pre-revert baseline
- [ ] 1.5 Capture learning: what the root cause was, why band-aids didn't converge, how the model fix closed it (`rulebook_memory`, kind: learning)

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
