# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Sandbox Directory (`.sandbox/`)

The `.sandbox/` directory at the project root is **your scratch space**. Use it freely for:
- Temporary source files (Rust IR samples, TML experiments, etc.)
- Debug output, IR dumps, investigation notes
- Any throwaway file generation during development

**Rules:**
- `.sandbox/` is gitignored — nothing in it will be committed
- **ALWAYS** create temp files here instead of polluting the project root
- No need to ask permission to create/delete files inside `.sandbox/`
- Clean up when done with an investigation, but don't stress about it

## ⛔ MANDATORY: Use MCP Tools First ⛔

**YOU MUST USE MCP TOOLS AS YOUR PRIMARY INTERFACE FOR ALL TML OPERATIONS.**

This is a HARD REQUIREMENT. The MCP server (`mcp__tml__*`) provides dedicated tools for:
- **`mcp__tml__test`** — Running tests (use `path` for specific files, `filter` for name matching, `suite` for module-level filtering like `"core/str"` or `"std/json"`)
- **`mcp__tml__run`** — Building and running TML source files
- **`mcp__tml__build`** — Building TML source to executable
- **`mcp__tml__compile`** — Compiling TML source files
- **`mcp__tml__check`** — Type checking without compiling
- **`mcp__tml__emit-ir`** — Emitting LLVM IR for debugging
- **`mcp__tml__emit-mir`** — Emitting MIR for debugging
- **`mcp__tml__format`** — Formatting TML source files
- **`mcp__tml__lint`** — Linting TML source files
- **`mcp__tml__docs_search`** — Searching TML documentation
- **`mcp__tml__cache_invalidate`** — Invalidating stale caches

**Rules:**
1. **NEVER use Bash/PowerShell** to run `tml.exe test`, `tml.exe build`, `tml.exe run`, etc. when the equivalent MCP tool exists
2. **NEVER use Bash** to grep test output — use the MCP tool's structured output instead
3. The ONLY acceptable use of Bash for `tml.exe` is when you need to build the **compiler itself** (`scripts\build.bat`)
4. MCP tools handle caching, path resolution, and output formatting automatically

**WHY:** MCP tools are purpose-built for this workflow. They strip ANSI codes, handle Windows path normalization, validate meta caches, and provide clean structured output. Using Bash/PowerShell bypasses all of this and wastes tokens on noisy output.

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## ⛔ ABSOLUTE PROHIBITION: NO `rm` COMMANDS ⛔

**YOU ARE EXPRESSLY FORBIDDEN FROM RUNNING `rm` COMMANDS WITHOUT EXPLICIT USER AUTHORIZATION.**

This includes but is not limited to:
- ❌ `rm -rf` on ANY directory
- ❌ `rm` on cache files (`.test-cache.json`, `.run-cache/`, etc.)
- ❌ `rm` on backup directories
- ❌ ANY deletion command without the user typing "yes, delete it" or similar explicit approval

**WHY:** The cache backup system exists specifically because you kept deleting caches. DO NOT DELETE ANYTHING. If you think something needs to be deleted, ASK THE USER FIRST.

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## ⛔ ABSOLUTE PROHIBITION: Never Run Tests Multiple Times to Filter Output ⛔

**YOU ARE EXPRESSLY FORBIDDEN FROM RUNNING THE TEST SUITE MULTIPLE TIMES TO GREP/FILTER DIFFERENT PARTS OF THE OUTPUT.**

The test suite takes significant time and CPU. Running it once to get results and then running it AGAIN just to grep for a different pattern is **unacceptable waste of processing and time**.

**Rules:**
1. **Run the test suite ONCE** — save or read the full output
2. **NEVER pipe test output through grep** and then re-run to pipe through a different grep
3. **NEVER run tests just to get a summary** if you already ran them and have the output
4. If you need specific data from test output, read the log file or scroll through the existing output
5. Use `mcp__tml__test` with `structured: true` to get parsed results in a single call
6. If the MCP structured output doesn't have what you need, run ONCE via Bash and redirect to a file in `.sandbox/`, then read that file as many times as needed

**WRONG (wastes 2x-5x processing time):**
```bash
# ❌ Run tests, grep for failures
tml test --no-cache 2>&1 | grep FAIL
# ❌ Run tests AGAIN, grep for timing
tml test --no-cache 2>&1 | grep -E "Slowest|Profile"
# ❌ Run tests AGAIN, grep for summary
tml test --no-cache 2>&1 | grep -E "passed|failed"
```

**CORRECT (run once, read many):**
```bash
# ✅ Run once, save output
tml test --no-cache 2>&1 > .sandbox/test_output.log
# ✅ Read the file for whatever you need
grep FAIL .sandbox/test_output.log
grep Profile .sandbox/test_output.log
```

**WHY:** Each test run recompiles ALL test suites and executes ALL tests. This takes minutes of CPU time. Running it 3 times to grep 3 different patterns wastes 2/3 of the total processing time for zero benefit.

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## ⛔ MANDATORY: Analyze Before Executing ⛔

**YOU MUST ANALYZE PROJECT PATTERNS AND CONVENTIONS BEFORE EXECUTING ANY TASK.**

