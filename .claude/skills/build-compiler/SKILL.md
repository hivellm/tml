---
name: build
description: Build the TML compiler from C++ sources. Use when the user says "build", "compila", "rebuild", or needs to recompile the compiler after C++ changes.
user-invocable: true
allowed-tools: mcp__tml__project_build, Bash(cd * && cmd *)
argument-hint: [optional: release, clean, compiler, mcp]
---

## Build Workflow

### Determine Build Type

Based on `$ARGUMENTS`:

- **No arguments**: Debug build, all targets
- **"release"**: Release build with optimizations
- **"clean"**: Clean build directory first
- **"compiler"**: Build only tml.exe (skip MCP server rebuild)
- **"mcp"**: Build only tml_mcp.exe

### Execute Build

**Preferred**: Use `mcp__tml__project_build` MCP tool with:
- `mode`: "debug" or "release"
- `clean`: true if clean build requested
- `target`: "all", "compiler", or "mcp"

**Fallback** (if MCP tool unavailable): Use the canonical Bash command:
```bash
cd /f/Node/hivellm/tml && cmd //c "scripts\\build.bat --no-tests" 2>&1
```

Add flags as needed:
- `release` for release mode
- `--clean` for clean build
- `--no-tests` to skip test executable (default for quick builds)

### NEVER

- NEVER use cmake directly
- NEVER use powershell to build
- NEVER skip the build scripts

### Report

After build completes, report success/failure and the output path.