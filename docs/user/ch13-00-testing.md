# Testing

TML has a built-in testing framework that integrates with the compiler and the `tml test` command. Tests live in files alongside or separate from the code they exercise, are annotated with the `@test` decorator, and are discovered and run automatically.

## The Testing Model

A test in TML is an ordinary function that:

1. Is annotated with `@test`
2. Returns `I32` (zero means success; the framework interprets any non-zero return as a failure, though assertions abort the function before it can return a non-zero value explicitly)
3. Uses assertion functions to check conditions; a failing assertion aborts the test immediately and reports which assertion failed and where

The test file imports the `test` package to bring assertion functions into scope:

```tml
use test

@test
func test_addition() -> I32 {
    assert_eq(1 + 1, 2, "basic addition")
    return 0
}
```

## Test Files

Test files use the `.test.tml` extension and live inside a `tests/` directory next to the code they test:

```
project/
├── src/
│   ├── math.tml
│   └── parser.tml
└── tests/
    ├── math/
    │   ├── basic.test.tml
    │   └── edge_cases.test.tml
    └── parser/
        └── parser.test.tml
```

The `tml test` command discovers all `.test.tml` files in the project and runs them.

## What This Chapter Covers

- **Writing Tests** (ch13-01) — the `@test` decorator, assertion functions, `@should_panic`, organizing test files, and patterns for testing different kinds of code
- **Running Tests** (ch13-02) — the `tml test` command, filtering, coverage, verbose output, and the output format
- **Benchmarks** (ch13-03) — the `@bench` decorator, iteration counts, warmup, and reading benchmark output
