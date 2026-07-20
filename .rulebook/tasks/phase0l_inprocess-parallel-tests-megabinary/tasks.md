# phase0l — In-process parallel tests + mega-binary (the test-speed endgame)

> Filed 2026-07-20 from `docs/analysis/language-deep-review/` findings
> L-101/L-105/L-108 (06-runtime-concurrency.md). HARD dependency: phase0c
> (real atomics) — running TML tests on threads with fake atomics is unsound.
> Re-based premise: test EXEs are 0.3–0.7 MB static binaries with ~11–30 ms
> spawn (NOT "~100 MB DLLs / ~500 ms"); the real lever is 1 link instead of
> ~176 + intra-run thread parallelism.

## Motivation

The in-process panic/crash isolation machinery already exists and runs per-test
today (setjmp + VEH + `RtlRestoreContext` severe-crash recovery,
`compiler/runtime/core/essential.c:1383-1480`). It is single-threaded only
because ~15 pieces of its state are plain C statics (zero `_Thread_local` in
the runtime). Estimated scope for thread-correctness: 1.5–2.5 engineer-weeks —
no LLVM EH, no landingpads, no new mechanism.

## 1. Implementation
- [ ] 1.1 `_Thread_local` the panic/catch/backtrace/recovery/output statics in
  `essential.c` (the Group-C single-TU constraint at :36-38 is unaffected);
  implement the POSIX side (sigaltstack + TLS jmp_buf) in the same change so
  the platform gap does not widen (L-108).
- [ ] 1.2 Thread-pool dispatcher consuming the codegen-emitted test
  fn-pointer table (`compiler/src/testing/testing_dispatcher_gen.cpp` + a
  small C runtime function); TLS output capture preserving per-test NDJSON
  ordering.
- [ ] 1.3 Watchdog: per-test `TerminateProcess` becomes a suite-level deadline
  (Go/Rust semantics); retain a subprocess fallback mode; the severe-crash
  policy (heap-corruption class aborts the process) stays unchanged.
- [ ] 1.4 Remove the per-entry fail-fast that skips an EXE's remaining tests
  after the first failure (`compiler/src/codegen/llvm/core/generate_entry.cpp:684-688`)
  — full results per run.
- [ ] 1.5 Mega-binary endgame: aggregate to one (or few) test binaries per
  run — 1 link instead of ~176 — executed by the threaded dispatcher. Measure
  and record full-suite wall-clock before/after.
- [ ] 1.6 Update the stale cost premises: the "~100MB of runtime DLLs" comment
  (`compiler/src/cli/commands/cmd_test.cpp:299`) and an errata note in
  `docs/analysis/architecture-performance-review/02-test-speed-architecture.md`
  (F-007), citing the L-105 measurements.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation (test execution model: threads, deadline,
  fallback, crash policy)
- [ ] 2.2 Write tests covering the new behavior (panic-on-thread isolation
  fixtures; parallel-suite determinism run 20/20; watchdog deadline fixture)
- [ ] 2.3 Run tests and confirm they pass
