---
name: fix-codegen
description: Fix a codegen bug in the TML compiler. Use when the user says "fix codegen", "corrige codegen", "codegen bug", or when a test fails due to incorrect LLVM IR generation.
user-invocable: true
argument-hint: "<description of the bug or failing test>"
---

**Delegation**: Use the Agent tool to dispatch a `codegen-debugger` agent with `model: opus` for this task.

## Codegen Bug Fix Workflow

### 1. Reproduce

If a test file is given, run it:
- Use `mcp__tml__test` with `path`, `verbose: true`, `no_cache: true`

If a description is given, write a minimal `.tml` reproducer in `.sandbox/`.

### 2. Emit IR

Use `mcp__tml__emit-ir` on the failing file to see the generated LLVM IR.

### 3. Trace the Bug

Identify which codegen phase produces the bad IR:
- Search `compiler/src/codegen/` for the relevant emission code
- Trace the value through: Source → HIR → MIR → LLVM IR
- Compare with Rust's IR for the equivalent code

### 4. Fix

Edit the compiler C++ source to fix the codegen.

### 5. Verify

1. Rebuild: `/build`
2. Re-run the failing test
3. Run affected suites: `/verify`
4. Emit IR again to confirm the fix

### IMPORTANT

- Follow CLAUDE.md Rust-as-Reference methodology
- NEVER simplify tests — fix the compiler
- Update memory with the fix details for future reference
