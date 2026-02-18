---
name: emit-ir
description: Emit LLVM IR for a TML source file. Use when the user wants to see the generated IR, debug codegen, or compare IR output.
user-invocable: true
allowed-tools: mcp__tml__emit-ir
argument-hint: <file.tml> [--function name] [--optimize O0|O1|O2|O3]
---

## Emit LLVM IR

Use the `mcp__tml__emit-ir` MCP tool.

Parse `$ARGUMENTS`:
- **file**: The `.tml` file path (required, first argument)
- **function**: Filter to specific function if `--function` or `-f` is specified
- **optimize**: Optimization level if mentioned
- **offset/limit**: For chunked output on large files

Call `mcp__tml__emit-ir` with extracted parameters.

Display the IR output. If the output is large, use offset/limit for pagination.