# TML Test Framework

Subprocess-based test framework with assertions, benchmarking, and code coverage. Each test suite compiles to a standalone EXE and streams results via NDJSON.

[Changelog](CHANGELOG.md)

## Module Index

| Module | Path | Description |
|--------|------|-------------|
| assertions | `test::assertions` | `assert`, `assert_eq`, `assert_ne`, `assert_gt`, `assert_gte`, `assert_lt`, `assert_lte`, `assert_in_range`, `assert_true`, `assert_false`, `assert_str_len`, `assert_str_empty` |
| types | `test::types` | `TestResult`, `TestStatus`, `TestStats` — result types |
| runner | `test::runner` | Test runner with discovery and execution |
| bench | `test::bench` | `@bench` decorator — benchmark support with timing |
| report | `test::report` | Test report generation and formatting |
| coverage | `test::coverage` | Function-level coverage tracking via `TML_COVERAGE_FILE` |
| mock | `test::mock` | Mocking support |
| property | `test::property` | Property-based testing |
| e2e | `test::e2e` | End-to-end test utilities |

## CLI

| Command | Description |
|---------|-------------|
| `tml test` | Run all tests |
| `tml test --suite=core/str` | Run specific module suite |
| `tml test --path=file.test.tml` | Run single test file |
| `tml test --coverage` | Track function-level coverage |
| `tml test --no-cache` | Force full recompilation |
| `tml test --verbose` | Verbose output |
| `tml test --fail-fast` | Stop on first failure |

## Architecture

```
tml test → coordinator → discover *.test.tml → group into suites
         → compile each suite to .exe (parallel)
         → launch subprocess per suite
         → parse NDJSON results → aggregate → report
```

Each suite runs as an isolated subprocess. Coverage tracked via env var — no LLVM profiling, no hangs.
