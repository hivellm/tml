# TML Test Framework

A comprehensive testing framework for TML with assertions, benchmarking, and code coverage.

**Status**: 2510 tests passing across 235 test files

## Features

- **Decorator-based tests** - Mark tests with `@test`
- **Automatic test discovery** - Files ending in `.test.tml`
- **Module-based assertions** - Import via `use test`
- **Polymorphic assertions** - `assert_eq`, `assert_ne`, `assert_gt`, etc. work with any comparable type
- **Pattern matching support** - Full enum pattern matching in tests
- **Parallel execution** - Multi-threaded test runner
- **Benchmarking** - `@bench` decorator for performance testing
- **Code coverage** - Track function, line, and branch coverage
- **HTML/JSON reports** - Generate coverage reports

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
tml test                    # Run all tests in current directory
tml test lib               # Run tests in lib directory
tml test --coverage        # Run with code coverage
```

## Available Assertions

All assertion functions are available via `use test`. All comparison assertions are **polymorphic** - they work with any type that supports the required comparison operators.

### Basic Assertions

```tml
// Boolean assertion
assert(x > 0, "x must be positive")

// Equality (polymorphic)
assert_eq(result, 42, "result should be 42")
assert_eq(is_valid, true, "should be valid")
assert_eq(name, "Alice", "name should be Alice")

// Inequality (polymorphic)
assert_ne(result, 0, "result should not be zero")

// Comparisons (polymorphic)
assert_gt(score, 50, "score must be above 50")
assert_gte(age, 18, "must be at least 18")
assert_lt(errors, 10, "must have fewer than 10 errors")
assert_lte(count, 100, "must not exceed 100")

// Range check (polymorphic)
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

@test
func test_maybe_matching() {
    let value: Maybe[I32] = Just(42)

    when value {
        Just(n) => assert_eq(n, 42, "should be 42"),
        Nothing => panic("should not be Nothing")
    }
}
```

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

# Coverage options
tml test --coverage             # Enable coverage tracking
tml test --coverage-html        # Generate HTML coverage report
tml test --coverage-json        # Generate JSON coverage report
```

## Test Timeout

Tests have a maximum execution time to prevent infinite loops:

- **Default timeout**: 20 seconds per test
- **Configurable via CLI**: `--timeout=N` where N is seconds
- **Behavior**: If a test exceeds the timeout, it's marked as FAILED with a TIMEOUT message

```bash
tml test --timeout=5            # Set 5 second timeout
tml test --timeout=60           # Set 60 second timeout for slower tests
```

## Benchmarking

Benchmark functions with the `@bench` decorator:

```tml
@bench
func bench_fibonacci() -> Unit {
    var a: I32 = 0
    var b: I32 = 1
    var i: I32 = 0
    loop i < 20 {
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

## Code Coverage

The test framework includes code coverage tracking:

### Coverage Functions

```tml
use test::coverage

// Track function coverage
cover_func("my_function")

// Track line coverage
cover_line("file.tml", 42)

// Track branch coverage
cover_branch("file.tml", 50, 0)  // branch 0 (e.g., if-true)
cover_branch("file.tml", 50, 1)  // branch 1 (e.g., if-false)

// Get coverage stats
let func_count: I32 = get_covered_func_count()
let line_count: I32 = get_covered_line_count()
let branch_count: I32 = get_covered_branch_count()
let percent: I32 = get_coverage_percent()

// Check specific function
let is_covered: Bool = is_func_covered("my_function")

// Reset coverage data
reset_coverage()

// Generate reports
print_coverage_report()
```

### Coverage Reports

```bash
# Generate HTML report
tml test --coverage --coverage-html
# Output: coverage.html

# Generate JSON report
tml test --coverage --coverage-json
# Output: coverage.json
```

### Example Coverage Test

```tml
use test
use test::coverage

func add(a: I32, b: I32) -> I32 {
    cover_func("add")
    return a + b
}

@test
func test_coverage_tracking() {
    reset_coverage()

    assert_eq(get_covered_func_count(), 0, "initially no coverage")

    let result: I32 = add(2, 3)
    assert_eq(result, 5, "add works")

    assert_eq(get_covered_func_count(), 1, "one function covered")
    assert(is_func_covered("add"), "add should be covered")
}
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
│   └── coverage/         # Coverage tracking
└── runtime/
    ├── assertions.c      # C runtime for assertions
    └── coverage.c        # C runtime for coverage
```

## Test Output

```
TML v0.1.0

Running 223 test files...
Grouped into 46 test suites

 + compiler_tests (1094 tests) 202ms
 + lib/core (657 tests) 0ms
 + lib/std (694 tests) 128ms

Tests       2445 passed (2445 tests, 223 files)
Duration    571ms

All tests passed!
```

## Implementation Status

- [x] Test decorator (`@test`)
- [x] Test discovery (*.test.tml files)
- [x] Auto-generated test runner
- [x] Module system (`use test`)
- [x] Polymorphic assertions (`assert_eq[T]`, `assert_ne[T]`, etc.)
- [x] Pattern matching support
- [x] CLI integration (`tml test`)
- [x] Parallel execution (multi-threaded)
- [x] Test filtering by name/pattern
- [x] Benchmarking (`@bench`)
- [x] Test timeout (default 20s, configurable)
- [x] Code coverage (function, line, branch)
- [x] HTML/JSON coverage reports
