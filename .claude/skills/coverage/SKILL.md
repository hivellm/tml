---
name: coverage
description: Run tests with coverage and show coverage report. Use when the user says "coverage", "cobertura", or wants to see test coverage data.
user-invocable: true
allowed-tools: mcp__tml__test, mcp__tml__project_coverage, Read
argument-hint: [optional module filter e.g. core::str, std::json]
---

## Coverage Workflow

### 1. Run Tests with Coverage

Use `mcp__tml__test` with:
- `coverage: true`
- `verbose: true`
- `no_cache: true` (coverage requires fresh compilation)

### 2. Show Coverage Report

After tests complete, use `mcp__tml__project_coverage` with:
- `module`: `$ARGUMENTS` if a specific module was requested
- `sort`: "lowest" to show modules needing the most coverage work
- `refresh`: false (we just ran tests above)

### 3. Report

Show coverage summary:
- Overall coverage percentage
- Modules with lowest coverage (opportunities for improvement)
- If a specific module was requested, show detailed breakdown for that module