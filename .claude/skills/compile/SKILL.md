---
name: compile
description: Compile a TML source file to executable. Use when the user wants to compile a .tml file.
user-invocable: true
allowed-tools: mcp__tml__compile
argument-hint: <file.tml> [--release] [--optimize O0|O1|O2|O3]
---

## Compile a TML File

Use the `mcp__tml__compile` MCP tool.

Parse `$ARGUMENTS` to extract:
- **file**: The `.tml` file path (required, first argument)
- **output**: Output path if specified with `-o` or `--output`
- **optimize**: Optimization level if `O0`, `O1`, `O2`, or `O3` is mentioned
- **release**: `true` if `--release` or `release` is mentioned

Call `mcp__tml__compile` with the extracted parameters.

Report compilation success/failure and diagnostics.