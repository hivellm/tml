# TML v1.0 — Testing Framework

## 1. Basic Tests

### 1.1 @test Directive

```tml
@test
func test_addition() {
    assert_eq(2 + 2, 4)
}

@test
func test_string_concat() {
    let result: String = "hello" + " world"
    assert_eq(result, "hello world")
}
```

### 1.2 Assertions

```tml
assert(condition)                    // fails if false
assert_eq(actual, expected)          // equality
assert_ne(actual, expected)          // difference
assert_lt(a, b)                      // a < b
assert_le(a, b)                      // a <= b
assert_gt(a, b)                      // a > b
assert_ge(a, b)                      // a >= b
```

### 1.3 Custom Messages

```tml
@test
func test_with_message() {
    let x: I32 = compute()
    assert(x > 0, "x should be positive, got: " + x.to_string())
    assert_eq(x, 42, "expected 42")
}
```

## 2. Outcome Tests

### 2.1 Expect Panic

```tml
@test
@should_panic
func test_divide_by_zero() {
    divide(10, 0)
}

@test
@should_panic(message = "division by zero")
func test_panic_message() {
    divide(10, 0)
}
```

### 2.2 Expect Error

```tml
@test
func test_file_not_found() -> Outcome[Unit, TestError] {
    let result: Outcome[File, Error] = File.open("nonexistent.txt")
    assert(result.is_err())
    return Ok(unit)
}

@test
@should_error(IoError)
func test_expects_io_error() -> Outcome[Unit, IoError] {
    let _: Outcome[File, Error] = File.open("nonexistent.txt")!
    return Ok(unit)
}
```

## 3. Fixtures and Setup

### 3.1 Before/After

```tml
module tests {
    var test_db: Maybe[Database] = Nothing

    @before_all
    func setup_database() {
        test_db = Just(Database.create_test())
    }

    @after_all
    func cleanup_database() {
        when test_db {
            Just(db) -> db.destroy(),
            Nothing -> unit,
        }
    }

    @before_each
    func reset_tables() {
        test_db.unwrap().reset()
    }

    @test
    func test_insert() {
        let db: Database = test_db.unwrap()
        db.insert("key", "value")
        assert_eq(db.get("key"), Just("value"))
    }
}
```

### 3.2 Test Context

```tml
type TestContext {
    temp_dir: Path,
    config: Config,
}

@fixture
func create_context() -> TestContext {
    return TestContext {
        temp_dir: TempDir.create(),
        config: Config.test_defaults(),
    }
}

@test
func test_with_context(ctx: TestContext) {
    let file: Path = ctx.temp_dir.join("test.txt")
    File.write(file, "data")
    assert(file.exists())
}
```

## 4. Property-Based Testing (FUTURE)

> **Status:** Not yet implemented. This section describes planned features.

### 4.1 @property Directive (Planned)

```tml
@property
func prop_addition_commutative(a: I32, b: I32) -> Bool {
    return a + b == b + a
}
```

### 4.2 Custom Generators (Planned)

```tml
@generator
func gen_positive_int() -> I32 {
    return random_range(1, I32.MAX)
}
```

## 5. Mocking (FUTURE)

> **Status:** Not yet implemented. This section describes planned features.

### 5.1 Mock Behaviors (Planned)

```tml
behavior HttpClient {
    func get(this, url: String) -> Outcome[Response, Error]
}

@mock
type MockHttpClient {}
```

### 5.2 Spy and Verification (Planned)

Spy/verification framework for recording and asserting function calls.

## 6. Benchmarks

### 6.1 @bench Directive

```tml
// Simple benchmark with default 1000 iterations
@bench
func bench_addition() {
    let _x: I32 = 1 + 2 + 3 + 4 + 5
}

// Custom iteration count
@bench(10000)
func bench_loop() {
    var sum: I32 = 0
    var i: I32 = 0
    loop {
        if i >= 100 { break }
        sum = sum + i
        i = i + 1
    }
}
```

### 6.2 Benchmark Files

Benchmark files use the `.bench.tml` extension and are discovered separately from tests:

```
tests/
  math.test.tml          # Unit tests
  benchmarks/
    sorting.bench.tml    # Sorting benchmarks
    parsing.bench.tml    # Parsing benchmarks
```

### 6.3 Run Benchmarks

```bash
# Run all benchmarks (*.bench.tml files)
tml test --bench

# Filter by pattern
tml test --bench sorting

# Save results as baseline
tml test --bench --save-baseline=baseline.json

# Compare against baseline
tml test --bench --compare=baseline.json
```

### 6.4 Benchmark Output

