---
name: tdd
description: Test-driven development loop. Run a test, show errors, fix, repeat until passing. Use when the user says "tdd", "test driven", or wants an iterative test-fix cycle.
user-invocable: true
allowed-tools: mcp__tml__test, mcp__tml__emit-ir, mcp__tml__check, Read, Edit, Write, Grep, Glob
argument-hint: <test-file-path> — the test file to iterate on
---

## TDD Loop Workflow

### 1. Identify Test File

Extract the test file path from `$ARGUMENTS`.

### 2. Run Test

Use `mcp__tml__test` with:
- `path`: the test file
- `verbose`: true
- `no_cache`: true

### 3. Analyze Result

If the test **passes**: Report success and ask if the user wants to write more tests.

If the test **fails**:
- Parse the error message
- Identify whether it's a:
  - **Compilation error** → show the error, suggest a fix
  - **Runtime error** → show the assertion failure, trace the issue
  - **Codegen error** → emit IR for the test file to debug
- Present the error clearly and suggest a specific fix

### 4. Loop

After the user makes changes (or you make changes), re-run the test automatically.

Repeat until the test passes.

### IMPORTANT

Follow the incremental test development rule from CLAUDE.md:
- Write 1-3 tests at a time
- Run after each batch
- Fix errors before writing more tests