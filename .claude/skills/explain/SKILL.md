---
name: explain
description: Explain a TML compiler error code. Use when the user asks about an error code like T001, B001, L003.
user-invocable: true
allowed-tools: mcp__tml__explain
argument-hint: <error-code e.g. T001, B001, L003>
---

## Explain Error Code

Use the `mcp__tml__explain` MCP tool.

Parse `$ARGUMENTS`:
- **code**: The error code (required, e.g., "T001", "B001", "L003")

Call `mcp__tml__explain` with the code parameter.

Present the explanation clearly: description, common causes, and fix examples.