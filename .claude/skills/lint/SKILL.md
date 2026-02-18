---
name: lint
description: Lint TML source files for style and potential issues. Use when the user says "lint" or wants to check code quality.
user-invocable: true
allowed-tools: mcp__tml__lint
argument-hint: <file.tml or directory> [--fix]
---

## Lint TML Files

Use the `mcp__tml__lint` MCP tool.

Parse `$ARGUMENTS`:
- **file**: The `.tml` file or directory path (required, first argument)
- **fix**: `true` if `--fix` or `fix` is specified (auto-fix issues)

Call `mcp__tml__lint` with extracted parameters.

Report lint diagnostics: warnings, errors, and suggestions.