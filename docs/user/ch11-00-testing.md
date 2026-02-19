# Testing

TML provides a built-in testing framework through the `test` package. The framework includes assertions, test runners, benchmarking utilities, and reporting tools.

## Writing Tests

### Basic Test Structure

Tests are written using the `@test` decorator:

```tml
@test
func test_addition() {
    let result = 2 + 2
    assert_eq(result, 4)
}

@test
func test_subtraction() {
    let result = 5 - 3
    assert_eq(result, 2)
}
```

### Running Tests

Run all tests in a file or project:

```bash
tml test                    # Run all tests
tml test file.tml          # Run tests in specific file
tml test --verbose         # Show detailed output
tml test --filter add      # Run tests matching "add"
tml test --suite=core/str  # Run all tests in a module
tml test --list-suites     # Show available suite groups
```

## Assertions

The `test::assertions` module provides assertion functions:

### Basic Assertions

```tml
use test::assertions::*

@test
func test_basic_assertions() {
    // Assert condition is true
    assert(true)
    assert_msg(true, "custom failure message")

    // Assert equality
    assert_eq(2 + 2, 4)
    assert_eq_msg(2 + 2, 4, "math is broken")

    // Assert inequality
    assert_ne(5, 3)
    assert_ne_msg(5, 3, "values should differ")
}
```

### Comparison Assertions

```tml
@test
func test_comparisons() {
    // Greater than
    assert_gt(5, 3)
    assert_gt_msg(10, 5, "10 should be greater than 5")

    // Greater or equal
    assert_ge(5, 5)
    assert_ge(6, 5)

    // Less than
    assert_lt(3, 5)
    assert_lt_msg(1, 10, "1 should be less than 10")

    // Less or equal
    assert_le(5, 5)
    assert_le(3, 5)
}
```

### Testing Optional Values

```tml
use std::option::{Maybe, Just, Nothing}

@test
func test_maybe() {
    let value: Maybe[I32] = Just(42)

    // Check if Some/Nothing
    assert(value.is_some())
    assert(not value.is_none())

    // Unwrap and check value
    assert_eq(value.unwrap(), 42)
}

@test
func test_nothing() {
    let value: Maybe[I32] = Nothing

    assert(value.is_none())
    assert_eq(value.unwrap_or(0), 0)
}
```

### Testing Results

```tml
use std::result::{Outcome, Ok, Err}

@test
func test_success() {
    let result: Outcome[I32, Str] = Ok(42)

    assert(result.is_ok())
    assert_eq(result.unwrap(), 42)
}

@test
func test_error() {
    let result: Outcome[I32, Str] = Err("failed")

    assert(result.is_err())
    assert_eq(result.unwrap_or(0), 0)
}
```

## Test Organization

### Grouping Tests

Organize tests in modules:

```tml
// tests/math_tests.tml
mod math_tests {
    use test::assertions::*

    @test
    func test_add() {
        assert_eq(add(2, 2), 4)
    }

    @test
    func test_multiply() {
        assert_eq(multiply(3, 4), 12)
    }
}
```

### Setup and Teardown

Use regular functions for test setup:

```tml
func setup_database() -> Database {
    let db = Database::new()
    db.init()
    db
}

func teardown_database(db: Database) {
    db.cleanup()
    db.close()
}

@test
func test_database_query() {
    let db = setup_database()

    let result = db.query("SELECT * FROM users")
    assert_eq(result.len(), 10)

    teardown_database(db)
}
```

## Benchmarking

### Basic Benchmarks

Use `@bench` decorator for performance tests:

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

@bench(5000)
func bench_multiplication() {
    let _x: I64 = 123456 * 789012
}
```

### Benchmark Files

Benchmark files use the `.bench.tml` extension:

```
project/
├── src/
│   └── lib.tml
├── tests/
│   ├── unit.test.tml        # Unit tests
│   └── benchmarks/
│       ├── sorting.bench.tml
│       └── parsing.bench.tml
└── tml.toml
```

### Benchmark Configuration

The `@bench` decorator accepts an optional iteration count:

```tml
@bench           // Default: 1000 iterations
@bench(100)      // Run 100 iterations
@bench(10000)    // Run 10,000 iterations
```

Each benchmark automatically includes:
- 10 warmup iterations (not measured)
- Nanosecond-precision timing
- Per-iteration timing calculation

### Manual Benchmarking

For more control, use the timing API directly:

```tml
use std::time::{Instant, Duration}

