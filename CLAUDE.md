# CLAUDE.md

## Sandbox (`.sandbox/`)

Scratch space for temp files, IR dumps, experiments. Gitignored. Use freely, no permission needed.

## Hard Rules

### Use MCP Tools First

**ALWAYS use `mcp__tml__*` tools** for test/build/run/check/emit-ir/emit-mir/format/lint/docs/cache operations. NEVER use Bash to run `tml.exe` when an MCP tool exists. Only exception: building the **compiler itself** (`scripts\build.bat`).

### No `rm` Commands

**NEVER run `rm` without explicit user authorization.** No cache deletion, no directory removal. Ask first.

### No Repeated Test Runs

Run the test suite **ONCE**. Use `structured: true` for parsed results. If you need to grep output, save to `.sandbox/` and read the file multiple times. Never re-run tests just to filter differently.

### Analyze Before Executing

Check existing patterns/conventions before creating files or directories. Look at how similar things were done before. Wrong naming/structure wastes more time than the analysis.

### Minimize C/C++ Code

The project is migrating to pure TML. Priority order for new implementations:
1. **Pure TML** (preferred) — memory intrinsics, algorithms in `.tml` files
2. **`@extern("c")` FFI** (acceptable) — bindings to system libraries
3. **New C/C++ code** (last resort) — only for OS-level I/O, panic handlers, test harness

C runtime directories marked MIGRATE (`collections/`, `text/`, `math/`, `search/`) must not grow. See [ROADMAP.md](docs/ROADMAP.md).

### No Test Circumvention

Never simplify, skip, comment out, or move tests. Fix the compiler/library instead. Write tests incrementally (1-3 at a time), run individually via MCP, fix before writing more.

### Rust-as-Reference IR Methodology

When working on codegen (`compiler/src/codegen/`), always compare TML IR against Rust IR. Write equivalent `.rs` and `.tml` files in `.sandbox/`, generate IR from both, compare function-by-function. Log findings in `.rulebook/tasks/optimize-codegen-like-rust/tasks.md`.

## Project Overview

**TML (To Machine Language)** — programming language for LLM code generation. C++ compiler with LLVM IR backend. Source files use `.tml` extension.

### Key Syntax (vs Rust)

| Rust | TML | Reason |
|------|-----|--------|
| `<T>` | `[T]` | No comparison conflicts |
| `fn` / `match` / `trait` | `func` / `when` / `behavior` | Self-documenting |
| `&&` `\|\|` `!` | `and` `or` `not` | Keywords over symbols |
| `&T` / `&mut T` | `ref T` / `mut ref T` | Words over symbols |
| `Option` / `Result` | `Maybe` / `Outcome` | Intent-revealing |
| `Some`/`None` | `Just`/`Nothing` | Self-documenting |
| `unsafe` | `lowlevel` | Accurate |
| `#[...]` | `@...` | Cleaner |
| `for`/`while`/`loop` | `loop` unified | Single keyword |

### Project Structure

```
tml/
├── compiler/           # C++ compiler
│   ├── src/            # lexer, parser, types, borrow, hir, mir, codegen, query, backend, cli, plugin
│   ├── include/        # Headers (including plugin ABI)
│   ├── runtime/        # Essential C runtime (essential.c, mem.c)
│   └── tests/          # C++ unit tests
├── lib/                # TML standard libraries
│   ├── core/           # Core (alloc, iter, slice, simd, fmt, etc.)
│   ├── std/            # Std (collections, file, json, etc.)
│   └── test/           # Test framework
├── docs/               # Language spec (01-OVERVIEW through 14-EXAMPLES)
├── scripts/            # Build scripts (build.bat, test.bat, clean.bat)
└── build/              # Output (debug/, release/)
```

## Build Commands

**NEVER use cmake directly** — CMakeLists.txt enforces a build token. Always use scripts:

```bash
# Canonical build command:
cd /f/Node/hivellm/tml && cmd //c "scripts\\build.bat" 2>&1

# Variants:
scripts\build.bat              # Debug (monolithic ~100MB)
scripts\build.bat release      # Release
scripts\build.bat --clean      # Clean build
scripts\build.bat --tests      # Also build tml_tests.exe
scripts\build.bat --modular    # Thin launcher + plugin DLLs
```

### Modular Build

