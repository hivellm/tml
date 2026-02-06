# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ⛔ ABSOLUTE PROHIBITION: NO `rm` COMMANDS ⛔

**YOU ARE EXPRESSLY FORBIDDEN FROM RUNNING `rm` COMMANDS WITHOUT EXPLICIT USER AUTHORIZATION.**

This includes but is not limited to:
- ❌ `rm -rf` on ANY directory
- ❌ `rm` on cache files (`.test-cache.json`, `.run-cache/`, etc.)
- ❌ `rm` on backup directories
- ❌ ANY deletion command without the user typing "yes, delete it" or similar explicit approval

**WHY:** The cache backup system exists specifically because you kept deleting caches. DO NOT DELETE ANYTHING. If you think something needs to be deleted, ASK THE USER FIRST.

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
scripts\build.bat              # Debug build (default)
scripts\build.bat release      # Release build
scripts\build.bat --clean      # Clean build
scripts\build.bat --no-tests   # Skip tests

# Run tests
scripts\test.bat

# Clean
scripts\clean.bat
```

**Why scripts only?** The build scripts handle environment setup, path configuration, and proper sequencing that direct cmake calls miss. Using cmake directly can result in tests that appear to pass but actually fail silently or hang indefinitely.

## Test Cache Management

**⚠️⚠️⚠️ CRITICAL: NEVER USE --no-cache WITHOUT PERMISSION! ⚠️⚠️⚠️**

**HARD RULE:** The `--no-cache` flag requires **EXPLICIT USER CONFIRMATION** before use.

- ❌ NEVER run `tml test --no-cache` without asking first
- ❌ NEVER run `tml build --no-cache` without asking first
- ✅ ALWAYS use the MCP tool `mcp__tml__test` which respects the cache
- ✅ ALWAYS run tests WITHOUT `--no-cache` by default

**If you think you need --no-cache:** ASK THE USER FIRST with AskUserQuestion tool. The cache system auto-invalidates changed files - you almost NEVER need `--no-cache`.

**CRITICAL: NEVER DELETE TEST CACHES!**

Do NOT delete or clear any of the following cache directories:
- `build/debug/.run-cache/` - Compiled test DLLs
- `build/debug/.test-cache/` - Test results cache
- `.test-cache.json` - Test metadata cache

The test cache system is designed to automatically invalidate when:
- Source files change (hash-based detection)
- Coverage mode changes (coverage_enabled flag)
- Dependencies change

**Why not delete caches or use --no-cache?** Forces full recompilation of ALL test DLLs, which is slow and unnecessary. The cache invalidation logic handles all cases correctly.

Output directories:

- `build/debug/tml.exe` - Debug compiler
- `build/release/tml.exe` - Release compiler
- `build/debug/tml_tests.exe` - Test executable

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
│   │   └── format/    # Code formatter
│   ├── include/       # Header files
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

This project uses @hivellm/rulebook for task management. Key rules:

1. **ALWAYS read AGENTS.md first** for project standards
2. **Use Rulebook tasks** for new features: `rulebook task create <id>`
3. **Validate before commit**: `rulebook task validate <id>`
4. **Update /docs/** when modifying specifications

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
# Basic build
tml build file.tml                # Build executable
tml build file.tml --verbose      # Show detailed output

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

# Caching
tml build file.tml --no-cache     # Force full recompilation

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
```
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

- `build.cpp` - Main build orchestration
- `parallel_build.cpp/.hpp` - Multi-threaded compilation
- `object_compiler.cpp/.hpp` - LLVM IR to object file compilation
- `build_cache.cpp/.hpp` - MIR cache for incremental compilation
- `build_config.cpp/.hpp` - Build configuration
- `compiler_setup.cpp/.hpp` - Toolchain discovery (clang, MSVC)
- `dependency_resolver.cpp/.hpp` - Module dependency resolution
- `rlib.cpp/.hpp` - TML library format (.rlib)

### Tester (`compiler/src/cli/tester/`)

- `test_runner.cpp/.hpp` - DLL-based test execution
- `suite_execution.cpp` - Suite mode (multiple tests per DLL)
- `run.cpp` - Test orchestration
- `discovery.cpp` - Test file discovery
- `benchmark.cpp` - Benchmark execution
- `fuzzer.cpp` - Fuzz testing

### Core

- `compiler/src/cli/dispatcher.cpp` - CLI argument parsing
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
