---
name: structure
description: Show the TML project module tree with file counts and test coverage. Use when the user asks about project structure, module layout, or file organization.
user-invocable: true
allowed-tools: mcp__tml__project_structure
argument-hint: [optional module e.g. core, std::json] [--files]
---

## Show Project Structure

Use the `mcp__tml__project_structure` MCP tool.

Parse `$ARGUMENTS`:
- **module**: Filter to specific module if a module path is given (e.g., "core", "std::json", "test")
- **show_files**: `true` if `--files` or `files` is mentioned (show individual file names)
- **depth**: Numeric value if specified (default: 3)

Call `mcp__tml__project_structure` with extracted parameters.

Display the module tree to the user.