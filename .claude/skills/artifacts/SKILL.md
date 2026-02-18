---
name: artifacts
description: List build artifacts with sizes and ages. Use when the user asks about build outputs, compiled binaries, or disk usage.
user-invocable: true
allowed-tools: mcp__tml__project_artifacts
argument-hint: [executables|libraries|cache|coverage|all] [--release]
---

## List Build Artifacts

Use the `mcp__tml__project_artifacts` MCP tool.

Parse `$ARGUMENTS`:
- **kind**: Filter by type if specified: "executables", "libraries", "cache", "coverage", or "all"
- **config**: "release" if `--release` or `release` mentioned, "debug" by default, "all" if `--all`

Call `mcp__tml__project_artifacts` with extracted parameters.

Display artifact listing with sizes and modification times.