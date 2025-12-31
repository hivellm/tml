# RFC-0010: Testing Framework

## Status
Active - **Implemented in v0.1.0**

## Summary

This RFC defines the standard testing framework for TML, providing built-in support for unit tests, integration tests, and benchmarks through decorators and a dedicated CLI command.

## Motivation

Modern programming languages need robust, built-in testing support to ensure code quality and facilitate test-driven development. TML's testing framework aims to:

1. **Simplify Testing** - Make writing tests as easy as adding a `@test` decorator
2. **Test Discovery** - Automatically find and run tests without manual registration
3. **Parallel Execution** - Run tests concurrently for fast feedback
4. **Benchmarking** - Measure performance with `@bench` decorator
5. **Integration** - Seamless integration with CI/CD pipelines

## Specification

### 1. Test Files

Test files follow naming conventions:

```
src/
  lib.tml              # Main library
  math.tml             # Math module
  math.test.tml        # Unit tests for math
tests/
  integration.test.tml # Integration tests
  e2e.test.tml         # End-to-end tests
```

**Rules:**
- Files ending in `.test.tml` are automatically discovered
- Files in `tests/` directory are treated as tests
- Test files can be anywhere in the project hierarchy

### 2. Test Decorators

#### 2.1 Basic Test

```tml
@test
func test_addition() {
    assert_eq!(2 + 2, 4)
}
```

#### 2.2 Named Test

```tml
@test(name: "Addition works correctly")
func test_addition() {
    assert_eq!(2 + 2, 4)
}
```

#### 2.3 Should Panic

```tml
@test
@should_panic
func test_division_by_zero() {
    let x: I32 = 1 / 0
}
```

#### 2.4 Should Panic with Message

```tml
@test
@should_panic(expected: "division by zero")
func test_division_error() {
    let x: I32 = 1 / 0
}
```

#### 2.5 Ignored Test

```tml
@test
@ignore
func slow_test() {
    // Only runs with --ignored flag
}
```

#### 2.6 Benchmark

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

Benchmark files use `.bench.tml` extension and are discovered separately from tests.

### 3. Assertions

#### 3.1 Basic Assertions

```tml
assert!(condition)
assert!(condition, "custom message")
```

#### 3.2 Equality

```tml
assert_eq!(left, right)
assert_eq!(left, right, "values should match")
assert_ne!(left, right)
```

#### 3.3 Comparison

```tml
assert_gt!(a, b)   // greater than
assert_ge!(a, b)   // greater or equal
assert_lt!(a, b)   // less than
assert_le!(a, b)   // less or equal
```

#### 3.4 Result/Option Assertions

```tml
assert_ok!(result)     // Assert Ok, returns value
assert_err!(result)    // Assert Err, returns error
assert_some!(option)   // Assert Some, returns value
assert_none!(option)   // Assert Nothing
```

### 4. CLI Commands

#### 4.1 Run All Tests

```bash
tml test
```

#### 4.2 Run Specific Tests

```bash
tml test test_name           # Run tests matching name
tml test math::              # Run tests in module
tml test --test file.test.tml  # Run specific file
```

#### 4.3 Test Options

```bash
tml test --nocapture         # Show stdout/stderr
tml test --verbose           # Verbose output
tml test --quiet             # Minimal output
tml test --ignored           # Run only ignored tests
tml test --test-threads=4    # Parallel threads
```

#### 4.4 Benchmarking

```bash
tml test --bench                        # Run all benchmarks (*.bench.tml)
tml test --bench pattern                # Run specific benchmarks
tml test --bench --save-baseline=b.json # Save results for comparison
tml test --bench --compare=b.json       # Compare against baseline
```

### 5. Test Discovery

The test framework automatically discovers tests by:

1. Scanning for `*.test.tml` files recursively
2. Scanning `tests/` directory for `.tml` files
3. Finding functions decorated with `@test`
4. Building a test registry

**Discovery Algorithm:**

```
for each file in [src/**/*.test.tml, tests/**/*.tml]:
    parse(file)
    for each function with @test decorator:
        register_test(
            name: function.name,
            file: file.path,
            line: function.line,
            metadata: extract_from_decorators(function)
        )
```

### 6. Test Execution

#### 6.1 Test Lifecycle

```
1. Test Discovery
   └─> Find all *.test.tml files
   └─> Parse and find @test functions

2. Test Compilation
   └─> Compile each test file
   └─> Link with test runtime

3. Test Execution
   └─> Run tests (parallel or sequential)
   └─> Capture output
   └─> Measure time

4. Result Reporting
   └─> Aggregate results
   └─> Print summary
   └─> Exit with appropriate code
```

#### 6.2 Parallel Execution

Tests run in parallel by default:

