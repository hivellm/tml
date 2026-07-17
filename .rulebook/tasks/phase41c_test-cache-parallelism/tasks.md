## 1. Implementation
- [ ] 1.1 F-014 diagnosis: instrument/trace why `tests.json` stays ~empty in normal runs (is `save` never called? does `all_passed` gating skip persistence? does the write fail silently?); document root cause with file:line evidence
- [ ] 1.2 F-014 fix — persistence: store per-suite results reliably (including runs with failures — cache what passed, recheck what failed); verify `tests.json` populates across two consecutive runs and the second run skips recompiling unchanged green suites
- [ ] 1.3 F-014 fix — invalidation: replace whole-DLL mtime:size fingerprint with a content-aware key (compiler VERSION + DLL content hash computed once per run); verify a no-op rebuild (touch DLL without behavior change... if unreachable, rebuild twice) no longer wipes the EXE cache; correctness bias documented (when in doubt, invalidate)
- [ ] 1.4 F-010: batch/append incremental-cache writes (per-suite flush instead of per-file global-mutex write); measure suite-worker serialization before/after
- [ ] 1.5 F-013: single import-scan per test file, result threaded through registry synthesis / runtime-object selection / package detection
- [ ] 1.6 F-008/F-009 (as far as phase41b's codegen-state restructuring allows): raise per-suite file parallelism > 1 with evidence it is safe, or document the precise LLVM global-state blocker that remains; replace per-file detached watchdog threads with a shared timer wheel
- [ ] 1.7 GATE: second consecutive `tml test` run (no changes) reuses cache and its wall-clock is recorded (expect near-free); full-run wall-clock before/after in `01-measurements.md`; zero result divergence

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation: `04-test-framework-performance.md` (F-008/F-009/F-010/F-013/F-014 statuses with numbers); CHANGELOG/VERSION bump
- [ ] 2.2 Write tests covering the new behavior (cache persistence with partial failures, content-aware invalidation, import-scan single-pass)
- [ ] 2.3 Run tests and confirm they pass
