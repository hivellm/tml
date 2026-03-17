# Running Tests

TML provides a comprehensive test runner with filtering, coverage, profiling, and caching built in.

## Basic Commands

```bash
# Run the full test suite
tml test

# Run a specific module suite
tml test --suite core/str
tml test --suite std/json

# Run a single test file
tml test lib/core/tests/str/basic.test.tml

# Filter tests by name
tml test --filter "test_add"
tml test --filter "hash"
```

## Command-Line Options

| Flag | Description |
|------|-------------|
| `--suite <path>` | Run tests for a specific module (e.g., `core/str`, `std/json`) |
| `--filter <pattern>` | Run only tests whose names match the pattern |
| `--coverage` | Generate a coverage report after running |
| `--profile` | Show timing information for each test |
| `--fail-fast` | Stop on the first test failure |
| `--no-cache` | Skip the compilation cache and rebuild everything |
| `--verbose` | Show detailed output including individual test results |
| `--bench` | Run benchmark functions in addition to tests |
| `--timeout <seconds>` | Set a per-test timeout |

## Test Architecture

TML uses a subprocess-based test architecture inspired by Go's testing model. Each test suite compiles into a standalone executable that runs as a subprocess.

This design provides:

- **Isolation**: A crash in one test suite does not affect others.
- **Parallelism**: Multiple test suites run concurrently.
- **Clean output**: Results stream via NDJSON protocol from subprocess to coordinator.

## Caching

The test runner automatically caches compilation results. When you modify a source file, only the affected test suites are recompiled. The cache invalidates based on source file timestamps and content hashes.

Cache files are stored in the build directory. You never need to manage them manually. Use `--no-cache` only when you suspect stale results.

## Coverage Reports

Run tests with coverage to see which functions are exercised:

```bash
tml test --coverage
```

The report shows the percentage of functions covered per module. Use this to identify gaps in your test suite.

## Profiling

The `--profile` flag adds timing information to the output, showing the slowest tests and compilation bottlenecks:

```bash
tml test --profile
```

This is useful for identifying tests that take too long and may need optimization.

## Practical Workflow

A typical development workflow uses targeted test runs during development and full suite runs before committing:

```bash
# During development: run only the module you're changing
tml test --suite core/str

# Before committing: run the full suite with coverage
tml test --coverage

# Investigating a failure: run with verbose and filter
tml test --filter "test_split" --verbose
```
