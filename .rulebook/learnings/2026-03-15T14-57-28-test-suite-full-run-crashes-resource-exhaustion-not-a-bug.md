# Test suite full run crashes — resource exhaustion, not a bug
**Source**: manual
**Date**: 2026-03-15
**Related Task**: rewrite-test-system
**Tags**: testing, crash, resource-exhaustion, llvm
Running ALL 1452 tests at once causes segfault (exit code -1073741819). Individual suites all pass. This is resource exhaustion from concurrent compilation of 200+ suites, not a logic bug. The LLVM global state accumulation from compiling many suites in one process eventually corrupts memory. Mitigations in place: ThreadBudget limits concurrency, forced subprocess LLD, SEH wrapping. Full suite run should use sharding or sequential suite-by-suite execution.