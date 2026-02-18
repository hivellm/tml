---
name: run
description: Build and execute a TML source file. Use when the user says "run", "roda", "execute", or wants to run a .tml file.
user-invocable: true
allowed-tools: mcp__tml__run
argument-hint: <file.tml> [args...] [--release]
---

## Run a TML File

Use the `mcp__tml__run` MCP tool.

Parse `$ARGUMENTS`:
- **file**: The `.tml` file path (required, first argument)
- **args**: Any remaining arguments to pass to the program
- **release**: `true` if `--release` is mentioned

Call `mcp__tml__run` with extracted parameters.

Show the program output to the user.