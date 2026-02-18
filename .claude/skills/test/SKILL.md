---
name: test
description: Run TML tests using the MCP test tool. Use when the user says "test", "roda os testes", "run tests", or wants to verify changes. Supports running specific test files, filtered tests, or the full suite.
user-invocable: true
allowed-tools: mcp__tml__test, mcp__tml__cache_invalidate, Read, Grep, Glob
argument-hint: [optional path or filter]
---

## Test Workflow

Use the `mcp__tml__test` MCP tool for ALL test operations. NEVER use Bash to run `tml.exe test`.

### Determine What to Test

Based on `$ARGUMENTS`:

- **No arguments**: Run full test suite with `verbose: true`
- **A file path** (e.g., `lib/std/tests/net/ip.test.tml`): Run that specific file with `path` parameter
- **A directory** (e.g., `lib/std/tests/net/`): Run all tests in that directory with `path` parameter
- **A filter string** (e.g., `collections`): Run matching tests with `filter` parameter
- **"no-cache"** or **"fresh"**: Add `no_cache: true` to force recompilation
- **"fast"**: Add `fail_fast: true` to stop on first failure

### Execution

Call `mcp__tml__test` with appropriate parameters:
- `path`: Specific file or directory
- `filter`: Test name filter
- `verbose`: Always true for useful output
- `no_cache`: Force full recompilation when requested
- `fail_fast`: Stop on first failure when requested
- `structured`: Use for machine-readable results

### Report Results

After tests complete:
- Report total/passed/failed counts
- If tests failed, show which specific tests failed and why
- If all passed, confirm success concisely