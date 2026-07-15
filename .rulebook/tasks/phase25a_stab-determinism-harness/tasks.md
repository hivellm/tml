# phase25a — Determinism Harness (Stabilization ERA 0, Phase A2)

> Analysis: `docs/analysis/tml-table-analysis/06-execution-plan.md` (Phase A).
> Goal: crash rate becomes a tracked CI number, not a vibe. Heap-layout-dependent
> bugs must surface deterministically instead of 2-in-30.

## 1. Implementation
- [x] 1.1 Repro runner script — `scripts/determinism.sh` (single-command + `--corpus` modes, N runs, pass-rate, exit-code histogram, `--min-pass` threshold, per-run timeout)
- [x] 1.2 Corpus registered — `scripts/determinism-corpus.txt`, TWO sections per user directive (compiler-tml is FROZEN): PRIMARY = 5 pure-TML `@test`-framework repros in `compiler/tests/determinism/` (F-002 hashmap/list, phase24h partial-move enum, F-013 refcount cycles, UzDB-shaped churn), run as prebuilt exes via ADR-004 `--run-all`; LEGACY (secondary) = cc_driver repros, fixtures moved OUT of compiler-tml to `compiler/tests/determinism/legacy-cc/`. Authoring the corpus surfaced 4 new checker/codegen divergence specimens — recorded in phase27a tasks.md.
- [x] 1.3 Poison mode — `TML_ALLOC_POISON=1` fills freed blocks with 0xDD (`compiler/runtime/memory/mem.c::adv_free`); zero overhead when unset (single cached-flag branch in `mem_free`)
- [x] 1.4 Quarantine mode — `TML_ALLOC_QUARANTINE=N` FIFO ring; double-free of a quarantined block reports `detected by TML_ALLOC_QUARANTINE` and aborts deterministically (verified via `compiler/tests/determinism/fixtures/double_free_probe.tml`: flags OFF = silent heap abort, flags ON = deterministic diagnostic)
- [x] 1.5 Baseline ×100 recorded in `docs/analysis/tml-table-analysis/07-determinism-baseline.md` — pure-TML: 5/5 targets 100/100 both modes (regression sentinels); legacy: essential.c 0/100 (mode split 139×72/127×28 now tracked), c_essential_repro 86/100 normal (refines the flattering ×30 "28/30") and 98/100 adversarial — quarantine converts read-after-free crashes into silent wrong data, so pass-rate must pair with checksum asserts (key input for phase26 gates)
- [x] 1.6 Gate integration — GitHub CI runs `--ci` builds without full LLVM (TML tests disabled there), so the gate is local: `scripts/determinism-gate.sh [runs]` (recompiles corpus exes, adversarial ON, per-target baseline floors) wired into `.git/hooks/pre-push` at 10 runs/target with `TML_SKIP_DETERMINISM=1` escape hatch; harness self-test in `scripts/determinism-selftest.sh` (3 checks, all green)

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [x] 2.1 Documentation — `docs/analysis/tml-table-analysis/07-determinism-baseline.md` (numbers + interpretation + reproduce section), env-var contract documented in `mem.c` block comment and both script headers, patch notes v0.3.53
- [x] 2.2 Tests — 5 corpus `.test.tml` suites (checksum-validating canaries), `scripts/determinism-selftest.sh` (double-free detection, clean-run under flags, runner enforcement), `scripts/fixtures/double_free_probe.tml` (kept OUT of test discovery — a deliberately-crashing fixture under compiler/tests/ was picked up as an X003 suite; moved to scripts/fixtures/)
- [x] 2.3 All green — core/alloc 41/41, determinism suite 5/5, selftest 3/3, gate 17/17 targets at floor (10 runs, adversarial ON)

## Attempt log
- Initial corpus draft anchored on cc_driver repros and standalone-main .tml files; user corrected BOTH: compiler-tml is frozen (nothing new anchored on it) and tests must use the @test framework (T7). Final shape: pure-TML @test suites primary, legacy cc secondary.
- Adversarial-mode finding: quarantine+poison RAISES the pass rate of the read-after-free class (c_essential_repro 86→98) by converting crashes into silent wrong data — pass-rate must pair with checksum asserts. Double-free class IS deterministically caught (fixture: 100% abort with diagnostic).
- Authoring the 5 corpus files surfaced 4 checker/codegen divergence specimens → recorded in phase27a tasks.md.
