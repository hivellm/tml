---
name: format
description: Format TML source files. Use when the user says "format", "formata", or wants to auto-format code.
user-invocable: true
allowed-tools: mcp__tml__format
argument-hint: <file.tml or directory> [--check]
---

## Format TML Files

Use the `mcp__tml__format` MCP tool.

Parse `$ARGUMENTS`:
- **file**: The `.tml` file or directory path (required, first argument)
- **check**: `true` if `--check` is specified (only check, don't modify)

Call `mcp__tml__format` with extracted parameters.

Report which files were formatted or if all files were already formatted.