@test
func benchmark_manual() {
    let runs: I64 = 10
    let mut total_us: I64 = 0

    for _ in 0 to runs {
        let start: I64 = Instant::now()
        expensive_function()
        let elapsed: I64 = Instant::elapsed(start)
        total_us += elapsed
    }

    let avg_ms: F64 = Duration::as_millis_f64(total_us / runs)
    println("Average: {:.3} ms ({} runs)", avg_ms, runs)

    // Assert performance requirement
    assert_lt(avg_ms, 100.0)  // Must complete in < 100ms
}
```

## Test Patterns

### Testing Error Cases

```tml
@test
func test_division_by_zero() {
    let result = safe_divide(10, 0)

    when result {
        Ok(_) => panic("Should have returned error"),
        Err(e) => assert_eq(e, "Division by zero"),
    }
}
```

### Testing Panics

```tml
@test
@should_panic
func test_panic_on_invalid_input() {
    validate_input(-1)  // Should panic
}

@test
@should_panic(expected = "Invalid input")
func test_panic_message() {
    validate_input(-1)  // Should panic with specific message
}
```

### Property-Based Testing

```tml
@test
func test_addition_commutative() {
    // Test that a + b == b + a for various inputs
    for a in -100 to 100 {
        for b in -100 to 100 {
            assert_eq(a + b, b + a)
        }
    }
}

@test
func test_addition_associative() {
    for a in -10 to 10 {
        for b in -10 to 10 {
            for c in -10 to 10 {
                assert_eq((a + b) + c, a + (b + c))
            }
        }
    }
}
```

## Test Output

### Standard Output

```
$ tml test

Running tests...

test test_addition ... ok
test test_subtraction ... ok
test test_multiplication ... ok
test test_division ... ok

test result: ok. 4 passed; 0 failed

Finished in 0.123s
```

### Verbose Output

```
$ tml test --verbose

Running tests...

test test_addition
  Time: 0.001ms
  Result: ok

test test_subtraction
  Time: 0.001ms
  Result: ok

test result: ok. 2 passed; 0 failed; 0 ignored

Finished in 0.123s
```

### Failure Output

```
test test_division ... FAILED

failures:

---- test_division ----
assertion failed: left != right
  left: 5
  right: 6
  at tests/math.tml:42

test result: FAILED. 3 passed; 1 failed
```

## Test Configuration

### Ignoring Tests

```tml
@test
@ignore
func test_not_ready_yet() {
    // This test will be skipped
    unimplemented()
}

@test
@ignore(reason = "waiting for API fix")
func test_blocked() {
    api_call()
}
```

### Conditional Tests

```tml
@test
@cfg(target_os = "linux")
func test_linux_only() {
    // Only runs on Linux
}

@test
@cfg(debug)
func test_debug_mode() {
    // Only runs in debug builds
}
```

## Integration Tests

### Test Directory Structure

```
project/
├── src/
│   └── lib.tml
├── tests/
│   ├── integration_test.tml
│   ├── api_test.tml
│   └── end_to_end_test.tml
└── tml.toml
```

### Integration Test Example

```tml
// tests/integration_test.tml
use mylib::*
use test::assertions::*

@test
func test_full_workflow() {
    // Setup
    let app = App::new()
    app.init()

    // Execute workflow
    let result = app.process_request(Request {
        method: "GET",
        path: "/users",
    })

    // Verify
    assert(result.is_ok())
    assert_eq(result.unwrap().status, 200)

    // Cleanup
    app.shutdown()
}
```

## Best Practices

### 1. One Assertion Per Test

```tml
// ❌ Bad: Multiple unrelated assertions
@test
func test_everything() {
    assert_eq(add(2, 2), 4)
    assert_eq(multiply(3, 3), 9)
    assert_eq(divide(10, 2), 5)
}

// ✅ Good: Focused tests
@test
func test_add() {
    assert_eq(add(2, 2), 4)
}

@test
func test_multiply() {
    assert_eq(multiply(3, 3), 9)
}
```

### 2. Descriptive Test Names

```tml
// ❌ Bad: Vague name
@test
func test1() { ... }

// ✅ Good: Describes what is tested
@test
func test_division_by_zero_returns_error() { ... }
```

### 3. Test Edge Cases

```tml
@test
func test_empty_list() {
    let list: Vec[I32] = Vec::new()
    assert_eq(list.len(), 0)
    assert(list.is_empty())
}

@test
func test_single_element() {
    let mut list = Vec::new()
    list.push(1)
    assert_eq(list.len(), 1)
}

@test
func test_boundary_values() {
    assert_eq(safe_divide(I64::MAX, 1), Ok(I64::MAX))
    assert_eq(safe_divide(I64::MIN, 1), Ok(I64::MIN))
}
```

### 4. Use Meaningful Assertions

```tml
// ❌ Bad: Generic assertion
@test
func test_result() {
    let result = compute()
    assert(result == 42)
}

