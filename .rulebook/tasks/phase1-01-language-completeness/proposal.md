# Proposal: TML Language Completeness Roadmap

## Status: PROPOSED

## Why

TML has a **production-quality compiler** (~182K lines C++, 50 MIR optimization passes, full borrow checker) and a **comprehensive language specification** (32 documents). However, the language cannot be used for real-world applications due to critical gaps in the standard library, runtime, tooling, and ecosystem.

Current state assessment:
- **Compiler**: 9/10 - Mature, all phases working (lexer -> parser -> types -> borrow -> HIR -> MIR -> LLVM IR)
- **Specification**: 9/10 - 32 docs, LL(1) grammar, complete type system
- **Core Library**: 7/10 - 136 modules exist but only 35.9% tested (1359/3790 functions)
- **Standard Library**: 5/10 - 70 modules, missing collections, path, env, datetime
- **Async Runtime**: 2/10 - Poll[T] exists but no executor/scheduler
- **Networking**: 1/10 - Stubs only in net/pending/
- **Ecosystem**: 1/10 - No IDE support, no doc generation, no package registry

This roadmap organizes ALL missing implementations into a dependency-ordered execution plan across 6 milestones, referencing consolidated rulebook tasks.

## What Changes

This is a **meta-task** that orchestrates the execution order of all pending work. It does NOT duplicate existing task proposals but rather:
1. Defines the dependency graph between tasks
2. Identifies NEW work not covered by existing tasks
3. Establishes milestone gates (what must be done before proceeding)
4. Tracks overall language completeness percentage

### Dependency Graph (Updated after task consolidation)

```
Milestone 0: Compiler Infrastructure (no dependencies, MAXIMUM priority)
├── compiler-infrastructure (embed LLVM, query system, red-green, cranelift, diagnostics, LLD)
└── go-style-test-system (75% done - EXE-based tests, subprocess execution)

Milestone 1: Foundation (no dependencies)
├── increase-library-coverage (35.9% -> 70%)
├── stdlib-essentials (collections, env, path, datetime)
├── NEW: error-context-chains (anyhow-style errors)
├── NEW: buffered-io (BufReader, BufWriter)
└── NEW: regex-engine

Milestone 2: Documentation & Reflection (depends on M1)
├── developer-tooling (doc generation + VSCode extension + LSP server)
├── implement-reflection (TypeInfo, @derive(Reflect))
├── NEW: logging-framework (structured logging)
└── NEW: serialization-framework (beyond JSON)

Milestone 3: Async & Networking (depends on M1)
└── async-network-stack (runtime + TCP/UDP + HTTP + TLS + Promise/Observable)

Milestone 4: Tooling & Ecosystem (depends on M2)
├── compiler-mcp-integration (AI integration)
└── package-manager (registry, publish)

Milestone 5: Advanced (depends on M3, M4)
├── cross-compilation (multi-target)
├── auto-parallel (automatic parallelization)
└── NEW: database-drivers (SQL, connection pooling)
```

### Active Tasks Referenced (11)

| Task | Status | Milestone |
|------|--------|-----------|
| `compiler-infrastructure` | 0% | M0 |
| `go-style-test-system` | 75% | M0 |
| `increase-library-coverage` | 0% | M1 |
| `stdlib-essentials` | 0% | M1 |
| `developer-tooling` | 0% | M2 |
| `implement-reflection` | 0% | M2 |
| `async-network-stack` | 0% | M3 |
| `compiler-mcp-integration` | 0% | M4 |
| `package-manager` | Partial | M4 |
| `cross-compilation` | 0% | M5 |
| `auto-parallel` | 0% | M5 |

### Consolidated Tasks (removed, content merged into active tasks)
- `achieve-rust-compiler-parity` -> merged into `compiler-infrastructure`
- `embed-llvm-incremental-compilation` -> merged into `compiler-infrastructure`
- `add-network-stdlib` -> merged into `async-network-stack`
- `async-http-runtime` -> merged into `async-network-stack`
- `multi-threaded-runtime` -> merged into `async-network-stack`
- `promises-reactivity` -> merged into `async-network-stack`
- `create-vscode-extension` -> merged into `developer-tooling`
- `implement-tml-doc` -> merged into `developer-tooling`
- `thread-safe-native` -> removed (99% complete, work absorbed)

### NEW Work Identified (not covered by existing tasks)
1. **error-context-chains** - anyhow/thiserror-style error wrapping with context
2. **buffered-io** - BufReader, BufWriter, LineWriter for performant I/O
3. **regex-engine** - Regular expression engine for text processing
4. **logging-framework** - Structured logging with levels, formatters, sinks
5. **serialization-framework** - YAML, TOML, MessagePack, Protobuf support
6. **database-drivers** - SQL drivers, connection pooling, basic ORM

## Impact

- Affected specs: All library specs, CLI spec, toolchain spec
- Affected code: lib/core, lib/std, compiler/src/cli, compiler/src/codegen, new extensions
- Breaking change: NO (all additive)
- User benefit: TML becomes a production-usable language capable of building real applications

## Completion Criteria

| Milestone | Gate Criteria | Target |
|-----------|--------------|--------|
| M0: Compiler Infra | Embedded LLVM, incremental < 500ms, Go-style tests done | Q2/2026 |
| M1: Foundation | Library coverage >= 70%, collections + env + path + datetime working | Q2/2026 |
| M2: Docs & Reflection | `tml doc` generates HTML, LSP works, @derive(Reflect) works | Q3/2026 |
| M3: Async & Network | TCP echo server runs, HTTP with decorators, TLS, 10K connections | Q3/2026 |
| M4: Tooling | MCP server running, package registry available | Q4/2026 |
| M5: Advanced | Cross-compile to Linux from Windows, auto-parallel loops, SQL queries | Q1/2027 |