This is a HARD REQUIREMENT because rushing to execute tasks without analysis leads to:
- ❌ Wrong folder structures (e.g., creating `archived/` instead of using existing `archive/`)
- ❌ Wrong naming conventions (e.g., missing date prefixes like `YYYY-MM-DD-task-name`)
- ❌ Wrong file formats (e.g., not following test patterns)
- ❌ Wasted tokens fixing your own mistakes
- ❌ Frustrated users

**BEFORE executing any task that involves project conventions, you MUST:**

1. **Check existing examples first** - Look at how similar tasks were done before
   ```bash
   # Example: Before archiving a task, check the archive folder structure
   ls rulebook/tasks/archive/
   # Learn: Tasks are named YYYY-MM-DD-task-name
   ```

2. **Read relevant documentation** - Check RULEBOOK.md, AGENTS.md, or related docs

3. **Identify patterns** - Note naming conventions, folder structures, file formats

4. **Only then execute** - After understanding the correct approach

**Examples of tasks that REQUIRE analysis first:**
- Archiving tasks → Check `archive/` folder naming pattern
- Creating tests → Check existing test file patterns (`use test`, return types, etc.)
- Creating tasks → Check `rulebook/RULEBOOK.md` for format
- Adding new modules → Check existing module structures

**WHY:** Executing quickly without analysis causes MORE errors, which requires MORE fixes, which wastes MORE tokens and time. Taking 30 seconds to analyze saves minutes of corrections.

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## ⛔ MANDATORY: Minimize C and C++ Code ⛔

**The TML project is actively migrating away from C/C++ toward pure TML. You MUST NOT add new C or C++ code unless absolutely necessary.**

This is a HARD REQUIREMENT aligned with the project roadmap (see [docs/ROADMAP.md](docs/ROADMAP.md)).

### Three-Tier Rule for New Implementations

When implementing new functionality, follow this decision hierarchy:

1. **Pure TML** (STRONGLY PREFERRED) — Use TML's existing memory intrinsics (`ptr_read`, `ptr_write`, `ptr_offset`, `mem_alloc`, `mem_free`, `copy_nonoverlapping`) to implement algorithms directly in `.tml` files. This includes: string operations, collections, formatting, sorting, search algorithms, data structures, math utilities, parsers, serialization.

2. **`@extern("c")` FFI to existing libraries** (ACCEPTABLE) — When calling external system libraries (LLVM, OpenSSL/BCrypt, zlib, libc, OS APIs). Do NOT reimplement what these libraries already provide. Declare `@extern("c")` bindings in TML and call them.

3. **New C/C++ code** (LAST RESORT ONLY) — Only for functionality that genuinely cannot be expressed in TML or as FFI bindings. Examples: OS-level I/O (print, file read/write), panic/abort handlers, test harness DLL entry points.

### What This Means in Practice

**NEVER do this:**
- ❌ Add new `.c` files to `compiler/runtime/` for algorithms that TML can express
- ❌ Add new `lowlevel` blocks in `.tml` files that call C functions for pure logic (string manipulation, collection operations, math formatting)
- ❌ Create C wrapper functions when `@extern("c")` to an existing library suffices
- ❌ Add new C++ code to the compiler for features that could be implemented as TML library code
- ❌ Use the C runtime as a shortcut instead of implementing properly in TML

**ALWAYS do this:**
- ✅ Implement new algorithms in pure TML using memory intrinsics
- ✅ Use `@extern("c")` for system APIs, crypto, compression, networking
- ✅ Keep `compiler/runtime/core/essential.c` as the ONLY essential C runtime (I/O, panic, test harness)
- ✅ When fixing a bug in existing C runtime code, consider if it's an opportunity to migrate that function to TML

### Current C Code That MUST NOT Grow

| Location | Purpose | Status |
|----------|---------|--------|
| `compiler/runtime/core/essential.c` | I/O, panic, test harness | KEEP — essential |
| `compiler/runtime/memory/mem.c` | malloc/free wrappers | KEEP — OS interface |
| `compiler/runtime/collections/` | List, HashMap, Buffer | MIGRATE — do not add code here |
| `compiler/runtime/text/` | String/Text algorithms | MIGRATE — do not add code here |
| `compiler/runtime/math/` | Number formatting | MIGRATE — do not add code here |
| `compiler/runtime/search/` | BM25, HNSW, distance | MIGRATE — do not add code here |
| `lib/std/runtime/` | Duplicate C files | MIGRATE — do not add code here |
| `lib/test/runtime/` | Coverage tracking | KEEP — lock-free atomics |

**WHY:** The project is on a path to self-hosting (compiler rewritten in TML). Every new line of C/C++ code is debt that must be rewritten later. Pure TML implementations serve double duty: they work today AND they prepare for self-hosting.

**See also:** [ROADMAP.md](docs/ROADMAP.md) Phase 4 (Runtime Migration), Phase 6 (Self-Hosting)

**VIOLATION OF THIS RULE IS UNACCEPTABLE.**

## Project Overview

**TML (To Machine Language)** is a programming language designed specifically for LLM code generation and analysis. This repository contains:

- Complete language specification documentation in `/docs/`
- Full compiler implementation in `/compiler/`
- Standard library modules in `/lib/` (core, std, test)

**Status**: Compiler implementation with LLVM IR backend

## Build Commands

**⚠️⚠️⚠️ CRITICAL: NEVER USE CMAKE DIRECTLY! ⚠️⚠️⚠️**

