---
name: verify
description: Build the compiler and run targeted tests to verify changes. Use when the user says "verify", "verifica", "build and test", or after making compiler C++ changes that need validation.
user-invocable: true
allowed-tools: mcp__tml__project_build, mcp__tml__test, mcp__tml__project_affected_tests, Read, Grep, Glob
argument-hint: optional suite names to test after build
---

## Verify Workflow

Build the compiler and run targeted tests — the most common workflow after C++ changes.

### 1. Build Compiler

Use `mcp__tml__project_build` with:
- `target`: "compiler" (don't rebuild MCP)
- `tests`: false (skip test executable for speed)

If the build fails, report the error and stop.

### 2. Determine Which Tests to Run

If `$ARGUMENTS` specifies suite(s), use those directly.

If no arguments:
- Use `mcp__tml__project_affected_tests` to detect which tests are affected by recent changes
- If affected tests are identified, run those specific suites
- If no affected tests detected (e.g., compiler-only changes), ask the user which suites to test

### 3. Run Tests

For each target suite, call `mcp__tml__test` with:
- `suite`: The suite name
- `no_cache`: true
- `fail_fast`: true
- `structured`: true

### 4. Report

- Build status (success/failure, duration)
- Test results per suite (passed/failed)
- If all pass: concise "Build + tests OK"
- If failures: show which tests failed and why