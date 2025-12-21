# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**TML (To Machine Language)** is a programming language specification designed specifically for LLM code generation and analysis. This repository contains the complete language specification documentation.

**Status**: Specification only (no implementation yet)

## What This Project Is

This is a **language specification project**, not an implementation. The `/docs/` directory contains the complete TML v1.0 specification:

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

TML source files use `.tml` extension (not implemented yet, specification only).
