# Proposal: Fix Pre-Existing Test Failures

## Why

The compiler test suite has 7 pre-existing failures across 264 suites (257 pass). These failures
have persisted across multiple sessions and block 100% pass rate. Each failure has a different
root cause: 2 runtime crashes (K001 codegen bugs), 2 runtime timeouts (X002 excessive test
duration), 2 codegen timeouts (X002 full parser chain exceeds 30s), and 1 transient linker error.
Fixing them brings the suite to 264/264 and eliminates noise in regression testing.

## What Changes

Fix 7 test failures:

1. **doc_generation crash (X003)** — exit code -1073741784 (stack overflow or heap corruption).
   Investigate the doc generation test, likely hits a recursive type or large struct codegen issue.

2. **maybe_inference crash (X003)** — exit code -1073740791 (heap corruption 0xAB fill pattern).
   Known K001 variant: Maybe[T] inference test constructs large structs at runtime.

3. **infer_differential timeout (X002)** — exceeds 100ms runtime limit.
   The 10 differential inference tests are too slow; optimize or increase timeout.

4. **mir_passes timeout (X002)** — exceeds 100ms runtime limit.
   MIR optimization pass tests are computationally heavy; optimize or split.

5. **parser_basic codegen timeout (X002)** — exceeds 30s codegen.
   Full parser chain (~8K lines) generates too much IR for the 30s compilation limit.

6. **test_frontend_sim codegen timeout (X002)** — same root cause as parser_basic.

7. **destructuring_let linker error (N001)** — transient LLD "permission denied" file lock.
   Race condition with antivirus or concurrent process holding the .exe file.

## Impact

- Affected code: compiler/src/codegen/ (K001 fixes), compiler-tml/tests/ (timeout adjustments)
- Breaking change: NO
- User benefit: 264/264 test pass rate, clean CI, no false positive regressions
