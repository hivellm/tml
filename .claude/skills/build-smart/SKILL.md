---
name: build-smart
description: Detect what changed and build only the necessary target. Use when the user says "build smart", "build inteligente", or wants an optimized build.
user-invocable: true
allowed-tools: mcp__tml__project_build, Bash(git diff *), Glob
argument-hint: no arguments needed — auto-detects what changed
---

## Smart Build Workflow

### 1. Detect Changes

Run `git diff --name-only HEAD` via Bash to see what files changed.

### 2. Determine Target

Based on changed files:
- **`compiler/src/`** changes → target: "compiler" (rebuild tml.exe)
- **`compiler/src/mcp/`** changes → target: "mcp" (rebuild tml_mcp.exe) — WARN: kills MCP connection
- **`compiler/tests/`** changes → target: "compiler" with `tests: true`
- **`lib/`** or **`docs/`** only → NO build needed, just re-run tests
- **Mixed changes** → target: "compiler"

### 3. Build

Use `mcp__tml__project_build` with the determined target.

- Default to `mode: "debug"`
- Set `tests: true` only if test C++ files changed

### 4. Report

- What changed (file list)
- What was built (target)
- Build result (success/failure)
- Suggestion: which tests to run next