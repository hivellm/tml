# Test Framework Architecture

This document describes the internal architecture of the TML test framework.

## Overview

The test framework is organized into modular components:

```
test/
├── src/
│   ├── mod.tml           # Module root (re-exports)
│   ├── types.tml         # Core type definitions
│   ├── assertions.tml    # Assertion functions
│   ├── runner.tml        # Test execution engine
│   ├── bench.tml         # Benchmarking utilities
│   └── report.tml        # Result formatting
├── examples/
│   └── basic.test.tml    # Example tests
└── package.tml           # Package manifest
```

## Component Responsibilities

### 1. Types Module (`types.tml`)

**Purpose:** Define core types used across the framework

**Exports:**
- `TestResult` - Success/failure/panic/ignored status
- `TestStatus` - Current execution state
- `TestMetadata` - Test function metadata
- `TestContext` - Execution context
- `TestFn` - Test function type
- `TestEntry` - Registered test
- `TestConfig` - Configuration options
- `TestStats` - Execution statistics

**Why separate:** Types are shared across all modules, avoiding circular dependencies.

### 2. Assertions Module (`assertions.tml`)

**Purpose:** Provide assertion functions for test validation

**Exports:**
- `assert!(condition)` - Basic assertion
- `assert_eq!(left, right)` - Equality
- `assert_ne!(left, right)` - Inequality
- `assert_gt/ge/lt/le!()` - Comparisons
- `assert_some/none!()` - Option checks
- `assert_ok/err!()` - Result checks

**Implementation:**
- All assertions panic on failure
- Generic over types with trait bounds (Eq, Ord)
- Support custom messages

**Why separate:** Assertions are the primary API developers interact with.

### 3. Runner Module (`runner.tml`)

**Purpose:** Test discovery and execution

**Exports:**
- `TESTS: [TestEntry]` - Global test registry
- `run_test(test)` - Execute single test
- `run_all_tests(config)` - Execute all tests
- `run_filtered_tests(pattern)` - Execute matching tests
- `run_ignored_tests()` - Execute ignored tests

**Flow:**
```
1. Compiler populates TESTS array during compilation
2. run_all_tests() iterates over TESTS
3. For each test:
   a. Check if ignored
   b. Check filter match
   c. Execute test function
   d. Catch panics (future)
   e. Record result
4. Return aggregate statistics
```

**Why separate:** Test execution logic is complex and deserves its own module.

### 4. Bench Module (`bench.tml`)

**Purpose:** Performance benchmarking

**Exports:**
- `bench(name, mode, fn)` - Run benchmark
- `bench_auto(name, fn)` - Auto-determine iterations
- `bench_fixed(name, count, fn)` - Fixed iterations
- `BenchResult` - Results with timing stats
- `BenchMode` - Iteration configuration

**Algorithm:**
```
1. Warm-up run
2. Sample run (10 iterations)
3. Calculate time per iteration
4. Determine optimal iteration count (target ~1s)
5. Run full benchmark
6. Collect stats (min, max, avg, std dev)
```

**Why separate:** Benchmarking is optional and has different concerns than testing.

### 5. Report Module (`report.tml`)

**Purpose:** Format and display results

**Exports:**
- `print_stats(stats, format)` - Print test summary
- `print_bench(result)` - Print benchmark results
- `print_failure(name, reason)` - Print failure details
- `OutputFormat` - Pretty/Quiet/Json

**Formats:**
- **Pretty:** Human-readable with colors (future)
- **Quiet:** Minimal output (ok/FAILED)
- **Json:** Machine-readable for CI/CD

**Why separate:** Reporting concerns are independent of test execution.

## Compiler Integration

### Test Discovery

The compiler's test command (`tml test`) performs:

```cpp
// cmd_test.cpp
1. discover_test_files(cwd)
   - Scan for *.test.tml files
   - Scan tests/ directory

2. For each file:
   - compile_and_run_test(file)
   - Parse file
   - Find @test decorators
   - Type check
   - Generate test binary
   - Execute and collect results
```

### Test Compilation

When compiling with `--test` flag (future):

```
Source Code
    ↓
Parser (find @test functions)
    ↓
Type Checker
    ↓
IR Generator
    ↓
Code Generator
    ↓
Test Harness (wraps tests)
    ↓
Test Binary
```

## Data Flow

```
┌─────────────────┐
│  Test Source    │  *.test.tml files
└────────┬────────┘
         │ Compiler discovers
         ↓
┌─────────────────┐
│  Test Registry  │  TESTS: [TestEntry]
└────────┬────────┘
         │ Runner iterates
         ↓
┌─────────────────┐
│  Test Execution │  run_test(entry)
└────────┬────────┘
         │ Assertions checked
         ↓
┌─────────────────┐
│  Test Result    │  Passed/Failed/Panicked
└────────┬────────┘
         │ Reporter formats
         ↓
┌─────────────────┐
│  Output         │  Pretty/Quiet/Json
└─────────────────┘
```

## Extension Points

### 1. Custom Assertions

Users can create domain-specific assertions:

```tml
use test::assertions

func assert_valid_email(email: Str) {
    if not email.contains("@") then {
        panic("invalid email: " + email)
    }
}
```

### 2. Test Fixtures

Setup/teardown pattern:

```tml
func setup() -> Database {
    Database::connect("test.db")
}

func teardown(db: Database) {
    db.close()
}

@test
func test_with_db() {
    let db: Database = setup()
    // Test code
    teardown(db)
}
```

### 3. Custom Reporters

```tml
use test::report

func my_reporter(stats: TestStats) {
    println("Custom format: ...")
}
```

## Performance Considerations

### Parallel Execution (Future)

Tests will run in parallel using thread pool:

```
Thread 1: [test_1, test_5, test_9]
Thread 2: [test_2, test_6, test_10]
Thread 3: [test_3, test_7, test_11]
Thread 4: [test_4, test_8, test_12]
```

**Requirements:**
- Tests must be thread-safe
- No shared mutable state
- Each test has isolated context

### Incremental Testing (Future)

Cache test results and only re-run changed tests:

```
test_cache.json:
{
  "test_addition": {
    "hash": "abc123",
    "result": "passed",
    "duration": 1000
  }
}
```

## Security Considerations

### 1. Test Isolation

Each test should run in isolated environment:
- Separate memory space (future with processes)
- No access to other test state
- Clean environment variables

### 2. Resource Limits

Tests should have limits:
- Max execution time (timeout)
- Max memory usage
- Max file system operations

### 3. Panic Handling

Panics must be caught to prevent:
- Test runner crashes
- Resource leaks
- Undefined behavior

## Future Enhancements

1. **Snapshot Testing** - Compare output to saved snapshots
2. **Property-Based Testing** - Generate random test inputs
3. **Coverage Analysis** - Track code coverage
4. **Test Templates** - Parameterized tests
5. **Async Tests** - Support for async/await
6. **Test Dependencies** - Run tests in order
7. **Parallel Benchmarks** - Multi-threaded benchmarks
8. **Result Caching** - Skip unchanged tests
9. **HTML Reports** - Web-based test reports
10. **IDE Integration** - VSCode test explorer

## Related Documentation

- [RFC-0010: Testing Framework](../../docs/rfcs/RFC-0010-TESTING.md)
- [RFC-0025: Decorators](../../docs/rfcs/RFC-0025-DECORATORS.md)
- [Package README](./README.md)
