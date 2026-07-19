# Proposal: phase44a_test-dispatcher-false-pass

## Why

The standalone test dispatcher reports `test_pass` for a test body that
PANICS mid-run. Discovered 2026-07-18 during phase26c verification:
`lib/core/tests/alloc/shared_get_sound.test.tml` printed
`panic: assertion failed at :39` and then emitted `{"event":"test_pass"}`
when run standalone, while the suite-packed run of the same binary correctly
surfaced the crash. This masked a REAL codegen soundness bug (bitwise
`ptr_read_clone` fallback for handle-bearing structs, fixed in phase26c) and
made it look composition-sensitive for a full debugging session.

A harness that can report pass on a panicking body silently undermines every
single-file verification run — the workflow every agent and developer uses
first. Trust in the standalone runner is a stabilization-era prerequisite.

## What Changes

Root-cause the standalone dispatcher path in
`compiler/src/testing/testing_dispatcher_gen.cpp`: a panic that unwinds or
exits the test body must produce `test_fail` (with the panic message) and a
non-zero suite exit, matching the suite-mode crash-detection semantics.
Add a regression fixture: a deliberately panicking `@test` run standalone
must be reported FAIL by both the NDJSON events and the process exit code.

## Impact

- Affected specs: ADR-004 (NDJSON subprocess test protocol).
- Affected code: `compiler/src/testing/testing_dispatcher_gen.cpp` (+ possibly
  the runtime panic handler's exit path when embedded in a test EXE).
- Breaking change: NO (tests that were silently broken will start failing —
  that is the point; survey fallout and file follow-ups rather than mask).
- User benefit: standalone test runs become trustworthy; no more false-pass
  mirages during codegen debugging.

## Source

- phase26c session 2026-07-18: codegen-debugger agent evidence
  (`build/debug/cache/tests/core_alloc.exe --run-all` vs standalone run of
  `shared_get_sound`).
