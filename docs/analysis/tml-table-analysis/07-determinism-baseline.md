# 07 — Determinism Baseline (phase25a)

**Measured:** 2026-07-15 · **TML:** 0.3.52 + adversarial allocator (`compiler/runtime/memory/mem.c`)
· **Branch:** `fix/era0-stabilization` · **Runs:** 100 per target per mode · **Host:** Windows 10 (win32)

Harness: `scripts/determinism.sh --runs 100 --corpus scripts/determinism-corpus.txt`.
Adversarial mode: `TML_ALLOC_POISON=1 TML_ALLOC_QUARANTINE=256` (freed blocks filled
with 0xDD; FIFO quarantine of 256 entries; double-free of a quarantined block aborts
with a diagnostic — self-tested via `scripts/determinism-selftest.sh`, 3/3 green).

## PRIMARY — pure-TML corpus (`compiler/tests/determinism/*.test.tml`)

Targets the C++ compiler codegen + lib/core stdlib only (compiler-tml is frozen and
not involved). Run as prebuilt ADR-004 test exes (`--run-all`).

| Target | Shape | Normal | Adversarial | Exit codes |
|--------|-------|--------|-------------|------------|
| tml_refcount_bleed_userpath¹ | F-013 bleed canary via `tml run` (USER/AST path) | **100/100** | **100/100** | 0 ×100 |
| tml_f002_hashmap | undecorated struct + Shared via HashMap.get | **100/100** | **100/100** | 0 ×100 |
| tml_f002_list | undecorated struct + 2 Shared via List.get | **100/100** | **100/100** | 0 ×100 |
| tml_partial_move_enum | phase24h shape, recursive enum, no band-aid | **100/100** | **100/100** | 0 ×100 |
| tml_f013_refcount_cycles | 2000 duplicate/drop cycles, independent-handle detector | **100/100** | **100/100** | 0 ×100 |
| tml_churn_uzdb_shape | insert/read/remove churn ×64 rounds | **100/100** | **100/100** | 0 ×100 |

**Reading:** at 0.3.52 these single-exe canaries pass — the `ptr_read_clone` fix
(phase24n) plus the AST-path field-drop special case (drop.cpp:460-471) currently
hold this ground. Their value is as **regression sentinels**: any phase26/27 change
that drops one below 100/100 under the gate is an immediate red flag. They are NOT
proof of soundness — the C-frontend workload (below) exercises deeper composition
and still fails.

¹ **Added 2026-07-15 (v0.3.55, phase26b step 1):** unlike the other primary
targets (test-framework exes → query/MIR pipeline), this canary runs via
`tml run` because the F-013 refcount bleed only manifests on the USER build
path (AST codegen) — see ADR-009. Measured 0/N before the shared.tml
counter-read fix (nested count 2→1→−1, silent UAF); 100/100 both modes after.

**Collateral finding:** authoring these five small files surfaced **4 new
checker/codegen divergences** (constructs that pass `tml check` and fail compile) —
recorded in `.rulebook/tasks/phase27a_codegen-k001-root-cause/tasks.md` with repro
pointers. The corpus pays for itself before its first scheduled run.

## LEGACY (secondary) — cc_driver repros (phase24 era)

Same C++ codegen bug class exercised through the frozen compiler-tml C frontend.
Kept for comparability with phase24 history; NOT the primary gate.

| Target | Normal | Adversarial | Exit codes (normal → adversarial) |
|--------|--------|-------------|------------------------------------|
| essential.c | **0/100** | **0/100** | 127 ×28, 139 ×72 → 127 ×31, 139 ×69 |
| c_essential_repro | **86/100** | **98/100** | 0 ×86, 127 ×14 → 0 ×98, 127 ×2 |
| sig_alone | 100/100 | 100/100 | 0 ×100 |
| int_p | 100/100 | 100/100 | 0 ×100 |
| funcptr_typedef | 100/100 | 100/100 | 0 ×100 |
| macro_r0 … macro_r5 (6 targets) | 100/100 each | 100/100 each | 0 ×100 |

## Interpretation (feeds ADR-009 / phase26)

1. **essential.c is 0/100 — worse than the historical "0/5" suggested.** The failure
   is total but the failure MODE is layout-dependent: ~70% SIGSEGV (139), ~30% heap
   abort (127). The mode split is now a tracked number.
2. **c_essential_repro at 86/100 refines the phase24 "28/30" (93%) figure** — at
   ×100 resolution the true crash rate is ~14%, higher than the ×30 samples showed.
   Small-N sampling was flattering the numbers.
3. **Adversarial mode is not a universal determinizer for the aliasing class — and
   that is itself a finding.** Quarantine+poison RAISED c_essential_repro to 98/100:
   delaying block reuse means the dangling read sees poisoned-but-allocated memory,
   converting crashes into **silent wrong data** in paths that don't validate what
   they read. Consequences:
   - pass-rate alone under-counts the bug class; value-checking asserts (as in the
     pure-TML corpus checksums) are mandatory;
   - the quarantine's deterministic value is for the **double-free** shape
     (proven: fixture aborts 100% with the `detected by TML_ALLOC_QUARANTINE`
     diagnostic), not the read-after-free shape;
   - phase26's gate must use BOTH modes plus checksum-style validation, exactly as
     the corpus does.
4. **Gate floors** (encoded in `scripts/determinism-gate.sh`): every target 100,
   except `c_essential_repro` ≥ 90 and `essential.c` ≥ 0 (documented debt — phase26
   raises both to 100).

## Reproduce

```bash
# full baseline, both modes (~8 min)
scripts/determinism.sh --runs 100 --min-pass 0 --corpus scripts/determinism-corpus.txt
TML_ALLOC_POISON=1 TML_ALLOC_QUARANTINE=256 \
    scripts/determinism.sh --runs 100 --min-pass 0 --corpus scripts/determinism-corpus.txt

# gate (adversarial, baseline floors, default 30 runs; pre-push uses 10)
scripts/determinism-gate.sh

# harness self-test
scripts/determinism-selftest.sh
```
