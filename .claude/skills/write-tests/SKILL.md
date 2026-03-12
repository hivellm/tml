---
name: write-tests
description: Write new TML tests for a module or feature. Use when the user says "write tests", "escreve testes", "add tests", "test coverage for X", or wants to improve test coverage for a specific module.
user-invocable: true
argument-hint: "<module or feature to test — e.g. 'core/str split methods', 'std/json parsing', 'Maybe[T] methods'>"
---

**Delegation**: Use the Agent tool to dispatch a `tml-library-engineer` agent with `model: opus` for this task. Writing tests requires deep knowledge of TML syntax and type system.

## Test Writing Workflow

### 1. Understand What to Test

From `$ARGUMENTS`, identify the module/feature. Then:
- Read the source implementation in `lib/core/` or `lib/std/`
- Check existing tests in `lib/core/tests/` or `lib/std/tests/`
- Use `mcp__tml__docs_search` to find function signatures

### 2. Check Coverage

Use `mcp__tml__project_coverage` with `module` filter to see which functions lack coverage.

### 3. Write Tests Incrementally

Follow CLAUDE.md mandatory rules:
- Write 1-3 tests at a time
- Run each batch immediately with `mcp__tml__test`
- Fix errors before writing more

### 4. Test File Pattern

```tml
use test

func test_<descriptive_name>() -> I32 {
    // Setup
    let value = ...

    // Assert
    assert_eq(value, expected)

    0
}
```

### 5. Run Coverage

After all tests pass, run `mcp__tml__test` with `coverage: true` to verify coverage improved.

### IMPORTANT

- NEVER simplify assertions or create placeholder tests
- Every test must actually verify real behavior
- Use descriptive test names
