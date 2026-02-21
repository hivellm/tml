---
name: migrate-c
description: Analyze a C runtime file and suggest pure TML replacement. Use when the user says "migrate", "migra", "replace C", or wants to convert C runtime code to TML.
user-invocable: true
allowed-tools: Read, Grep, Glob, mcp__tml__docs_search
argument-hint: <c-file-path> — the C runtime file to analyze
---

## C-to-TML Migration Analysis Workflow

### 1. Read the C File

Read the file from `$ARGUMENTS` (typically in `compiler/runtime/`).

### 2. Analyze Each Function

For each function in the file:
- **Classify** as: pure algorithm, OS API wrapper, LLVM intrinsic wrapper, or FFI bridge
- **Check usage**: Search the compiler C++ source for callers (`grep` for the function name in `compiler/src/`)
- **Check if already migrated**: Search `lib/` for TML implementations

### 3. Migration Assessment

For each function, determine the migration path per CLAUDE.md's three-tier rule:

1. **Pure TML** (strongly preferred): Functions that are pure algorithms (sorting, string manipulation, math formatting, data structures)
2. **@extern("c") FFI** (acceptable): Functions that wrap OS APIs or external libraries (file I/O, crypto, networking)
3. **Keep in C** (last resort): Functions requiring inline asm, OS-level I/O, or panic/abort handlers

### 4. Report

Present a table:
```
| Function | Lines | Callers | Migration Path | Effort |
|----------|-------|---------|---------------|--------|
| func_name | N | M calls | Pure TML / FFI / Keep | Low/Med/High |
```

And for "Pure TML" candidates, sketch the TML implementation approach using memory intrinsics (`ptr_read`, `ptr_write`, `mem_alloc`, etc.).

### IMPORTANT

Follow CLAUDE.md: NEVER add new C code. Always prefer pure TML implementations.