```
 TML Benchmarks v0.1.0

 Running 1 benchmark file...

 + simple
  + bench bench_addition       ... 2 ns/iter (1000 iterations)
  + bench bench_loop           ... 156 ns/iter (10000 iterations)
  + bench bench_multiplication ... 1 ns/iter (5000 iterations)

 Bench Files 1 passed (1)
 Duration    1.23s
```

### 6.5 Baseline Comparison

When comparing against a baseline, improvements show in green and regressions in red:

```
  + bench bench_addition ... 2 ns/iter (-15.2%)   # improved (green)
  + bench bench_loop     ... 180 ns/iter (+10.5%) # regressed (red)
  + bench bench_sort     ... 45 ns/iter (~0.3%)   # unchanged (gray)
```

The baseline file is stored in JSON format:
```json
{
  "benchmarks": [
    { "file": "simple.bench.tml", "name": "bench_addition", "ns_per_iter": 2 },
    { "file": "simple.bench.tml", "name": "bench_loop", "ns_per_iter": 156 }
  ]
}
```

## 7. Coverage

### 7.1 Generate Report

```bash
# Enable coverage tracking
tml test --coverage

# Specify output file
tml test --coverage --coverage-output=coverage.html

# Coverage with specific tests
tml test math --coverage
```

### 7.2 Coverage Runtime

The coverage runtime tracks:
- **Function coverage**: Which functions were executed
- **Line coverage**: Which lines were executed
- **Branch coverage**: Which branches were taken

```
================================================================================
                           CODE COVERAGE REPORT
================================================================================

FUNCTION COVERAGE: 8/10 (80.0%)
--------------------------------------------------------------------------------
  [+] main (hits: 1)
  [+] add (hits: 5)
  [-] unused_func (hits: 0)

LINE COVERAGE: 45/50 (90.0%)
--------------------------------------------------------------------------------

================================================================================
                              SUMMARY
================================================================================
  Functions: 8 covered / 10 total
  Lines:     45 covered / 50 total
  Branches:  12 covered / 15 total
================================================================================
```

### 7.3 Thresholds

```toml
# tml.toml
[test]
coverage-threshold = 80  # minimum 80%
coverage-fail-under = true
```

## 8. Organization

### 8.1 Test Module

```tml
// src/math.tml
module math

pub func add(a: I32, b: I32) -> I32 {
    return a + b
}

// Inline tests (only compiled with test flag)
@when(test)
module tests {
    import super.*

    @test
    func test_add() {
        assert_eq(add(2, 3), 5)
    }
}
```

### 8.2 Separate Test Files

```
src/
  math.tml
tests/
  test_math.tml
```

```tml
// tests/test_math.tml
module test_math

import mylib.math.*

@test
func test_add_external() {
    assert_eq(add(10, 20), 30)
}
```

## 9. Commands

```bash
# Run all
tml test

# Filter by file path substring
tml test test_add
tml test "str"

# Filter by suite/module
tml test --suite=core/str
tml test --suite=std/json

# List available suites
tml test --list-suites

# Verbose
tml test --verbose

# Profile (per-suite timing)
tml test --profile

# Timeout (seconds per suite, default: 300)
tml test --timeout=30s

# Stop on first failure
tml test --fail-fast

# Skip test cache
tml test --no-cache

# Output format
tml test --output=junit:results.xml
tml test --output=json:results.ndjson
```

## 10. Output

```
 TML Tests

 core/str
   ✓ basic.test.tml              (23ms)
   ✓ slice.test.tml              (18ms)
   ✓ split.test.tml              (14ms)

 Tests  253 passed | 0 failed
 Suites  21 passed | 0 failed | 0 cached
 Duration 1.2s
```

## 11. Test System Architecture

### 11.1 Subprocess Model (Go-Inspired)

The TML test runner uses a **subprocess model** where each test suite compiles to a
standalone EXE and runs as an isolated child process. Results stream back to the
coordinator via NDJSON (newline-delimited JSON) on stdout.

```
Coordinator (tml.exe)
  │
  ├── Discovery: find all *.test.tml files
  ├── Grouping:  pack N files into suites (default max=8 per suite)
  │
  ├── Compilation phase (parallel, std::thread pool)
  │     ├── Suite 1 → test_suite_abc123.exe
  │     ├── Suite 2 → test_suite_def456.exe
  │     └── Suite N → ...
  │
  └── Execution phase (parallel, async subprocess polling)
        ├── Launch Suite 1 subprocess → read NDJSON events
        ├── Launch Suite 2 subprocess → read NDJSON events
        └── Aggregate results → terminal/JSON/JUnit output
```

