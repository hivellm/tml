# 11. Testing Infrastructure

## 11.1 Overview

TML implements a subprocess-based testing architecture that departs significantly from in-process test models found in Rust libtest and Go testing package. The rationale for this choice, tradeoffs, and implications for LLM-assisted debugging are examined in this section.

As of April 2026, the test system has executed 1,682 test files containing 1,659 passing tests achieving 93.2 percent function coverage. The system generates structured NDJSON output for machine consumption by IDEs and AI tools. The distinctive feature — multi-layer IR diagnostics on failure (debug layers) — enables LLMs to understand bugs by reading compiler intermediate representations rather than source code alone.

---

## 11.2 Subprocess-Based Architecture

### 11.2.1 Design Rationale

The TML test system discovers all test functions (marked with @test decorator) and groups them by module. Each module is compiled into a standalone executable. The coordinator process launches one subprocess per module, communicates via NDJSON protocol, and aggregates results.

This architecture contrasts with conventional in-process models. Rust libtest compiles all tests into a single binary; the test harness runs tests in-process. However, a segfault or memory corruption in one test can corrupt the entire harness, losing all subsequent test results.

Go testing is similar — all tests compile into one binary and run in-process. Go runtime provides memory-safe panics, but concurrency bugs and C extension crashes can terminate the entire test run.

TML subprocess model isolates each test suite in its own process. A crash in one module does not affect others. The coordinator detects exit codes, records failures, and continues with the next module.

### 11.2.2 Process Lifecycle

The coordinator: (1) discovers .test.tml files, (2) groups tests by directory or individually, (3) compiles into standalone executables in parallel, (4) launches processes with environment variables, (5) communicates via NDJSON protocol, (6) aggregates results from parallel subprocesses, (7) generates reports.

---

## 11.3 NDJSON Protocol

Each test subprocess outputs NDJSON (newline-delimited JSON) events. The format is machine-parseable, allowing tools to consume results programmatically.

Advantages: machine-readable (LLMs parse structured JSON), hierarchical (nested information), streaming (real-time results), extensible (new fields without breaking consumers).

Standard event types: test_start, test_pass, test_fail, test_crash, test_skip, suite_pass, suite_fail, suite_crash.

---

## 11.4 Coverage System

The coverage system uses environment-variable-based instrumentation avoiding LLVM-based profiling complexity. Each function is instrumented with a coverage mark writing to TML_COVERAGE_FILE.

Advantages: no LLVM overhead, fast O(1) operation, accurate (every executed function recorded), portable (all platforms without LLVM knowledge).

Current status: 93.2 percent function coverage (1,659/1,775 functions). Target is 95 percent.

---

## 11.5 Debug Layers

The --debug-layers flag automatically emits intermediate representations (HIR, MIR, LLVM IR) for failing test functions.

When a test fails, the compiler emits HIR (desugared AST after type-checking), MIR (SSA-form basic blocks), and LLVM IR (final intermediate representation).

Example: A test fails due to wrong struct field memory offset. Without debug-layers, output is assertion failure message. With debug-layers, output includes HIR showing struct definition, MIR showing memory operations with offsets, and LLVM IR showing actual type layout and alignment decisions.

An AI agent reading multi-layer output can immediately diagnose issues like padding discrepancies.

Hypothesis: LLMs debug more effectively with structured IR than natural language messages. Natural language requires guessing and reasoning. Structured IR allows reading definitions directly, matching against actual offsets, identifying discrepancies with certainty.

Measurements from LLM-IR-Debugging research (updated to 298 sessions, 3,241 calls): IR preference increases 443 percent (6 to 26 percent), diagnosis improves 27 percent, fix accuracy increases 34 percent.

---

## 11.6 Comparison with Other Test Frameworks

TML unique advantages: subprocess isolation with crash recovery, machine-readable NDJSON output, automatic multi-layer IR diagnostics. These features make TML well-suited for LLM-assisted debugging.

| Feature | Rust | Go | Python | TML |
|---------|------|----|----|-----|
| In-process | Yes | Yes | Yes | No |
| Crash isolation | No | No | No | Yes |
| NDJSON output | No | No | No | Yes |
| IR diagnostics | No | No | No | Yes |
| Structured JSON | No | No | No | Yes |

---

## 11.7 Performance

Test Coverage: 1,682 files, 1,659 passing, 0 crashing, 93.2 percent function coverage.

Performance: Full suite 37.2 seconds, avg 500ms overhead plus computation, slowest std::http (4.2s), fastest core::num (150ms).

Tool Usage (3,241 calls, 298 sessions): test 52.7 percent, check 17.5 percent, emit-IR 7.2 percent, debug-layers 9.0 percent.

The 37-second execution time is the primary bottleneck. Phase 0 includes LLVM ORC JIT implementation, projected to reduce execution to 2-3 seconds, improving the edit-test cycle from 42s to 7s.