**THIS IS A HARD REQUIREMENT - NO EXCEPTIONS:**
- ❌ NEVER run `cmake --build`
- ❌ NEVER run `cmake -B`
- ❌ NEVER run any direct cmake commands
- ❌ NEVER use powershell/cmd to call cmake

**WHY:** Direct cmake calls CORRUPT the build directory, cause silent failures, break incremental compilation, and waste time. The build scripts handle critical environment setup that cmake alone cannot.

**ENFORCED:** The CMakeLists.txt has a build token check that will FAIL with a fatal error if you try to use cmake directly. Only the build scripts pass the required token.

**ALWAYS use the provided scripts:**

```bash
# Windows - from project root (f:\Node\hivellm\tml)
scripts\build.bat              # Debug build (default, monolithic ~100MB)
scripts\build.bat release      # Release build
scripts\build.bat --clean      # Clean build
scripts\build.bat --tests      # Also build C++ unit tests (tml_tests.exe)
scripts\build.bat --modular    # Modular build (thin launcher + plugin DLLs)

# Run tests
scripts\test.bat

# Clean
scripts\clean.bat
```

### Modular Build (Plugin Architecture)

The `--modular` flag produces a thin launcher + plugin DLLs instead of a single monolithic executable:

```bash
scripts\build.bat --modular          # Debug modular build
scripts\build.bat --modular release  # Release modular build
```

**Output structure (modular):**
| File | Size | Description |
|------|------|-------------|
| `build/debug/bin/tml.exe` | ~367KB | Thin launcher (no LLVM) |
| `build/debug/bin/tml_full.exe` | ~100MB | Monolithic fallback |
| `build/debug/bin/plugins/tml_compiler.dll` | ~100MB | Core compiler plugin |
| `build/debug/bin/plugins/tml_codegen_x86.dll` | ~75MB | LLVM codegen backend |
| `build/debug/bin/plugins/tml_tools.dll` | ~47KB | Formatter, linter, doc, search |
| `build/debug/bin/plugins/tml_test.dll` | ~47KB | Test runner, coverage |
| `build/debug/bin/plugins/tml_mcp.dll` | ~46KB | MCP server |

**How it works:**
1. `tml.exe` handles `--help`/`--version` locally (no plugins loaded)
2. All other commands load `tml_compiler.dll` via `plugin_loader`
3. Plugins discover each other via `plugins/` directory next to the exe (in `bin/plugins/`)
4. Plugin ABI is pure C (`plugin/abi.h`) to avoid C++ ABI issues across DLL boundaries
5. Compressed plugins (`.dll.zst`) are auto-decompressed and cached

**Key files:**
- `compiler/include/plugin/abi.h` — Plugin ABI (pure C interface)
- `compiler/include/plugin/loader.hpp` — Plugin loader class
- `compiler/src/plugin/loader.cpp` — Cross-platform LoadLibrary/dlopen + zstd cache
- `compiler/src/launcher/main_launcher.cpp` — Thin launcher entry point
- `compiler/src/plugin/*_plugin.cpp` — Plugin entry points for each module
- `compiler/cmake/collect_modules.cmake` — CMake module scanner

**Why scripts only?** The build scripts handle environment setup, path configuration, and proper sequencing that direct cmake calls miss. Using cmake directly can result in tests that appear to pass but actually fail silently or hang indefinitely.

**⚠️ EXACT BUILD COMMAND (MANDATORY) ⚠️**

When running a build via Bash, the correct command is:

```bash
cd /f/Node/hivellm/tml && cmd //c "scripts\\build.bat" 2>&1
```

This is the canonical build invocation. Always use this exact form (adjusting flags as needed, e.g. adding `release` or `--tests`).

## Test Commands

**⚠️ EXACT TEST + COVERAGE COMMAND (MANDATORY) ⚠️**

When running the full test suite with coverage, the correct command is:

```bash
cd f:/Node/hivellm/tml && build/debug/bin/tml.exe test --profile --verbose --no-cache --coverage 2>&1
```

This is the canonical test invocation for generating coverage reports. Always use this exact form.

### Suite-Level Test Filtering

To run tests for a specific module without running the full suite:

```bash
# Via MCP (PREFERRED):
mcp__tml__test with suite="core/str"        # Run all core::str tests
mcp__tml__test with suite="std/json"        # Run all std::json tests
mcp__tml__test with suite="core/error"      # Run all core::error tests

# Via CLI:
tml test --suite=core/str --no-cache        # Suite filter + fresh build
tml test --suite=std/collections            # Suite filter with cache
tml test --list-suites                      # Show all available suites
```

**Suite name mapping:** `core/str` → `lib/core/tests/str/`, `std/json` → `lib/std/tests/json/`, etc.

**When to use:**
- Verifying changes to a specific module (much faster than full suite)
- Investigating suite-level failures (some bugs only manifest in suite mode, not individual file mode)
- Running `--no-cache` on a targeted subset after compiler changes

**CRITICAL: NEVER DELETE TEST CACHES!**

Do NOT delete or clear any of the following cache directories:
- `build/debug/.run-cache/` - Compiled test DLLs
- `build/debug/.test-cache/` - Test results cache
- `.test-cache.json` - Test metadata cache

