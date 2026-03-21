# 4. NDJSON Subprocess Test Protocol (Go Model)

**Status**: proposed
**Date**: 2026-03-15

## Context

Test runner needs crash isolation, structured results, and parallel execution. Previous in-process test runner could not isolate crashes and leaked LLVM state between suites.

## Decision

Each test suite compiles to a standalone .exe that streams NDJSON events (suite_start, test_start, test_output, test_pass/fail/crash/timeout/skip, suite_end) to stdout. Coordinator parses events without maintaining parser state across lines. Modeled on Go's test2json.

## Alternatives Considered

- In-process test execution (no crash isolation)
- Protobuf over pipes (heavier protocol)
- TAP format (less structured, no crash events)
- JUnit XML (post-hoc only, no streaming)

## Consequences

Pros: Full crash isolation via process boundaries, structured streaming results, any language can speak the protocol. Cons: Compiling 200+ suite executables adds overhead; Windows process startup cost is non-trivial. Coverage via TML_COVERAGE_FILE env var avoids LLVM profiling instrumentation issues.
