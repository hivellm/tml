# Proposal: phase25a_stab-determinism-harness

## Why

The memory-model bug class (docs/analysis/tml-table-analysis/, F-001..F-003)
produces heap-layout-dependent crashes: `c_essential_repro.c` passes 28/30,
`essential.c` passes 0/5, and pass rates historically moved between phases
(sig_alone.c: 0% → 60% → 100%) without the underlying defect being closed.
A bug that reproduces 2-in-30 and shifts with allocator state cannot be
bisected, cannot be gated, and cannot be trusted as "fixed". Until crash
rate is a measured number, every fix claim in phases 26–27 is unverifiable.

## What Changes

A determinism harness: (1) a repro runner that executes a target N times and
reports pass-rate + exit-code histogram; (2) the standing repro corpus
(essential.c, c_essential_repro.c, sig_alone.c, int_p.c, phase24h/24i minimal
repros) registered as tracked targets; (3) an adversarial allocator mode in
the TML runtime — freed-memory poisoning (0xDD fill) and a free-quarantine so
use-after-free fails loudly and deterministically instead of silently reading
stale heap; (4) baseline numbers recorded and a CI job that fails on
pass-rate regression.

## Impact

- Affected specs: none (tooling + runtime debug mode).
- Affected code: `scripts/` (runner), TML runtime allocator
  (`compiler/runtime/` alloc path — debug-only flag), CI config,
  `docs/analysis/tml-table-analysis/07-determinism-baseline.md` (new).
- Breaking change: NO (debug-only, opt-in flag).
- User benefit: crash rate becomes a tracked CI metric; phases 26–27 gain
  falsifiable gates; heap-layout bugs become deterministic and debuggable.

## Source

- docs/analysis/tml-table-analysis/06-execution-plan.md — Phase A2.
- Analysis finding F-008 (non-deterministic crashes are the adoption killer).