The test cache system is designed to automatically invalidate when:
- Source files change (hash-based detection)
- Coverage mode changes (coverage_enabled flag)
- Dependencies change

Output directories:

- `build/debug/bin/tml.exe` - Debug compiler
- `build/release/bin/tml.exe` - Release compiler
- `build/debug/bin/tml_tests.exe` - Test executable

## What This Project Is

The `/docs/` directory contains the complete TML v1.0 specification:

- Grammar (EBNF, LL(1) parser-friendly)
- Type system (I8-I128, U8-U128, F32, F64)
- Semantics (caps, effects, contracts, ownership)
- Toolchain design (CLI, testing, debug)
- IR format (canonical representation for LLM patches)

## Key Design Decisions

TML has its own identity, optimized for LLM comprehension with self-documenting syntax:

| Rust                 | TML                     | Reason                        |
| -------------------- | ----------------------- | ----------------------------- |
| `<T>`                | `[T]`                   | `<` conflicts with comparison |
| `\|x\| expr`         | `do(x) expr`            | `\|` conflicts with OR        |
| `&&` `\|\|` `!`      | `and` `or` `not`        | Keywords are clearer          |
| `fn`                 | `func`                  | More explicit                 |
| `match`              | `when`                  | More intuitive                |
| `for`/`while`/`loop` | `loop` unified          | Single keyword                |
| `&T` / `&mut T`      | `ref T` / `mut ref T`   | Words over symbols            |
| `trait`              | `behavior`              | Self-documenting              |
| `#[...]`             | `@...`                  | Cleaner directives            |
| `Option[T]`          | `Maybe[T]`              | Intent is clear               |
| `Some(x)` / `None`   | `Just(x)` / `Nothing`   | Self-documenting              |
| `Result[T,E]`        | `Outcome[T,E]`          | Describes what it is          |
| `Ok(x)` / `Err(e)`   | `Ok(x)` / `Err(e)`      | Same as Rust (concise)        |
| `..` / `..=`         | `to` / `through`        | Readable ranges               |
| `Box[T]`             | `Heap[T]`               | Describes storage             |
| `Rc[T]` / `Arc[T]`   | `Shared[T]` / `Sync[T]` | Describes purpose             |
| `.clone()`           | `.duplicate()`          | No confusion with Git         |
| `Clone` trait        | `Duplicate` behavior    | Consistent naming             |
| `unsafe`             | `lowlevel`              | Less scary, accurate          |
| Lifetimes `'a`       | Always inferred         | No syntax noise               |
| No C-style unions    | `union { ... }`         | FFI interop, low-level memory |

## Project Structure

```
tml/
├── compiler/           # C++ compiler implementation
│   ├── src/           # Source files
│   │   ├── cli/       # CLI implementation
│   │   │   ├── commands/   # Command handlers (cmd_*.cpp)
│   │   │   ├── builder/    # Build system (compile, link, cache)
│   │   │   ├── tester/     # Test runner (suite execution)
│   │   │   └── linter/     # Linting system
│   │   ├── lexer/     # Tokenizer
│   │   ├── parser/    # AST parser
│   │   ├── types/     # Type checker
│   │   ├── borrow/    # Borrow checker
│   │   ├── hir/       # High-level IR
│   │   ├── mir/       # Mid-level IR (SSA)
│   │   ├── codegen/   # LLVM IR generation
│   │   ├── query/     # Query system (demand-driven compilation)
│   │   ├── backend/   # LLVM backend + LLD linker (in-process)
│   │   ├── format/    # Code formatter
│   │   ├── plugin/    # Plugin entry points (*_plugin.cpp)
│   │   └── launcher/  # Thin launcher (modular build)
│   ├── include/       # Header files
│   │   └── plugin/    # Plugin ABI (abi.h, loader.hpp, module.hpp)
│   ├── cmake/         # CMake modules (collect_modules.cmake)
│   ├── third_party/   # Vendored deps (zstd decoder)
│   ├── runtime/       # Essential runtime (essential.c)
│   └── tests/         # Compiler unit tests (C++)
├── lib/               # TML standard libraries
│   ├── core/          # Core library (alloc, iter, slice, etc.)
│   ├── std/           # Standard library (collections, file, etc.)
│   └── test/          # Test framework (assert_eq, etc.)
├── docs/              # Language specification
├── scripts/           # Build scripts
└── build/             # Build output
    ├── debug/         # Debug binaries
    └── release/       # Release binaries
```

## Documentation Structure

```
/docs/
├── INDEX.md           # Overview and quick start
├── 01-OVERVIEW.md     # Philosophy, TML vs Rust
├── 02-LEXICAL.md      # Tokens, 32 keywords
├── 03-GRAMMAR.md      # EBNF grammar (LL(1))
├── 04-TYPES.md        # Type system
├── 05-SEMANTICS.md    # Caps, effects, contracts
├── 06-MEMORY.md       # Ownership, borrowing
├── 07-MODULES.md      # Module system
├── 08-IR.md           # Canonical IR, stable IDs
├── 09-CLI.md          # tml build/test/run
├── 10-TESTING.md      # Test framework
├── 11-DEBUG.md        # Debug, profiling
├── 12-ERRORS.md       # Error catalog
├── 13-BUILTINS.md     # Builtin types/functions
└── 14-EXAMPLES.md     # Complete examples
```

