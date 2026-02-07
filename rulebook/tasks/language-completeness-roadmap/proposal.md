# Proposal: TML Language Completeness Roadmap

## Status: PROPOSED

## Why

TML has a **production-quality compiler** (~100K lines C++, 48 MIR optimization passes, full borrow checker) and a **comprehensive language specification** (32 documents). However, the language cannot be used for real-world applications due to critical gaps in the standard library, runtime, tooling, and ecosystem.

Current state assessment:
- **Compiler**: 9/10 - Mature, all phases working (lexer → parser → types → borrow → HIR → MIR → LLVM IR)
- **Specification**: 9/10 - 32 docs, LL(1) grammar, complete type system
- **Core Library**: 7/10 - 136 modules exist but only 35.9% tested (1359/3790 functions)
- **Standard Library**: 5/10 - 70 modules, missing collections, path, env, datetime
- **Async Runtime**: 2/10 - Poll[T] exists but no executor/scheduler
- **Networking**: 1/10 - Stubs only in net/pending/
- **Ecosystem**: 1/10 - No IDE support, no doc generation, no package registry

This roadmap organizes ALL missing implementations into a dependency-ordered execution plan across 6 milestones, referencing existing rulebook tasks where applicable.

## What Changes

This is a **meta-task** that orchestrates the execution order of all pending work. It does NOT duplicate existing task proposals but rather:
1. Defines the dependency graph between tasks
2. Identifies NEW work not covered by existing tasks
3. Establishes milestone gates (what must be done before proceeding)
4. Tracks overall language completeness percentage

### Dependency Graph

```
Milestone 1: Foundation (no dependencies)
├── increase-library-coverage (35.9% → 70%)
├── stdlib-essentials (collections, env, path, datetime)
├── NEW: error-context-chains (anyhow-style errors)
├── NEW: buffered-io (BufReader, BufWriter)
└── NEW: regex-engine

Milestone 2: Documentation & Reflection (depends on M1)
├── implement-tml-doc (doc generation)
├── implement-reflection (TypeInfo, @derive(Reflect))
├── NEW: logging-framework (structured logging)
└── NEW: serialization-framework (beyond JSON)

Milestone 3: Async & Networking (depends on M1)
├── multi-threaded-runtime (work-stealing executor)
├── add-network-stdlib (TCP, UDP, sockets)
└── thread-safe-native (99% done, close out)

Milestone 4: Web & HTTP (depends on M3)
├── async-http-runtime (HTTP/1.1, HTTP/2)
├── promises-reactivity (Observable, Promise)
└── NEW: tls-integration (SSL/TLS for networking)

Milestone 5: Tooling & IDE (depends on M2)
├── create-vscode-extension (syntax, snippets)
├── developer-tooling (LSP server)
├── compiler-mcp-integration (AI integration)
└── package-manager (registry, publish)

Milestone 6: Advanced (depends on M3, M5)
├── cross-compilation (multi-target)
├── auto-parallel (automatic parallelization)
└── NEW: database-drivers (SQL, connection pooling)
```

### Existing Tasks Referenced (15)
- increase-library-coverage
- stdlib-essentials
- implement-tml-doc
- implement-reflection
- multi-threaded-runtime
- add-network-stdlib
- thread-safe-native
- async-http-runtime
- promises-reactivity
- create-vscode-extension
- developer-tooling
- compiler-mcp-integration
- package-manager
- cross-compilation
- auto-parallel

### NEW Work Identified (6 items not covered by existing tasks)
1. **error-context-chains** - anyhow/thiserror-style error wrapping with context
2. **buffered-io** - BufReader, BufWriter, LineWriter for performant I/O
3. **regex-engine** - Regular expression engine for text processing
4. **logging-framework** - Structured logging with levels, formatters, sinks
5. **serialization-framework** - YAML, TOML, MessagePack, Protobuf support
6. **tls-integration** - SSL/TLS layer for secure networking
7. **database-drivers** - SQL drivers, connection pooling, basic ORM

## Impact

- Affected specs: All library specs, CLI spec, toolchain spec
- Affected code: lib/core, lib/std, compiler/src/cli, compiler/src/codegen, new extensions
- Breaking change: NO (all additive)
- User benefit: TML becomes a production-usable language capable of building real applications

## Completion Criteria

| Milestone | Gate Criteria | Target |
|-----------|--------------|--------|
| M1: Foundation | Library coverage ≥70%, collections + env + path + datetime working | Q1/2026 |
| M2: Docs & Reflection | `tml doc` generates HTML, @derive(Reflect) works, logging available | Q2/2026 |
| M3: Async & Network | TCP echo server runs, async/await compiles, 10K concurrent connections | Q2/2026 |
| M4: Web & HTTP | HTTP server handles routes with decorators, TLS works | Q3/2026 |
| M5: Tooling | VSCode extension published, LSP autocomplete works, MCP server running | Q3/2026 |
| M6: Advanced | Cross-compile to Linux from Windows, auto-parallel loops, SQL queries | Q4/2026 |
