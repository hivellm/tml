# TML Test Framework

A lightweight, built-in testing framework for TML.

## Features

- **Decorator-based tests** - Mark tests with `@test`
- **Automatic test discovery** - Files ending in `.test.tml`
- **Module-based assertions** - Import via `use test`
- **Type-specific assertions** - `assert_eq_i32`, `assert_eq_bool`, `assert_eq_str`, etc.
- **Pattern matching support** - Full enum pattern matching in tests
- **Auto-generated test runner** - Tests return I32 (0 = success)

## Quick Start

Create a test file `math.test.tml`:

```tml
use test

@test
func test_addition() -> I32 {
    let result: I32 = 2 + 2
    assert_eq_i32(result, 4, "2 + 2 should equal 4")
    return 0
}

@test
func test_subtraction() -> I32 {
    let result: I32 = 5 - 3
    assert_eq_i32(result, 2, "5 - 3 should equal 2")
    return 0
}
```

Run tests:

```bash
tml test                    # Run all tests in current directory
```

## Available Assertions

All assertion functions are available via `use test`:

### `assert(condition: Bool, message: Str)`
Basic boolean assertion:
```tml
assert(x > 0, "x must be positive")
```

### `assert_eq_i32(left: I32, right: I32, message: Str)`
Assert I32 equality:
```tml
assert_eq_i32(result, 42, "result should be 42")
```

### `assert_ne_i32(left: I32, right: I32, message: Str)`
Assert I32 inequality:
```tml
assert_ne_i32(result, 0, "result should not be zero")
```

### `assert_eq_bool(left: Bool, right: Bool, message: Str)`
Assert boolean equality:
```tml
assert_eq_bool(is_valid, true, "should be valid")
```

### `assert_eq_str(left: Str, right: Str, message: Str)`
Assert string equality:
```tml
assert_eq_str(name, "Alice", "name should be Alice")
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
func test_color_matching() -> I32 {
    let color: Color = Color::Red
    
    let is_red: Bool = when color {
        Color::Red => true,
        _ => false
    }
    
    assert_eq_bool(is_red, true, "color should be Red")
    return 0
}
```

## Module System

Tests must explicitly import the test module:

```tml
use test  // Required at top of file

@test
func my_test() -> I32 {
    assert_eq_i32(1, 1, "test")
    return 0
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
    loop {
        if i >= 20 then break
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
- [x] Type-specific assertions (I32, Bool, Str)
- [x] Pattern matching support
- [x] CLI integration (`tml test`)
- [x] Parallel execution (multi-threaded)
- [x] Test filtering by name/pattern
- [x] Benchmarking (`@bench`)

## Test Results

Current test suite: 9/10 tests passing (90%)
- All compiler tests: PASSED
- All runtime tests except collections: PASSED
- Known issue: collections.test.tml (runtime bug)
