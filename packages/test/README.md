# TML Test Framework

A lightweight, built-in testing framework for TML inspired by Rust's test system.

## Features

- **Decorator-based tests** - Mark tests with `@test`
- **Automatic test discovery** - Files ending in `.test.tml`
- **Rich assertions** - `assert!`, `assert_eq!`, `assert_ne!`
- **Test filtering** - Run specific tests or patterns
- **Parallel execution** - Tests run concurrently by default
- **Benchmarking** - Performance testing with `@bench`
- **Setup/teardown** - Per-test and per-module hooks

## Quick Start

Create a test file `math.test.tml`:

```tml
use std.test

@test
func test_addition() {
    assert_eq!(2 + 2, 4)
}

@test
func test_subtraction() {
    assert_eq!(5 - 3, 2)
}

@test(name: "Division by zero panics")
@should_panic
func test_division_panic() {
    let x: I32 = 1 / 0
}
```

Run tests:

```bash
tml test                    # Run all tests
tml test math              # Run tests in math.test.tml
tml test test_addition     # Run specific test
tml test --nocapture       # Show print output
```

## Test Decorators

### `@test`
Mark a function as a test:

```tml
@test
func test_name() {
    // Test code
}
```

### `@test(name: "...")`
Named test with custom display name:

```tml
@test(name: "User creation with valid email")
func test_user_creation() {
    // Test code
}
```

### `@should_panic`
Expect test to panic:

```tml
@test
@should_panic
func test_overflow() {
    let x: I32 = I32::MAX + 1
}
```

### `@should_panic(expected: "...")`
Expect panic with specific message:

```tml
@test
@should_panic(expected: "division by zero")
func test_divide_zero() {
    let _: I32 = 1 / 0
}
```

### `@ignore`
Skip test:

```tml
@test
@ignore
func slow_test() {
    // This won't run by default
}
```

### `@bench`
Benchmark test (requires `--bench` flag):

```tml
@bench
func bench_sort() {
    let arr: [I32] = [10, 5, 8, 2, 7]
    arr.sort()
}
```

## Assertions

### `assert!(condition)`
Assert condition is true:

```tml
assert!(x > 0)
assert!(user.is_valid())
```

### `assert_eq!(left, right)`
Assert equality:

```tml
assert_eq!(result, 42)
assert_eq!(user.name, "Alice")
```

### `assert_ne!(left, right)`
Assert inequality:

```tml
assert_ne!(x, 0)
assert_ne!(status, Status.Error)
```

### Custom messages:

```tml
assert!(x > 0, "x must be positive, got: {}", x)
assert_eq!(a, b, "values should match")
```

## Test Organization

### File naming
- Test files: `*.test.tml`
- Integration tests: `tests/*.tml`
- Unit tests: Inline or separate `*.test.tml` files

### Module structure
```
src/
  lib.tml           # Main library
  math.tml          # Math module
  math.test.tml     # Math tests
  string.tml        # String module
  string.test.tml   # String tests
tests/
  integration.test.tml  # Integration tests
```

## CLI Commands

### Basic usage
```bash
tml test                    # Run all tests
tml test --release          # Run in release mode
tml test --nocapture        # Show stdout/stderr
tml test --test-threads=4   # Parallel threads
```

### Filtering
```bash
tml test test_name          # Run specific test
tml test math::             # Run tests in math module
tml test --ignored          # Run only ignored tests
```

### Benchmarking
```bash
tml test --bench            # Run benchmarks
tml test --bench pattern    # Run specific benchmarks
```

### Output control
```bash
tml test --quiet            # Minimal output
tml test --verbose          # Verbose output
tml test --nocapture        # Show test output
```

## Example Test Suite

```tml
// calculator.test.tml
use std.test

type Calculator {
    value: I32,
}

impl Calculator {
    func new() -> Calculator {
        Calculator { value: 0 }
    }

    func add(mut this, x: I32) {
        this.value = this.value + x
    }

    func result(this) -> I32 {
        this.value
    }
}

@test
func test_new_calculator() {
    let calc: Calculator = Calculator.new()
    assert_eq!(calc.result(), 0)
}

@test
func test_addition() {
    let mut calc: Calculator = Calculator.new()
    calc.add(5)
    calc.add(3)
    assert_eq!(calc.result(), 8)
}

@test
@should_panic(expected: "overflow")
func test_overflow() {
    let mut calc: Calculator = Calculator.new()
    calc.value = I32::MAX
    calc.add(1)  // Should panic
}

@bench
func bench_many_additions() {
    let mut calc: Calculator = Calculator.new()
    for i in 0 to 1000 {
        calc.add(1)
    }
}
```

## Implementation Status

- [x] Test decorator syntax
- [x] Test discovery design
- [x] Test runner implementation (basic)
- [x] CLI integration (via compiler)
- [x] Assertion functions (builtin)
- [x] Benchmark support (basic)
- [x] Output formatters (basic)
- [ ] Parallel execution
- [ ] Advanced assertions (generics)
- [ ] Test fixtures
- [ ] Snapshot testing

## Related RFCs

- RFC-0010: Testing Framework
- RFC-0025: Decorators (Section 8 - Built-in decorators)
