# phase25a — Determinism Harness (Stabilization ERA 0, Phase A2)

> Analysis: `docs/analysis/tml-table-analysis/06-execution-plan.md` (Phase A).
> Goal: crash rate becomes a tracked CI number, not a vibe. Heap-layout-dependent
> bugs must surface deterministically instead of 2-in-30.

## 1. Implementation
- [ ] 1.1 Repro runner script (`scripts/determinism.{bat,sh}` or `tml`-driven): runs a given executable/repro N times (default 100), records exit codes, and prints `pass/N` + failure exit-code histogram
- [ ] 1.2 Register the standing repros as the tracked corpus: `essential.c --emit=ast`, `c_essential_repro.c`, `sig_alone.c`, `int_p.c`, phase24h/24i minimal repros (from `compiler-tml/tests/native/`)
- [ ] 1.3 Adversarial allocator mode for TML runtime: debug flag (env var or `tml` flag) enabling allocation poisoning (fill freed memory with 0xDD pattern) so use-after-free reads fail loudly instead of silently reading stale data
- [ ] 1.4 Guard/quarantine mode: freed blocks are quarantined (not reused) for K allocations, converting heap-layout-dependent crashes into deterministic ones
- [ ] 1.5 Baseline measurement: run the corpus ×100 under normal AND adversarial modes; record the numbers in `docs/analysis/tml-table-analysis/07-determinism-baseline.md`
- [ ] 1.6 CI integration: determinism run (corpus × 30 minimum) as a scheduled/pre-push job with the pass-rate published in output; regression = pass-rate drop vs recorded baseline

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