**Benefits over the previous in-process DLL model:**
- **Crash isolation**: a crashing test doesn't kill the coordinator
- **No coverage hangs**: each subprocess writes coverage via `TML_COVERAGE_FILE` env var
- **True parallelism**: subprocesses run concurrently with non-blocking poll loop
- **Timeout enforcement**: `Process::wait(timeout_ms)` per suite

### 11.2 NDJSON Protocol

Each test suite EXE emits one JSON object per line on stdout:

```json
{"event":"suite_start","suite":"core/str/basic.test.tml","test_count":12}
{"event":"test_start","index":0,"name":"test_split_basic","file":"lib/core/tests/str/basic.test.tml"}
{"event":"test_pass","index":0,"duration_us":0}
{"event":"test_start","index":1,"name":"test_split_empty","file":"lib/core/tests/str/basic.test.tml"}
{"event":"test_fail","index":1,"name":"test_split_empty","error":"assertion failed at :7: expected [] but got [\"\"]","file":"lib/core/tests/str/basic.test.tml","line":0,"exit_code":-1,"duration_us":1203}
{"event":"suite_end","passed":11,"failed":1,"duration_ms":23}
```

Events: `suite_start`, `test_start`, `test_pass`, `test_fail`, `test_crash`,
`test_timeout`, `test_skip`, `coverage`, `suite_end`

**Failure contract (phase44a, v0.3.83).** A test body that panics MUST emit
`test_fail` and the process MUST exit non-zero — identically in standalone
(one EXE per file) and suite-packed modes. The generated test-entry wrapper
records the first non-zero result from `tml_run_test_with_catch` and returns
it; a discarded result is what produced the pre-v0.3.83 false-pass, where a
panicking body printed `panic: …` to stderr and was still reported
`test_pass`.

`test_fail` carries `error` — the actual panic message, via
`tml_test_error_json()` — not a generic "non-zero exit" string. `@should_panic`
bodies run through the same catch path with `quiet=1`, so an expected panic
emits no `FATAL [runtime] panic:` line and is not misread as a crash by the
coordinator's stderr scan.

All string fields (`name`, `file`, `error`, suite name) are JSON-escaped.
Before v0.3.83 they were not, which made the stream invalid JSON for any
Windows path (unescaped `\`). Consumers that parsed it leniently now receive
strictly conformant input.

### 11.3 Test Cache (Go Model)

Suite results are cached using CRC32C content hashing:

- **Cache key**: CRC32C of all `.tml` source files in the suite (sorted) + compiler binary hash + flags hash
- **Cache pass rule**: only suites where ALL tests pass are stored (Go model)
- **Cache location**: `build/debug/.new-test-cache.json`
- **Cold run**: full compilation + execution (~5–6 minutes for full suite)
- **Warm run**: cache hit skips both compilation and execution (~65ms for full suite — 107x speedup)
- **Bypass**: `tml test --no-cache` forces full recompilation

### 11.4 Coverage System

Coverage uses **function-level instrumentation** (not LLVM profiling):

- The compiler injects calls to `tml_cover_func(module_name, func_name)` at function entry
- Each test suite subprocess writes its coverage data to a temp file via `TML_COVERAGE_FILE` env var
- The coordinator reads all temp files after execution and aggregates into a merged set
- Full suite coverage generates three reports:
  - `build/coverage/coverage.html` — interactive HTML with 5 tabs (Overview, Module Coverage, Priorities, Uncovered, Test Suites)
  - `build/coverage/coverage.json` — machine-readable JSON for CI
  - `build/coverage/coverage.jsonl` — NDJSON for streaming parsers
- Partial runs (`--suite` or path filter) only show console output — no HTML/JSON saved
- **Zero guard**: zero covered functions = fatal error, no reports written
- **Regression guard**: new run must not drop below previous coverage percentage

### 11.5 Reporter System

The reporter system uses a Catch2-inspired multi-reporter pattern:

- **MultiReporter**: broadcasts all events to all registered reporters simultaneously
- **TerminalReporter**: colored vitest-style output, ANSI cross-platform (Win: `SetConsoleMode`, detect `isatty`/`WT_SESSION`)
- **JsonReporter**: NDJSON to file, one JSON object per event
- **JunitXmlReporter**: JUnit-compatible XML for CI systems (GitHub Actions, Jenkins)

Multiple reporters can run simultaneously:
```bash
tml test --output=terminal --output=junit:ci.xml --output=json:results.ndjson
```

---

*Previous: [09-CLI.md](./09-CLI.md)*
*Next: [11-DEBUG.md](./11-DEBUG.md) — Debug System*
