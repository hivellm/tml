---
name: test
description: Run TML tests using the MCP test tool. Use when the user says "test", "roda os testes", "run tests", or wants to verify changes. Supports running specific test files, filtered tests, specific suites, or the full suite.
user-invocable: true
allowed-tools: mcp__tml__test, mcp__tml__cache_invalidate, Read, Grep, Glob
argument-hint: optional suite name, file path, filter, no-cache, fast
---

## Test Workflow

Use the `mcp__tml__test` MCP tool for ALL test operations. NEVER use Bash to run `tml.exe test`.

### Determine What to Test

Based on `$ARGUMENTS`:

- **No arguments**: Run full test suite with `verbose: true`
- **A suite name** (e.g., `core/str`, `std/json`, `compiler/compiler`): Run that suite with `suite` parameter
- **A file path** (e.g., `lib/std/tests/net/ip.test.tml`): Run that specific file with `path` parameter
- **A directory** (e.g., `lib/std/tests/net/`): Run all tests in that directory with `path` parameter
- **A filter string** (e.g., `collections`): Run matching tests with `filter` parameter
- **"no-cache"** or **"fresh"**: Add `no_cache: true` to force recompilation
- **"fast"**: Add `fail_fast: true` to stop on first failure

### Suite Names

Suite names follow the pattern `<lib>/<module>`:
- `core/str`, `core/fmt`, `core/num`, `core/iter`, `core/ops`, `core/alloc`, ...
- `std/json`, `std/collections`, `std/crypto`, `std/io`, `std/sync`, `std/net`, ...
- `compiler/compiler`, `compiler/runtime`, `compiler/borrow`, ...

Use `--list-suites` via CLI to see all available suites.

### Execution

Call `mcp__tml__test` with appropriate parameters:
- `suite`: Suite group name (e.g., "core/str", "std/json") — **preferred for module-level testing**
- `path`: Specific file or directory
- `filter`: Test name filter (file path substring)
- `verbose`: Always true for useful output
- `no_cache`: Force full recompilation when requested
- `fail_fast`: Stop on first failure when requested
- `structured`: Use for machine-readable results (total/passed/failed/failures)

### IMPORTANT: Prefer Suite Over Full Suite

When verifying changes to a specific module, ALWAYS use `suite` parameter instead of running the full test suite. This is dramatically faster:
- `suite="core/str"` → ~3s (241 tests)
- `suite="std/json"` → ~4s (153 tests)
- Full suite → minutes (8496 tests)

Only run the full suite when explicitly asked or when changes are cross-cutting.

### Report Results

After tests complete:
- Report total/passed/failed counts
- If tests failed, show which specific tests failed and why
- If all passed, confirm success concisely