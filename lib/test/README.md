# TML Test Framework

A lightweight, built-in testing framework for TML.

## Features

- **Decorator-based tests** - Mark tests with `@test`
- **Automatic test discovery** - Files ending in `.test.tml`
- **Module-based assertions** - Import via `use test`
- **Polymorphic assertions** - `assert_eq`, `assert_ne`, `assert_gt`, etc. work with any comparable type
- **Pattern matching support** - Full enum pattern matching in tests
- **Auto-generated test runner** - Tests are void functions, fail via assertions

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

> **Note**: Test functions don't need a return type. They pass if they complete
> without triggering an assertion failure. Failed assertions call `panic()` which
> exits with code 1.

Run tests:

```bash
tml test                    # Run all tests in current directory
```

## Available Assertions

All assertion functions are available via `use test`. All comparison assertions are **polymorphic** - they work with any type that supports the required comparison operators.

### `assert(condition: Bool, message: Str)`
Basic boolean assertion:
```tml
assert(x > 0, "x must be positive")
```

### `assert_eq[T](left: T, right: T, message: Str)`
Assert equality (polymorphic - works with any type):
```tml
assert_eq(result, 42, "result should be 42")
assert_eq(is_valid, true, "should be valid")
assert_eq(name, "Alice", "name should be Alice")
```

### `assert_ne[T](left: T, right: T, message: Str)`
Assert inequality (polymorphic):
```tml
assert_ne(result, 0, "result should not be zero")
```

### `assert_gt[T](left: T, right: T, message: Str)`
Assert greater than (polymorphic):
```tml
assert_gt(score, 50, "score must be above 50")
```

### `assert_gte[T](left: T, right: T, message: Str)`
Assert greater than or equal (polymorphic):
```tml
assert_gte(age, 18, "must be at least 18")
```

### `assert_lt[T](left: T, right: T, message: Str)`
Assert less than (polymorphic):
```tml
assert_lt(errors, 10, "must have fewer than 10 errors")
```

### `assert_lte[T](left: T, right: T, message: Str)`
Assert less than or equal (polymorphic):
```tml
assert_lte(count, 100, "must not exceed 100")
```

### `assert_in_range[T](value: T, min: T, max: T, message: Str)`
Assert value is within range [min, max] (polymorphic):
```tml
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

## Pattern Matching in Tests

Full enum pattern matching is supported:

```tml
use test

type Color {
    Red,
    Green,
    Blue
}

@test
func test_color_matching() {
    let color: Color = Color::Red

    let is_red: Bool = when color {
        Color::Red => true,
        _ => false
    }

    assert_eq(is_red, true, "color should be Red")
}
```

## Module System

Tests must explicitly import the test module:

```tml
use test  // Required at top of file

@test
func my_test() {
    assert_eq(1, 1, "test")
}
```

Without `use test`, assertion functions will not be available.

## CLI Options

```bash
tml test                        # Run all tests (auto-detect threads)
tml test --test-threads=4       # Run with 4 threads
tml test --test-threads=1       # Single-threaded mode
tml test --timeout=30           # Set test timeout to 30 seconds (default: 20s)
tml test basics                 # Filter by test name
tml test --group=compiler       # Filter by directory
tml test --verbose              # Verbose output (single-threaded)
tml test --quiet                # Minimal output
```

## Test Timeout

Tests have a maximum execution time to prevent infinite loops from blocking the test suite:

- **Default timeout**: 20 seconds per test
- **Configurable via CLI**: `--timeout=N` where N is seconds
- **Behavior**: If a test exceeds the timeout, it's marked as FAILED with a TIMEOUT message
- **Use case**: Prevents tests with infinite loops or deadlocks from hanging forever

Example:
```bash
tml test --timeout=5            # Set 5 second timeout for all tests
tml test --timeout=60           # Set 60 second timeout for slower tests
```

## Benchmarking

Benchmark functions with the `@bench` decorator to measure performance:

```tml
@bench
func bench_fibonacci() -> Unit {
    let mut a: I32 = 0
    let mut b: I32 = 1
    let mut i: I32 = 0
    loop (i < 20) {
        let temp: I32 = a + b
        a = b
        b = temp
        i = i + 1
    }
}
```

**How benchmarks work:**
- Each `@bench` function is executed 1000 times automatically
- Total time is measured using `tml_time_us()` (microseconds)
- Average time per iteration is calculated
- No manual timing code needed

**Running benchmarks:**
```bash
tml run bench_file.tml     # Run all benchmarks in file
```

**Benchmark requirements:**
- Function must be decorated with `@bench`
- Must return `Unit` (no return value)
- Should perform meaningful work in the function body

## Implementation Status

- [x] Test decorator (`@test`)
- [x] Test discovery (*.test.tml files)
- [x] Auto-generated test runner
- [x] Module system (`use test`)
- [x] **Polymorphic assertions** (`assert_eq[T]`, `assert_ne[T]`, etc.)
- [x] Pattern matching support
- [x] CLI integration (`tml test`)
- [x] Parallel execution (multi-threaded)
- [x] Test filtering by name/pattern
- [x] Benchmarking (`@bench`)
- [x] Test timeout (default 20s, configurable)

## Test Results

Current test suite: 34/34 tests passing (100%)
- All compiler tests: PASSED
- All test framework examples: PASSED
