# phase27a — K001 Root-Cause Sweep (Stabilization ERA 0, Phase C1)

> Analysis: `docs/analysis/tml-table-analysis/03-codegen-stability.md` (F-005, F-007).
> Fix the MECHANISM (mangling/monomorphization mismatches), not the instance —
> the release history shows one-off K001 fixes (0.3.38/0.3.46) keep being followed
> by new instances of the same shape. Requires phase25b (verifier) landed so
> fixes are protected.

## 1. Implementation
- [ ] 1.1 Root-cause `std/collections` K001 (btreeset / btreemap / arraylist) — these are the flagship container suites; trace the exact verifier error to the emitting code path (MIR vs legacy)
- [ ] 1.2 Root-cause `hir_types` + `infer_differential` K001
- [ ] 1.3 Root-cause `c_preprocessor` K001 and `c_frontend` `Maybe[Heap[CBlockItem]]` K001
- [ ] 1.4 Root-cause the `future_fuse` / `cache_aligned_box` / `cache` / `cache_soavec_set` K001 class (surfaced by phase24l Attempt 3 — generic monomorphization of `Shared` users; likely same mechanism as 1.1)
- [ ] 1.5 Audit the impl-level type-param re-inference path (`method_impl.cpp` heuristics fixed in 0.3.46) and the struct-by-value ABI fixups (0.3.39) for remaining holes; replace heuristics with authoritative signature data where possible
- [ ] 1.6 Gate: zero K001 across compiler + core + std suites with the verifier (phase25b) as hard error

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
