# phase44a — Standalone Test Dispatcher False-Pass on Panic

> Evidence (2026-07-18, phase26c session): standalone run of
> `lib/core/tests/alloc/shared_get_sound.test.tml` printed
> `panic: assertion failed at :39` and then `{"event":"test_pass"}`; the
> suite-packed run of the same body correctly reported the crash. The
> false-pass masked a real codegen soundness bug and mimicked
> composition-sensitivity.

## 1. Implementation
- [ ] 1.1 Root-cause the standalone dispatcher path (`compiler/src/testing/testing_dispatcher_gen.cpp` + the runtime panic handler exit path inside test EXEs): why does a panicking body still reach the `test_pass` emission? Trace the actual control flow with a deliberately panicking fixture.
- [ ] 1.2 Fix: panic in a test body ⇒ `test_fail` NDJSON event carrying the panic message + non-zero process exit, identical semantics in standalone and suite modes.
- [ ] 1.3 Fallout survey: run the alloc/collections/str suites standalone-per-file and catalogue any tests that were silently failing behind the false-pass; file follow-ups rather than mask.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update or create documentation covering the implementation (ADR-004 notes if the NDJSON contract text changes)
- [ ] 2.2 Write tests covering the new behavior (regression fixture: panicking `@test` must FAIL standalone — events + exit code)
- [ ] 2.3 Run tests and confirm they pass
