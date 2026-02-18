---
name: affected-tests
description: Detect which tests are affected by recent code changes. Use when the user wants to know what tests to run after making changes.
user-invocable: true
allowed-tools: mcp__tml__project_affected-tests
argument-hint: [--run] [--base HEAD~1]
---

## Find Affected Tests

Use the `mcp__tml__project_affected-tests` MCP tool.

Parse `$ARGUMENTS`:
- **run**: `true` if `--run` or `run` is specified (automatically run affected tests)
- **base**: Git ref to diff against if `--base` is specified (default: "HEAD")
- **verbose**: `true` if `--verbose` or `verbose` is specified

Call `mcp__tml__project_affected-tests` with extracted parameters.

Report which test directories are affected and optionally their test results.