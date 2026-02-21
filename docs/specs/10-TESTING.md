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
    let mut sum: I32 = 0
    let mut i: I32 = 0
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

public func add(a: I32, b: I32) -> I32 {
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

# Filter by name
tml test test_add
tml test "test_*"

# Filter by module
tml test --module math

# Verbose
tml test --verbose

# List only
tml test --list

# Timeout
tml test --timeout 30s

# Retry on failure
tml test --retries 3

# Report
tml test --format junit --output results.xml
```

## 10. Output

```
   Running tests for myproject v1.0.0

running 24 tests
test math::test_add ........................ ok (0.1ms)
test math::test_subtract ................... ok (0.1ms)
test math::prop_commutative [1000 samples] . ok (42ms)
test io::test_file_read .................... ok (5ms)
test io::test_file_write ................... ok (3ms)

test result: ok. 24 passed; 0 failed; 0 skipped
   finished in 0.45s

   Coverage: 87.3% (threshold: 80%)
```

---

*Previous: [09-CLI.md](./09-CLI.md)*
*Next: [11-DEBUG.md](./11-DEBUG.md) — Debug System*
