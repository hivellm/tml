---
name: investigate
description: Investigate a failing test by running it, emitting IR, and analyzing the error. Use when the user says "investigate", "investiga", "debug test", or wants to understand why a test fails.
user-invocable: true
allowed-tools: mcp__tml__test, mcp__tml__emit-ir, mcp__tml__check, mcp__tml__explain, Read, Grep, Glob
argument-hint: <test-file-or-suite> — what to investigate
---

## Investigation Workflow

### 1. Run the Failing Test

Use `mcp__tml__test` with:
- `path` or `suite`: from `$ARGUMENTS`
- `verbose`: true
- `no_cache`: true
- `fail_fast`: true (stop at first failure)

### 2. Capture Error

Parse the test output to identify:
- Which specific test function failed
- The error type (compilation, runtime assertion, codegen, linker)
- The error message and location

### 3. Deep Analysis

Based on error type:

**Compilation error (T0xx, P0xx)**:
- Use `mcp__tml__check` to get type-check diagnostics
- Use `mcp__tml__explain` if there's an error code
- Read the source file around the error location

**Runtime/assertion error**:
- Read the test file to understand what it's testing
- Trace the assertion to the library implementation
- Read the library source to find the bug

**Codegen/linker error**:
- Use `mcp__tml__emit-ir` to see the generated IR
- Look for undefined symbols, type mismatches, or missing functions
- Check if it's a lazy-lib resolution issue

### 4. Report

Present:
- **Error**: What went wrong
- **Root cause**: Why it's happening
- **Suggested fix**: What to change and where
- **Affected files**: Which files need modification