## Working with This Project

Since this is a specification project:

1. **Editing specs**: Modify files in `/docs/` directly
2. **Consistency**: Keep examples in sync across documents
3. **Grammar changes**: Update both `02-LEXICAL.md` and `03-GRAMMAR.md`
4. **New features**: Add to appropriate spec file, update `INDEX.md`

## Rulebook Integration

This project uses [@hivehub/rulebook](https://www.npmjs.com/package/@hivehub/rulebook) v3.2.0+ for task management, persistent memory, and **Ralph** (autonomous AI iteration loops). Key rules:

1. **ALWAYS read AGENTS.md first** for project standards
2. **Use Rulebook tasks** for new features: `rulebook task create <id>`
3. **Validate before commit**: `rulebook task validate <id>`
4. **Update /docs/** when modifying specifications
5. **Use Ralph for complex tasks** — for work requiring multiple iterations with fresh context per cycle

### Ralph: Autonomous Iteration Loops (v3.1+)

Ralph is an AI-driven autonomous loop that automatically solves complex development tasks through multiple iterations, resetting context between cycles to avoid exhaustion. Unlike manual iteration, Ralph handles the cycling automatically.

**How Ralph Works:**
1. **Initialization** — Generates PRD from Rulebook task description
2. **Iteration Loop** — Runs up to configurable max iterations (default: 10)
3. **Fresh Context Per Cycle** — Each iteration starts with clean context (avoids AI context exhaustion)
4. **Quality Gates** — Validates output via type-checking, linting, testing, coverage
5. **Pause/Resume** — Supports graceful pausing and resuming across sessions
6. **History Tracking** — Maintains detailed iteration history with metrics

**Ralph is configured in rulebook.json:**
```json
{
  "ralph": {
    "enabled": true,
    "maxIterations": 10,
    "tool": "claude",
    "maxContextLoss": 3
  }
}
```

**Ralph CLI Commands:**
```bash
ralph init           # Initialize and generate PRD from task
ralph run            # Execute autonomous iteration loop
ralph status         # Check current loop status
ralph history        # View iteration history
ralph pause          # Gracefully pause current loop
ralph resume         # Resume from pause point
```

**When to use Ralph:**
- Complex features requiring multiple implementation iterations (compile → fix → test → repeat)
- Refactoring with extensive test/validation cycles
- Coverage improvements requiring repeated analysis and fixes
- Tasks where fresh context per iteration prevents drift/hallucination
- Long-running work spanning multiple development sessions

**MCP Tools for Ralph (programmatic use):**
- `rulebook_ralph_init` — Initialize and generate PRD
- `rulebook_ralph_run` — Execute iteration loop
- `rulebook_ralph_status` — Check loop status
- `rulebook_ralph_get_iteration_history` — Retrieve iteration history

### Persistent Memory (MANDATORY)

This project uses a **persistent memory system** via the Rulebook MCP server (`mcp__rulebook__rulebook_memory_*`).
Memory persists across sessions — use it to maintain context between conversations.

**You MUST actively use memory to preserve context.**

**When to Save** — Use `mcp__rulebook__rulebook_memory_save` whenever you:
- Make an architectural decision (why one approach over another)
- Fix a bug (root cause and solution)
- Discover something important (codebase patterns, gotchas, constraints)
- Implement a feature (what was built, key design choices)
- Encounter an error (root cause and solution for future reference)
- Complete a task or session (summarize what was accomplished)

```
type: decision | bugfix | feature | discovery | change | refactor | observation
title: Short descriptive title
content: Detailed context (what, why, how)
tags: [relevant, tags]
```

**When to Search** — Use `mcp__rulebook__rulebook_memory_search` to find past context:
- **At the START of every new session** — search for context relevant to the current task
- When working on code you've touched before
- When the user references a past discussion or decision
- When you need context about why something was done a certain way

**Session Summary** — Before ending a session or when context is getting long, save a summary:
```
type: observation
title: "Session summary: <date or topic>"
content: "Accomplished: ... | Pending: ... | Key decisions: ..."
```

### Rulebook MCP Tools

The Rulebook MCP server provides tools beyond memory:

| Tool | Purpose |
|------|---------|
| `mcp__rulebook__rulebook_memory_save` | Save a new persistent memory |
| `mcp__rulebook__rulebook_memory_search` | Search memories (hybrid BM25+vector) |
| `mcp__rulebook__rulebook_memory_get` | Get full details for specific memory IDs |
| `mcp__rulebook__rulebook_memory_stats` | Get memory database statistics |
| `mcp__rulebook__rulebook_memory_timeline` | Get chronological context around a memory |
| `mcp__rulebook__rulebook_task_create` | Create a new Rulebook task |
| `mcp__rulebook__rulebook_task_list` | List all tasks |
| `mcp__rulebook__rulebook_task_show` | Show task details |
| `mcp__rulebook__rulebook_task_update` | Update task status |
| `mcp__rulebook__rulebook_task_validate` | Validate task format |
| `mcp__rulebook__rulebook_task_archive` | Archive a completed task |
| `rulebook_ralph_init` | Initialize Ralph and generate PRD from task |
| `rulebook_ralph_run` | Execute Ralph autonomous iteration loop |
| `rulebook_ralph_status` | Check Ralph loop status |
| `rulebook_ralph_get_iteration_history` | Retrieve Ralph iteration history |
| `mcp__rulebook__rulebook_skill_list` | List available skills |
| `mcp__rulebook__rulebook_skill_search` | Search for skills |

### tasks.md Format

**IMPORTANT**: All `rulebook/tasks/*/tasks.md` files must be **simple checklists only**.

- **NO prose explanations** - tasks.md is for tracking, not documentation
- **NO "Fixed Issues" sections** with detailed descriptions
- **NO code examples** or implementation details
- **NO root cause analysis** or technical explanations

If you need to document implementation details, root causes, or technical explanations, put them in `proposal.md` instead.

**Correct format:**

```markdown
# Tasks: Feature Name

**Status**: In Progress (X%)

## Phase 1: Phase Name

- [x] 1.1.1 Completed task description
- [ ] 1.1.2 Pending task description
- [ ] 1.1.3 Another pending task

## Phase 2: Another Phase

- [ ] 2.1.1 Task description
```

**Wrong format (DO NOT do this):**

```markdown
## Fixed Issues

### Bug Name (FIXED 2026-01-18)

**Status**: Fixed in `file.cpp`

The bug was caused by X. Root cause was Y. We fixed it by Z.
```

## File Extension

TML source files use `.tml` extension.

## Compiler CLI Options

The TML compiler (`tml`) supports these build flags:

```bash
# Basic build (uses query-based pipeline with incremental compilation by default)
tml build file.tml                # Build executable
tml build file.tml --verbose      # Show detailed output
tml build file.tml --legacy       # Use traditional sequential pipeline (fallback)

# Output types
tml build file.tml --crate-type=bin      # Executable (default)
tml build file.tml --crate-type=lib      # Static library
tml build file.tml --crate-type=dylib    # Dynamic library
tml build file.tml --crate-type=rlib     # TML library format

# Optimization
tml build file.tml --release      # Build with -O3 optimization
tml build file.tml -O0/-O1/-O2/-O3  # Set optimization level
tml build file.tml --lto          # Enable Link-Time Optimization
tml build file.tml --debug/-g     # Include debug info

# Caching & Incremental
tml build file.tml --no-cache     # Force full recompilation (disables incremental)

# Diagnostics
tml build file.tml --emit-ir      # Emit LLVM IR (.ll files)
tml build file.tml --emit-mir     # Emit MIR for debugging
tml build file.tml --emit-header  # Generate C header for FFI
tml build file.tml --time         # Show compiler phase timings

# Preprocessor / Conditional Compilation
tml build file.tml -DDEBUG        # Define preprocessor symbol
tml build file.tml -DVERSION=1.0  # Define symbol with value
tml build file.tml --define=FEAT  # Alternative syntax
tml build file.tml --target=x86_64-unknown-linux-gnu  # Cross-compile

# Backend selection (EXPERIMENTAL)
tml build file.tml --backend=llvm       # Use LLVM backend (default)
tml build file.tml --backend=cranelift  # Use Cranelift backend (experimental, NOT ready for use)
```

## Compilation Architecture

The TML compiler uses a **demand-driven query system** (like rustc) as the default build pipeline:

```
Source (.tml) → [QueryContext] → ReadSource → Tokenize → Parse → Typecheck
             → Borrowcheck → HirLower → MirBuild → CodegenUnit → [Backend] → .obj → [LLD] → .exe
```

**Key components:**
- **Query System**: Each compilation stage is a memoized query with dependency tracking
- **Incremental Compilation**: Fingerprints + dependency edges persisted to `.incr-cache/incr.bin`
- **GREEN path**: No source changes → cached LLVM IR loaded from disk, entire pipeline skipped
- **RED path**: Source changed → affected queries recomputed, downstream checked for changes
- **Embedded LLVM** (default backend): IR compiled to .obj in-process (no clang subprocess)
- **Cranelift** (experimental backend, in development): Alternative backend for faster debug builds. **Not ready for production use.**
- **Embedded LLD**: Linking done in-process (no linker subprocess)

**Key files:**
- `compiler/include/query/query_context.hpp` - QueryContext with `force<R>()` template
- `compiler/include/query/query_key.hpp` - 8 query key/result types
- `compiler/include/query/query_incr.hpp` - PrevSessionCache, IncrCacheWriter, binary format
- `compiler/src/query/query_core.cpp` - Provider implementations (8 stage wrappers)
- `compiler/src/backend/llvm_backend.cpp` - LLVM C API for in-memory compilation
- `compiler/src/backend/lld_linker.cpp` - LLD in-process linker (COFF/ELF/MachO)
- `compiler/include/codegen/codegen_backend.hpp` - Backend abstraction interface
- `compiler/src/codegen/cranelift/` - Cranelift backend (experimental)
````

## Conditional Compilation

TML supports C-style preprocessor directives for platform-specific code:

```tml
#if WINDOWS
func get_home() -> Str { return env::var("USERPROFILE") }
#elif UNIX
func get_home() -> Str { return env::var("HOME") }
#endif

#ifdef DEBUG
func log(msg: Str) { print("[DEBUG] {msg}\n") }
#endif

#if defined(FEATURE_A) && !defined(FEATURE_B)
func feature_a_only() { ... }
#endif
```

**Predefined Symbols:**

- OS: `WINDOWS`, `LINUX`, `MACOS`, `ANDROID`, `IOS`, `FREEBSD`, `UNIX`, `POSIX`
- Architecture: `X86_64`, `X86`, `ARM64`, `ARM`, `WASM32`, `RISCV64`
- Pointer width: `PTR_32`, `PTR_64`
- Endianness: `LITTLE_ENDIAN`, `BIG_ENDIAN`
- Environment: `MSVC`, `GNU`, `MUSL`
- Build mode: `DEBUG`, `RELEASE`, `TEST`

**Key Files:**

- `compiler/include/preprocessor/preprocessor.hpp` - Preprocessor interface
- `compiler/src/preprocessor/preprocessor.cpp` - Full implementation
- `compiler/src/cli/builder/helpers.cpp` - CLI integration helpers

## MANDATORY: Rust-as-Reference IR Methodology

**When fixing codegen bugs or optimizing the TML compiler's LLVM IR output, you MUST use Rust as the reference implementation.**

This is a HARD REQUIREMENT. The TML compiler aims to produce IR of the same quality as `rustc`. Rust's IR is the gold standard for correctness, safety, and optimization.

### Workflow (MUST follow for every codegen task)

1. **Write equivalent code in BOTH languages:**
   - Create `.sandbox/temp_<feature>.rs` (Rust version)
   - Create `.sandbox/temp_<feature>.tml` (TML version with equivalent semantics)
   - Both files must exercise the EXACT same pattern (same struct, same methods, same calls)

2. **Generate IR from both compilers:**
   ```bash
   # Rust IR (debug)
   rustc --edition 2021 --emit=llvm-ir -C opt-level=0 .sandbox/temp_<feature>.rs -o .sandbox/temp_<feature>_rust_debug.ll

   # Rust IR (release)
   rustc --edition 2021 --emit=llvm-ir -C opt-level=3 .sandbox/temp_<feature>.rs -o .sandbox/temp_<feature>_rust_release.ll

   # TML IR (debug)
   tml build .sandbox/temp_<feature>.tml --emit-ir --legacy
   # Then copy: cp build/debug/temp_<feature>.ll .sandbox/temp_<feature>_tml_debug.ll

   # TML IR (release)
   tml build .sandbox/temp_<feature>.tml --emit-ir --legacy --release
   # Then copy: cp build/debug/temp_<feature>.ll .sandbox/temp_<feature>_tml_release.ll
   ```

3. **Compare function-by-function:**
   - Instruction count (TML must not exceed 2x Rust for equivalent logic)
   - Type layouts (struct/enum sizes should match)
   - Alloca count (TML should not have allocas that Rust avoids)
   - Safety features (overflow checks, null checks)
   - Call overhead (unnecessary wrappers, extra indirection)

4. **Fix the TML codegen** to match or exceed Rust's quality, then verify with the test suite.

### When to Use This Methodology

- Fixing ANY bug in `compiler/src/codegen/`
- Implementing new codegen features (new expressions, new types)
- Optimizing IR output for specific patterns
- Investigating "wrong code" or "miscompilation" bugs
- Adding new type layouts (enums, generics, closures)

### Key Optimization Targets (from IR comparison)

| Issue | Current TML | Rust Reference | Priority |
|-------|-------------|---------------|----------|
| `Maybe[I32]` layout | 16 bytes `{ i32, [1 x i64] }` | 8 bytes `{ i32, i32 }` | HIGH |
| Struct constructors | alloca+store+load (10 instr) | `insertvalue` (3 instr) | HIGH |
| Runtime declarations | 500+ lines unconditionally | Only what's used | MEDIUM |
| Integer arithmetic | `add nsw` (UB on overflow) | Checked with panic | MEDIUM |
| Exception handling | None | `invoke` + `cleanuppad` | LOW |

### Rulebook Task (Living Document)

Full task details: `rulebook/tasks/optimize-codegen-like-rust/`

**This task is incremental and deferred.** Whenever you discover a codegen inefficiency, bug, or interesting pattern during ANY work (iterator fixes, closure fixes, generic instantiation, etc.), you MUST update the task's `tasks.md` with the new finding — add new checklist items, add notes to existing items, or create new phases. The task serves as a running log of everything that needs optimization. Dedicated execution happens later when the compiler is stable.

**VIOLATION OF THIS METHODOLOGY IS UNACCEPTABLE when working on codegen.**

## Important Development Rules

**NEVER simplify or comment out tests!** When a test fails, the correct approach is to:

1. Fix the compiler or library implementation to make the test pass
2. Investigate why the feature isn't working
3. Implement missing functionality

Tests represent the specification of what the code should do. Simplifying tests to make them pass defeats the purpose of testing.

### MANDATORY: No Test Circumvention

**This is NON-NEGOTIABLE. You MUST follow these rules:**

1. **NEVER move tests to `pending/` folders** - All tests must live in the main `tests/` directory
2. **NEVER create placeholder implementations** - Implement the actual functionality
3. **NEVER simplify test assertions** - Fix the code, not the test
4. **NEVER create stubs** - Write real implementations
5. **NEVER comment out failing tests** - Fix the underlying issue
6. **NEVER skip tests** - Every test must pass

When a test fails:
- Investigate the root cause in the compiler or library
- Implement the missing codegen, type checking, or runtime functionality
- Keep working until the test passes
- Do NOT invent creative ways to bypass the test

If a test reveals a bug that requires significant work:
- Create a task in `rulebook/tasks/` to track the fix
- Fix the bug properly, don't defer it
- The test stays in place and must pass before committing

### MANDATORY: Incremental Test Development

**This is NON-NEGOTIABLE. You MUST follow this workflow when writing tests:**

1. **Write tests incrementally** - Create 1-3 tests at a time, NOT entire test files at once
2. **Test immediately after writing** - Run the individual test file before moving to the next
3. **Fix errors before proceeding** - If a test fails, fix it before writing more tests
4. **Use individual test execution** - NEVER run full test suite when developing tests

**Correct workflow:**
```bash
# Write 1-3 tests in a file
# Run ONLY that specific test file:
tml test path/to/specific.test.tml

# OR use MCP tool for individual file:
mcp__tml__test with path parameter

# Fix any errors
# Then write more tests
# Repeat
```

**WRONG workflow (DO NOT DO THIS):**
```bash
# ❌ Write 20 tests at once
# ❌ Run full suite: tml test --no-cache
# ❌ Get 15 errors and struggle to fix them all
```

**Why this matters:**
- Creating many tests at once leads to cascading errors that are hard to debug
- Running full test suite is slow and wasteful when only testing new code
- The compiler supports individual test file execution - USE IT
- Fixing errors one by one is faster than fixing 10+ errors at once

**Coverage Updates:**
- After completing a successful block of tests, run coverage: `tml test --coverage`
- Coverage reports are how progress is tracked and new features are planned
- Always update coverage after finishing a module's tests

## Key CLI Files

The CLI is organized into subfolders:

### Commands (`compiler/src/cli/commands/`)

- `cmd_build.cpp/.hpp` - Main build command implementation
- `cmd_test.cpp/.hpp` - Test command (TestOptions struct)
- `cmd_cache.cpp/.hpp` - Cache management
- `cmd_debug.cpp/.hpp` - Debug commands (lex, parse, check)
- `cmd_format.cpp/.hpp` - Code formatting
- `cmd_init.cpp/.hpp` - Project initialization
- `cmd_lint.cpp/.hpp` - Linting
- `cmd_pkg.cpp/.hpp` - Package management
- `cmd_rlib.cpp/.hpp` - Library format handling

### Builder (`compiler/src/cli/builder/`)

- `build.cpp` - Main build orchestration (`run_build_with_queries()` default, `run_build_impl()` legacy)
- `parallel_build.cpp/.hpp` - Multi-threaded compilation
- `object_compiler.cpp/.hpp` - LLVM IR to object file compilation (in-process via LLVM C API)
- `build_cache.cpp/.hpp` - MIR cache for incremental compilation
- `build_config.cpp/.hpp` - Build configuration
- `compiler_setup.cpp/.hpp` - Toolchain discovery (clang, MSVC)
- `dependency_resolver.cpp/.hpp` - Module dependency resolution
- `rlib.cpp/.hpp` - TML library format (.rlib)

### Query System (`compiler/src/query/`)

- `query_context.hpp/cpp` - QueryContext with `force<R>()`, incremental load/save, green checking
- `query_key.hpp` - 8 query key types + result types (ReadSource through CodegenUnit)
- `query_cache.hpp/cpp` - Thread-safe memoization cache with fingerprints
- `query_core.hpp/cpp` - Provider implementations wrapping each compilation stage
- `query_deps.hpp/cpp` - DependencyTracker with cycle detection
- `query_fingerprint.hpp/cpp` - 128-bit CRC32C fingerprinting
- `query_incr.hpp/cpp` - PrevSessionCache, IncrCacheWriter, binary serialization
- `query_provider.hpp/cpp` - Provider registry with O(1) lookup

### Backend (`compiler/src/backend/`)

- `llvm_backend.cpp` - LLVM C API wrapper (in-memory IR→obj compilation)
- `lld_linker.cpp` - LLD in-process linker (COFF/ELF/MachO)

### Tester (`compiler/src/cli/tester/`)

- `test_runner.cpp/.hpp` - DLL-based test execution
- `suite_execution.cpp` - Suite mode (multiple tests per DLL)
- `run.cpp` - Test orchestration
- `discovery.cpp` - Test file discovery
- `benchmark.cpp` - Benchmark execution
- `fuzzer.cpp` - Fuzz testing

### Core

- `compiler/src/cli/dispatcher.cpp` - CLI argument parsing (routes to query or legacy pipeline)
- `compiler/src/cli/utils.cpp/.hpp` - Shared utilities
- `compiler/src/cli/diagnostic.cpp/.hpp` - Error formatting

## File Editing Best Practices

**IMPORTANT**: When editing multiple files, follow this pattern:

1. **Read and edit sequentially** - Do NOT read multiple files in parallel and then try to edit them all. The "file read" context is lost between tool calls.
2. **One file at a time** - Read a file, edit it immediately, then move to the next file.
3. **Never batch reads before edits** - This causes "File has not been read yet" errors.

```
# CORRECT pattern:
Read file1 -> Edit file1 -> Read file2 -> Edit file2

# WRONG pattern (causes errors):
Read file1, file2, file3 (parallel) -> Edit file1 (works) -> Edit file2 (FAILS)
```
