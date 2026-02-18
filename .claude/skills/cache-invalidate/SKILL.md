---
name: cache-invalidate
description: Invalidate compilation cache for specific source files. Use when cached results seem stale or after external changes to source files.
user-invocable: true
allowed-tools: mcp__tml__cache_invalidate
argument-hint: <file1.tml> [file2.tml...]
---

## Invalidate Cache

Use the `mcp__tml__cache_invalidate` MCP tool.

Parse `$ARGUMENTS`:
- **files**: List of file paths to invalidate (required)
- **verbose**: `true` if `--verbose` is specified

If no specific files given, ask the user which files to invalidate.

Call `mcp__tml__cache_invalidate` with the files array and verbose flag.

Report which cache entries were invalidated.