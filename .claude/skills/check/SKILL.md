---
name: check
description: Type check a TML source file without compiling. Use when the user wants to verify types, check for errors, or validate code without building.
user-invocable: true
allowed-tools: mcp__tml__check
argument-hint: <file.tml>
---

## Type Check a TML File

Use the `mcp__tml__check` MCP tool.

Parse `$ARGUMENTS`:
- **file**: The `.tml` file path (required)

Call `mcp__tml__check` with the file parameter.

Report type check results: success or list of type errors with locations.