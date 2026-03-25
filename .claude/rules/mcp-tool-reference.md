# MCP Tool Reference — Complete Guide for LLM Usage

Every MCP tool call is logged to `mcp-call-log.jsonl` for research. Use tools intentionally.

## Compilation & Execution Tools

| Tool | Purpose | Key Params | When to Use |
|------|---------|------------|-------------|
| `mcp__tml__compile` | Compile a .tml file | `file` (required), `optimize`, `output`, `release` | Compile a single file to check for errors |
| `mcp__tml__build` | Full build with options | `file` (required), `optimize`, `output`, `release`, `crate_type` | Build executables, libraries, or rlibs |
| `mcp__tml__run` | Build and execute | `file` (required), `args`, `release` | Run a .tml file and see output |
| `mcp__tml__check` | Type check only | `file` (required) | Fast check without codegen — use for type errors |

## Testing Tools

| Tool | Purpose | Key Params | When to Use |
|------|---------|------------|-------------|
| `mcp__tml__test` | Run tests | `path`, `filter`, `suite`, `structured`, `debug_layers`, `coverage`, `no_cache`, `fail_fast`, `profile` | Primary testing tool — always use instead of Bash |

### Test Tool Param Guide

- **`suite`** = module filter: `"core/str"`, `"std/json"`, `"core/fmt"` — runs only that module's tests
- **`path`** = file path: `"lib/core/tests/str/basic.test.tml"` — runs single file
- **`filter`** = name substring: `"split"` — runs tests matching the filter
- **`structured`** = `true` — returns `{total, passed, failed, failures[]}` JSON instead of text
- **`debug_layers`** = `true` — on failure, emits HIR + MIR + LLVM IR for the failing function with diagnosis hints
- **`no_cache`** = `true` — force recompile (use after C++ compiler changes)
- **`coverage`** = `true` — run with coverage tracking

### When to Use `debug_layers`

USE `debug_layers: true` when:
- A test fails and you need to understand WHY at the IR level
- You suspect a codegen bug (wrong instruction, ABI mismatch, type layout)
- The error message alone doesn't tell you which compilation layer is wrong
- You need to compare what HIR/MIR/LLVM IR was generated

DO NOT use `debug_layers` for:
- Routine test runs (it adds ~10-30s per failing file)
- Tests that fail due to missing imports or syntax errors (visible without IR)

## Diagnostic Tools (IR-Level Debugging)

| Tool | Purpose | Key Params | When to Use |
|------|---------|------------|-------------|
| `mcp__tml__emit-ir` | Emit LLVM IR | `file` (required), `function`, `offset`, `limit`, `optimize` | View generated LLVM IR for codegen debugging |
| `mcp__tml__emit-mir` | Emit MIR | `file` (required) | View Mid-level IR (SSA, basic blocks, control flow) |
| `mcp__tml__explain` | Explain error code | `code` (required, e.g. "T001") | Understand what a compiler error means |

### IR Tool Usage Patterns

**For codegen bugs** (wrong output, crash, ABI mismatch):
```
1. mcp__tml__test with debug_layers=true  → see all layers at once
2. OR: mcp__tml__emit-ir with function="test_name"  → see specific function's LLVM IR
3. Compare with Rust reference IR if needed
```

**For type system bugs** (wrong inference, missing impl):
```
1. mcp__tml__check  → get type error details
2. mcp__tml__emit-mir  → see resolved types in MIR
```

**For optimization bugs** (wrong value after opt pass):
```
1. mcp__tml__emit-ir with optimize="O0"  → unoptimized IR
2. mcp__tml__emit-ir with optimize="O3"  → optimized IR
3. Compare: what did the optimizer change?
```

## Documentation Tools

| Tool | Purpose | Key Params | When to Use |
|------|---------|------------|-------------|
| `mcp__tml__docs_search` | Search docs | `query` (required), `kind`, `module`, `limit`, `mode` | Find types, functions, behaviors by keyword |
| `mcp__tml__docs_get` | Get full docs | `id` (required, e.g. "core::str::split") | Read complete API for a specific item |
| `mcp__tml__docs_list` | List module items | `module` (required, e.g. "std::sync"), `kind` | See everything in a module |
| `mcp__tml__docs_resolve` | Resolve short name | `name` (required, e.g. "HashMap") | Find the full path of a type/function |

### ALWAYS search docs before:
- Writing a new TML module (check if type already exists)
- Using `impl`, `loop`, `when`, enums (confirm syntax)
- Using `lowlevel` (search for safe alternative first)

## Code Quality Tools

| Tool | Purpose | Key Params | When to Use |
|------|---------|------------|-------------|
| `mcp__tml__format` | Format source | `file` (required), `check` | Auto-format TML files |
| `mcp__tml__lint` | Lint source | `file` (required), `fix` | Check for style issues |
| `mcp__tml__cache_invalidate` | Clear cache | `files` (required), `verbose` | When cached results seem stale |

## Project Tools

| Tool | Purpose | Key Params | When to Use |
|------|---------|------------|-------------|
| `mcp__tml__project_build` | Build compiler | `mode`, `target`, `clean`, `tests` | Build tml.exe from C++ sources |
| `mcp__tml__project_coverage` | Coverage data | `module`, `sort`, `refresh`, `limit` | View test coverage stats |
| `mcp__tml__project_structure` | Module tree | `module`, `depth`, `show_files` | See project file organization |
| `mcp__tml__project_affected-tests` | Changed tests | `base`, `run`, `verbose` | Find which tests to run after changes |
| `mcp__tml__project_artifacts` | Build outputs | `config`, `kind` | Check binary sizes, cache state |
| `mcp__tml__project_slow-tests` | Slow test analysis | `sort`, `limit`, `threshold` | Find compilation bottlenecks |

## Debugging Decision Tree

```
Test failing?
├─ Compilation error → mcp__tml__check (type errors) or mcp__tml__emit-ir (codegen errors)
├─ Assertion failure → mcp__tml__test with debug_layers=true
│  ├─ HIR looks wrong → Bug in type checker or HIR builder
│  ├─ MIR looks wrong → Bug in MIR builder or optimization pass
│  ├─ LLVM IR looks wrong → Bug in MIR codegen (emit_call_inst, types, etc.)
│  └─ All IR looks correct → Bug in C runtime or library logic
├─ Crash (exit code != 0) → mcp__tml__test with debug_layers=true
│  └─ Check LLVM IR for ABI mismatches, sret issues, type layout errors
└─ Don't know what's wrong → mcp__tml__explain with the error code
```

## Tool Call Logging (Research)

Every `mcp__tml__*` call is automatically logged to `mcp-call-log.jsonl`:
- **Tool name** + **parameters** (no output content)
- **Duration** in milliseconds
- **Session ID** + **condition** (baseline vs debug-layers)
- **Sequence number** for ordering

This data is used for LLM debugging behavior research. Use tools intentionally — prefer the most specific tool for the task instead of broad exploration.