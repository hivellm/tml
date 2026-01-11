# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**TML (To Machine Language)** is a programming language designed specifically for LLM code generation and analysis. This repository contains:
- Complete language specification documentation in `/docs/`
- Full compiler implementation in `/compiler/`
- Standard library modules in `/lib/` (core, std, test)

**Status**: Compiler implementation with LLVM IR backend

## Build Commands

**IMPORTANT**: Always use the build scripts, not cmake directly!

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

| Rust | TML | Reason |
|------|-----|--------|
| `<T>` | `[T]` | `<` conflicts with comparison |
| `\|x\| expr` | `do(x) expr` | `\|` conflicts with OR |
| `&&` `\|\|` `!` | `and` `or` `not` | Keywords are clearer |
| `fn` | `func` | More explicit |
| `match` | `when` | More intuitive |
| `for`/`while`/`loop` | `loop` unified | Single keyword |
| `&T` / `&mut T` | `ref T` / `mut ref T` | Words over symbols |
| `trait` | `behavior` | Self-documenting |
| `#[...]` | `@...` | Cleaner directives |
| `Option[T]` | `Maybe[T]` | Intent is clear |
| `Some(x)` / `None` | `Just(x)` / `Nothing` | Self-documenting |
| `Result[T,E]` | `Outcome[T,E]` | Describes what it is |
| `Ok(x)` / `Err(e)` | `Ok(x)` / `Err(e)` | Same as Rust (concise) |
| `..` / `..=` | `to` / `through` | Readable ranges |
| `Box[T]` | `Heap[T]` | Describes storage |
| `Rc[T]` / `Arc[T]` | `Shared[T]` / `Sync[T]` | Describes purpose |
| `.clone()` | `.duplicate()` | No confusion with Git |
| `Clone` trait | `Duplicate` behavior | Consistent naming |
| `unsafe` | `lowlevel` | Less scary, accurate |
| Lifetimes `'a` | Always inferred | No syntax noise |

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
```

## Important Development Rules

**NEVER simplify or comment out tests!** When a test fails, the correct approach is to:
1. Fix the compiler or library implementation to make the test pass
2. Investigate why the feature isn't working
3. Implement missing functionality

Tests represent the specification of what the code should do. Simplifying tests to make them pass defeats the purpose of testing.

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