`--modular` produces `tml.exe` (~367KB launcher) + `plugins/` DLLs. Plugin ABI is pure C (`plugin/abi.h`). Compressed plugins (`.dll.zst`) are auto-decompressed.

## Test Commands

```bash
# Full suite with coverage:
cd f:/Node/hivellm/tml && build/debug/bin/tml.exe test --profile --verbose --no-cache --coverage 2>&1

# Suite-level filtering (prefer MCP tools):
mcp__tml__test with suite="core/str"    # Maps to lib/core/tests/str/
mcp__tml__test with path="file.test.tml" # Individual file
```

**Never delete test caches** (`build/debug/.run-cache/`, `.test-cache/`, `.test-cache.json`). They auto-invalidate on source changes.

### Output Paths

- `build/debug/bin/tml.exe` — Debug compiler
- `build/release/bin/tml.exe` — Release compiler
- `build/debug/bin/tml_tests.exe` — C++ unit tests

## Compiler CLI Options

```bash
tml build file.tml                    # Query-based pipeline (default)
tml build file.tml --legacy           # Sequential pipeline (fallback)
tml build file.tml --crate-type=bin|lib|dylib|rlib
tml build file.tml --release|-O0|-O1|-O2|-O3|--lto|--debug
tml build file.tml --no-cache         # Force recompilation
tml build file.tml --emit-ir|--emit-mir|--emit-header|--time
tml build file.tml -DDEBUG|-DVERSION=1.0|--define=FEAT
tml build file.tml --target=x86_64-unknown-linux-gnu
tml build file.tml --backend=llvm|cranelift  # cranelift is experimental
```

## Compilation Architecture

```
Source (.tml) → QueryContext → ReadSource → Tokenize → Parse → Typecheck
             → Borrowcheck → HirLower → MirBuild → CodegenUnit → LLVM → .obj → LLD → .exe
```

- **Query System**: Memoized stages with dependency tracking, incremental via `.incr-cache/incr.bin`
- **GREEN path**: No changes → cached IR loaded, pipeline skipped
- **RED path**: Changed → affected queries recomputed
- **Embedded LLVM + LLD**: All in-process (no subprocesses)

## Conditional Compilation

```tml
#if WINDOWS
func get_home() -> Str { return env::var("USERPROFILE") }
#elif UNIX
func get_home() -> Str { return env::var("HOME") }
#endif
```

**Predefined symbols**: OS (`WINDOWS`, `LINUX`, `MACOS`, `UNIX`, etc.), Arch (`X86_64`, `ARM64`, `WASM32`, etc.), Width (`PTR_32`/`PTR_64`), Endian, Env (`MSVC`, `GNU`), Mode (`DEBUG`, `RELEASE`, `TEST`).

## Rulebook Integration

Uses [@hivehub/rulebook](https://www.npmjs.com/package/@hivehub/rulebook) v3.2+ for task management and persistent memory.

- **Tasks**: `.rulebook/tasks/` — create with `rulebook task create <id>`, validate before commit
- **Ralph**: Autonomous iteration loops for complex tasks (init → run → fresh context per cycle → quality gates)
- **Memory**: Save decisions, bugfixes, discoveries via `mcp__rulebook__rulebook_memory_save`. Search at session start via `mcp__rulebook__rulebook_memory_search`.
- **tasks.md format**: Simple checklists only. No prose, no code, no root cause analysis. Put details in `proposal.md`.

## Key Compiler Files

| Area | Key Files |
|------|-----------|
| **CLI** | `dispatcher.cpp`, `cmd_build.cpp`, `cmd_test.cpp` |
| **Builder** | `build.cpp` (query default, legacy fallback), `object_compiler.cpp`, `build_cache.cpp`, `dependency_resolver.cpp` |
| **Query** | `query_context.hpp/cpp`, `query_key.hpp`, `query_core.cpp`, `query_incr.hpp/cpp` |
| **Backend** | `llvm_backend.cpp` (LLVM C API), `lld_linker.cpp` (in-process) |
| **Tester** | `test_runner.cpp`, `suite_execution.cpp`, `discovery.cpp` |
| **Codegen** | `codegen_backend.hpp`, `llvm_codegen_backend.cpp` |
| **Plugin** | `plugin/abi.h`, `plugin/loader.cpp`, `*_plugin.cpp` |
