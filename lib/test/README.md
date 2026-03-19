# TML Test Framework

A comprehensive testing framework for TML with assertions, benchmarking, and code coverage.

**Status**: 1550+ tests passing across all libraries | [Changelog](CHANGELOG.md)

## Architecture

The test system uses a **subprocess-based architecture** (inspired by Go's test runner):

1. Each test suite compiles to a standalone **EXE**
2. The coordinator launches each suite as a **subprocess**
3. Results stream via **NDJSON protocol** from subprocess to coordinator
4. Coverage tracked via **`TML_COVERAGE_FILE`** env var — no LLVM profiling, no hangs

```
CLI (tml test) → TestConfig → testing_coordinator
  │
  ├─ discover_tests() → find *.test.tml files
  ├─ group_into_suites() → group by directory
  ├─ compile_suites_parallel() → QueryContext pipeline → .exe per suite
  ├─ Process::launch() → subprocess per suite
  ├─ parse_json_event() → NDJSON protocol (test results streamed)
  └─ print_coverage_report()
```

## Quick Start

Create a test file `math.test.tml`:

```tml
use test

@test
func test_addition() {
    let result: I32 = 2 + 2
    assert_eq(result, 4, "2 + 2 should equal 4")
}

@test
func test_subtraction() {
    let result: I32 = 5 - 3
    assert_eq(result, 2, "5 - 3 should equal 2")
}
```

Run tests:

```bash
tml test                    # Run all tests
tml test --suite=core/str   # Run core::str suite only
tml test --coverage         # Run with code coverage
```

## Available Assertions

All assertion functions are available via `use test`. All comparison assertions are **polymorphic** — they work with any type that supports the required comparison operators.

### Basic Assertions

```tml
assert(x > 0, "x must be positive")
assert_eq(result, 42, "result should be 42")
assert_ne(result, 0, "result should not be zero")
assert_gt(score, 50, "score must be above 50")
assert_gte(age, 18, "must be at least 18")
assert_lt(errors, 10, "must have fewer than 10 errors")
assert_lte(count, 100, "must not exceed 100")
assert_in_range(score, 0, 100, "score must be 0-100")
```

### Boolean Assertions

```tml
assert_true(value, "should be true")
assert_false(value, "should be false")
```

### String Assertions

```tml
assert_str_len(s, 5, "string should have length 5")
assert_str_empty(s, "string should be empty")
assert_str_not_empty(s, "string should not be empty")
```

## CLI Options

```bash
tml test                        # Run all tests (auto-detect threads)
tml test --suite=core/str       # Run specific suite
tml test --test-threads=4       # Run with 4 threads
tml test --test-threads=1       # Single-threaded mode
tml test --timeout=30           # Set test timeout (default: 20s)
tml test basics                 # Filter by test name
tml test --verbose              # Verbose output
tml test --quiet                # Minimal output
tml test --list-suites          # List available suites
tml test --no-cache             # Skip cache, recompile everything

# Coverage options
tml test --coverage             # Enable coverage tracking
tml test --coverage-html        # Generate HTML coverage report
tml test --coverage-json        # Generate JSON coverage report
```

## Suite-Level Filtering

Target specific modules for faster iteration:

```bash
tml test --suite=core/str       # lib/core/tests/str/
tml test --suite=std/json       # lib/std/tests/json/
tml test --suite=std/collections  # lib/std/tests/collections/
```

## Benchmarking

Benchmark functions with the `@bench` decorator:

```tml
@bench
func bench_fibonacci() -> Unit {
    var a: I32 = 0
    var b: I32 = 1
    loop (var i: I32 < 20) {
        let temp: I32 = a + b
        a = b
        b = temp
        i = i + 1
    }
}
```

Each `@bench` function is executed 1000 times. Total and average time are reported.

## Code Coverage

Coverage is tracked via `TML_COVERAGE_FILE` environment variable. No LLVM profiling — no hangs.

```bash
tml test --coverage             # Track coverage
tml test --coverage-html        # HTML report
tml test --coverage-json        # JSON report
```

Coverage tracks function-level execution across all test suites. Incremental saves ensure data survives mid-run crashes.

## Test Output

```
TML v0.2.0

Running 235 test files...
Grouped into 46 test suites

 + compiler_tests (210 tests) 202ms
 + lib/core (830 tests) 45ms
 + lib/std (510 tests) 128ms

Tests       1550 passed (1550 tests, 235 files)
Duration    571ms

All tests passed!
```

## NDJSON Protocol

Test suites communicate results via newline-delimited JSON events:

```json
{"event":"test_start","name":"test_addition","suite":"math"}
{"event":"test_pass","name":"test_addition","duration_ms":1}
{"event":"test_start","name":"test_subtraction","suite":"math"}
{"event":"test_pass","name":"test_subtraction","duration_ms":0}
{"event":"suite_complete","total":2,"passed":2,"failed":0}
```

## Module Structure

```
lib/test/
├── src/
│   ├── mod.tml           # Main module (exports all submodules)
│   ├── types.tml         # Test result types
│   ├── assertions/       # Assertion functions
│   ├── runner/           # Test runner
│   ├── bench/            # Benchmark support
│   ├── report/           # Report generation
│   ├── coverage/         # Coverage tracking
│   ├── e2e/              # End-to-end test utilities
│   └── mock.tml          # Mocking support
│   └── property.tml      # Property-based testing
├── runtime/
│   └── coverage.c        # Coverage tracking (lock-free atomics)
└── README.md

compiler/src/testing/     # Test coordinator (C++)
├── testing_coordinator.cpp
├── testing_discovery.cpp
├── testing_coverage.cpp
├── testing_process.cpp
├── testing_protocol.cpp
└── testing_cache.cpp
```

## Implementation Status

- [x] Test decorator (`@test`)
- [x] Test discovery (`*.test.tml` files)
- [x] Subprocess-based execution (EXE per suite)
- [x] NDJSON protocol for result streaming
- [x] Module system (`use test`)
- [x] Polymorphic assertions
- [x] Pattern matching support
- [x] CLI integration (`tml test`)
- [x] Parallel execution (multi-threaded)
- [x] Suite-level filtering (`--suite=core/str`)
- [x] Test filtering by name/pattern
- [x] Benchmarking (`@bench`)
- [x] Test timeout (default 20s, configurable)
- [x] Code coverage (function-level, no LLVM profiling)
- [x] HTML/JSON coverage reports
- [x] Incremental coverage saves (crash-resilient)
- [x] Test runtime archive (`.lib` for faster linking)
- [x] Object cache (SHA-256 based, skip backend for unchanged suites)
