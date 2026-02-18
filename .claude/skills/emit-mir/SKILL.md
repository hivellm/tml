---
name: emit-mir
description: Emit MIR (Mid-level IR) for a TML source file. Use for debugging the MIR pipeline.
user-invocable: true
allowed-tools: mcp__tml__emit-mir
argument-hint: <file.tml>
---

## Emit MIR

Use the `mcp__tml__emit-mir` MCP tool.

Parse `$ARGUMENTS`:
- **file**: The `.tml` file path (required)

Call `mcp__tml__emit-mir` with the file parameter.

Display the MIR output to the user.