```bash
tml test                     # Auto-detect threads
tml test --test-threads=4    # Use 4 threads
tml test --test-threads=1    # Sequential execution
```

**Safety Rules:**
- Tests must be thread-safe
- No shared mutable state between tests
- Each test runs in isolated context

### 7. Output Formats

#### 7.1 Default Output

```
running 12 tests

test math::test_addition ... ok
test math::test_subtraction ... ok
test string::test_concat ... ok
test string::test_length ... FAILED

test result: FAILED. 11 passed; 1 failed; 0 ignored; finished in 0.05s

---- string::test_length stdout ----
assertion failed: left != right
  left: 4
  right: 3
```

#### 7.2 Quiet Output

```bash
$ tml test --quiet
FAILED
```

#### 7.3 Verbose Output

```bash
$ tml test --verbose
running 12 tests

[1/12] math::test_addition ... ok (0.001s)
[2/12] math::test_subtraction ... ok (0.002s)
...
```

#### 7.4 JSON Output

```bash
$ tml test --format=json
{
  "total": 12,
  "passed": 11,
  "failed": 1,
  "ignored": 0,
  "duration_us": 50000
}
```

### 8. Package Structure

The test framework is provided as a standard package:

```
packages/test/
  package.tml           # Package manifest
  README.md             # Documentation
  src/
    mod.tml             # Module root
    types.tml           # Core types
    assertions.tml      # Assertion functions
    runner.tml          # Test execution
    bench.tml           # Benchmarking
    report.tml          # Result reporting
  examples/
    basic.test.tml      # Example tests
  tests/
    self_test.tml       # Framework self-tests
```

### 9. Integration with Compiler

#### 9.1 Test Mode

When compiling with `--test` flag:

```bash
tml build --test src/lib.tml
```

The compiler:
1. Discovers all `@test` functions
2. Generates test harness
3. Links with test runtime
4. Produces test binary

#### 9.2 Test Binary

Test binaries expose standard interface:

```bash
./target/debug/mylib_tests            # Run all
./target/debug/mylib_tests test_name  # Filter
./target/debug/mylib_tests --bench    # Benchmarks
```

### 10. Example Test Suite

```tml
// calculator.test.tml
use test

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

| Component | Status | Details |
|-----------|--------|---------|
| **Test Package** | ✅ Implemented | Complete modular structure |
| **CLI Command** | ✅ Implemented | `tml test` with options |
| **Test Discovery** | ✅ Implemented | File scanning and pattern matching |
| **Assertions** | ✅ Implemented | 12+ assertion functions |
| **Runner** | ✅ Implemented | Test execution engine |
| **Benchmarking** | ✅ Implemented | `@bench`, `@bench(N)`, `*.bench.tml` files |
| **Benchmark Comparison** | ✅ Implemented | `--save-baseline`, `--compare` |
| **Reporting** | ✅ Implemented | Multiple output formats |
| **Parallel Execution** | ✅ Implemented | Multi-threaded test runner |
| **Coverage** | ✅ Implemented | `--coverage`, `--coverage-output` |
| **Panic Catching** | ❌ TODO | Requires exception handling |

## Compatibility

### With RFC-0025 (Decorators)
Test framework uses decorators defined in RFC-0025:
- `@test` - Mark test function
- `@bench` - Mark benchmark
- `@ignore` - Skip test
- `@should_panic` - Expect panic

### With Rust
TML test framework is inspired by Rust's built-in testing:
- Similar decorator syntax (`#[test]` → `@test`)
- Compatible CLI arguments
- Similar output format
- Familiar workflow for Rust developers

## Alternatives Rejected

### 1. External Test Framework
**Rejected:** Baked-in testing reduces friction and ensures consistency.

### 2. JUnit-style Classes
**Rejected:** Function-based tests with decorators are simpler and more flexible.

### 3. Separate Test Binary
**Rejected:** Integrated `tml test` command provides better UX.

## References

### Inspiration
- **Rust** - `cargo test` and `#[test]` macro
- **Go** - `go test` and `testing` package
- **Python** - `pytest` decorator-based testing

### Related RFCs
- RFC-0025: Decorators (test decorators)
- RFC-0009: CLI (test command)

## Migration Guide

For projects without testing:

```bash
# 1. Create test file
touch src/lib.test.tml

# 2. Write tests
echo '@test
func test_example() {
    assert_eq!(1 + 1, 2)
}' > src/lib.test.tml

# 3. Run tests
tml test
```

## Future Enhancements

- ~~Test coverage analysis~~ ✅ Implemented (`--coverage`)
- Snapshot testing
- Property-based testing (`@fuzz`)
- Test fixtures and setup/teardown
- ~~Parallel test execution~~ ✅ Implemented (`--test-threads`)
- Test result caching
- HTML coverage reports
