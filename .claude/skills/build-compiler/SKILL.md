---
name: build
description: Build the TML compiler from C++ sources. Use when the user says "build", "compila", "rebuild", or needs to recompile the compiler after C++ changes.
user-invocable: true
allowed-tools: mcp__tml__project_build, Bash(cd * && cmd *)
argument-hint: optional release, clean, all, mcp
---

## Build Workflow

### Determine Build Type

Based on `$ARGUMENTS`:

- **No arguments**: Debug build, compiler only (tml.exe) — safe, won't kill MCP
- **"release"**: Release build with optimizations
- **"clean"**: Clean build directory first
- **"all"**: Build everything (tml.exe + tml_mcp.exe + tml_tests.exe) — WARNING: kills MCP connection
- **"mcp"**: Build only tml_mcp.exe — WARNING: kills MCP connection

### Execute Build

**Preferred**: Use `mcp__tml__project_build` MCP tool with:
- `mode`: "debug" or "release"
- `clean`: true if clean build requested
- `target`: "compiler" (default, safe), "all", or "mcp"
- `tests`: false to skip test executable for faster builds

**IMPORTANT**: Default target is "compiler" (tml.exe only). This is intentional — building "all" or "mcp" kills the running MCP server connection.

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