// ✅ Good: Specific assertion with context
@test
func test_compute_returns_expected_value() {
    let result = compute()
    assert_eq_msg(result, 42, "compute() should return the answer")
}
```

### 5. Benchmark Realistically

```tml
@bench
func bench_realistic_workload(b: Bencher) {
    // Setup realistic data
    let data = generate_test_data(1000)

    b.iter(|| {
        // Black box to prevent optimization
        black_box(process_data(black_box(&data)))
    })
}
```

## Running Benchmarks

```bash
# Run all benchmarks (discovers *.bench.tml files)
tml test --bench

# Filter by pattern
tml test --bench sorting

# Save results as baseline for comparison
tml test --bench --save-baseline=baseline.json

# Compare against previous baseline
tml test --bench --compare=baseline.json
```

### Benchmark Output

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

### Baseline Comparison

When comparing against a baseline:
- Improvements (faster) show in green with negative percentage
- Regressions (slower) show in red with positive percentage
- Changes within ±5% show in gray

```
  + bench bench_addition ... 2 ns/iter (-15.2%)   # improved
  + bench bench_loop     ... 180 ns/iter (+10.5%) # regressed
  + bench bench_sort     ... 45 ns/iter (~0.3%)   # unchanged
```

## Fuzz Testing

TML includes a built-in fuzzer for finding bugs through random input generation.

### Basic Fuzz Targets

Use the `@fuzz` decorator to mark functions as fuzz targets:

```tml
// parser.fuzz.tml
use test

// Fuzz target with input data
@fuzz
func fuzz_parser(data: Ptr[U8], len: U64) {
    // Convert input bytes to string or other format
    if len == 0 { return }

    // Use the fuzzed input to test your code
    let result = parse_input(data, len)

    // Assertions will catch bugs
    assert(result.is_valid())
}
```

### Fuzz File Naming

Fuzz files use the `.fuzz.tml` extension:

```
project/
├── src/
│   └── lib.tml
├── tests/
│   └── fuzz/
│       ├── parser.fuzz.tml
│       ├── serializer.fuzz.tml
│       └── validator.fuzz.tml
└── tml.toml
```

### Running Fuzz Tests

```bash
# Run all fuzz targets (discovers *.fuzz.tml files)
tml test --fuzz

# Run for specific duration (default: 10 seconds per target)
tml test --fuzz --fuzz-duration=60

# Filter by pattern
tml test --fuzz parser

# Specify maximum input length
tml test --fuzz --fuzz-max-len=1024
```

### Using a Corpus

The fuzzer can use a corpus of input files to guide fuzzing:

```bash
# Directory structure
project/
├── fuzz_corpus/
│   └── parser/           # Corpus for parser.fuzz.tml
│       ├── seed1.bin
│       ├── seed2.bin
│       └── seed3.bin
└── tests/
    └── fuzz/
        └── parser.fuzz.tml

# Run with corpus
tml test --fuzz --corpus-dir=fuzz_corpus/parser
```

### Crash Handling

When a crash is found:
1. The crashing input is saved to `fuzz_crashes/`
2. The file is named `{target_name}_{timestamp}.crash`
3. You can reproduce the crash by replaying the input

```bash
# View saved crashes
ls fuzz_crashes/

# Reproduce a crash
tml run tests/fuzz/parser.fuzz.tml < fuzz_crashes/parser_1735689600.crash
```

### Fuzz Target Requirements

Fuzz targets should:
1. Be marked with `@fuzz` decorator
2. Accept `(data: Ptr[U8], len: U64)` parameters (optional)
3. Return `void` (Unit) or `I32` (0 for success, non-zero for failure)
4. Avoid infinite loops
5. Use assertions to detect bugs

```tml
@fuzz
func fuzz_with_assertions(data: Ptr[U8], len: U64) {
    let result = process(data, len)

    // Catch invariant violations
    assert(result.len() <= len)
    assert(result.is_valid())
}
```

### Mutation Strategies

The fuzzer uses several mutation strategies:
- **Bit flipping**: Flip random bits in the input
- **Byte replacement**: Replace random bytes
- **Insertion**: Insert random bytes
- **Deletion**: Remove random bytes
- **Swapping**: Swap byte positions
- **Duplication**: Duplicate sections of input

70% of iterations mutate from the corpus, 30% generate new random input.

## Test Coverage

```bash
# Enable coverage tracking
tml test --coverage

# Specify output file
tml test --coverage --coverage-output=coverage.html

# Coverage with specific tests
tml test math --coverage
```

The coverage report shows:
- Function coverage: which functions were executed
- Line coverage: which lines were hit
- Branch coverage: which branches were taken

## See Also

- [Chapter 10 - Standard Library](ch10-00-standard-library.md)
- [Appendix C - Builtin Functions](appendix-03-builtins.md)
- [Error Handling with Outcome](ch10-00-standard-library.md#outcomet-e---error-handling)
