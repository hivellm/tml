---
name: rebuild
description: Clean build of the TML compiler. Use when the user says "rebuild", "clean build", "build limpo", or needs a fresh compilation.
user-invocable: true
allowed-tools: mcp__tml__project_build
argument-hint: [optional release]
---

## Rebuild Workflow

### 1. Confirm

This is a clean build — it deletes the build directory and recompiles everything. This takes significantly longer than an incremental build.

Tell the user: "Starting clean build. This will take longer than a normal build."

### 2. Execute

Use `mcp__tml__project_build` with:
- `clean`: true
- `mode`: "release" if `$ARGUMENTS` contains "release", otherwise "debug"
- `target`: "compiler" (default safe target)
- `tests`: false (skip for speed unless user requests)

### 3. Report

- Build result (success/failure)
- Duration
- Output path