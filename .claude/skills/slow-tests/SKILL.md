---
name: slow-tests
description: Analyze which tests are slowest and why. Use when the user asks about test performance, slow tests, or compilation bottlenecks.
user-invocable: true
allowed-tools: mcp__tml__project_slow_tests, mcp__tml__test, Read, Grep
argument-hint: [optional: limit, threshold in ms]
---

## Slow Tests Analysis Workflow

### 1. Check for Fresh Data

The analysis requires a `test_log.json` from a recent `--verbose --no-cache` run.
If the data seems stale, run tests first:

Use `mcp__tml__test` with `verbose: true`, `no_cache: true`

### 2. Analyze

Use `mcp__tml__project_slow_tests` (once the MCP server has the tool) or parse `build/debug/test_log.json` directly.

Look for "Phase 1 slow" entries which show per-file individual timing:
```
Phase 1 slow #N: filename.test.tml Xms [lex=A parse=B tc=C borrow=D cg=E]
```

Parameters based on `$ARGUMENTS`:
- A number like "30" -> use as `limit`
- A number with "ms" like "5000ms" -> use as `threshold`

### 3. Report

Present results as a table sorted by total time, showing:
- File name
- Total compilation time
- Breakdown: lex, parse, typecheck, borrow, codegen
- Which sub-phase is the bottleneck (almost always codegen)

Highlight the worst offenders and explain why they're slow (large module imports, complex generics, etc.)