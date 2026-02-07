# Proposal: Achieve Rust Compiler Infrastructure Parity

## Status: PROPOSED

## Why

The TML compiler has ~182K LOC in C++, with 50 MIR optimization passes, an NLL borrow checker, 4 cache layers, and a complete pipeline (lexer -> parser -> types -> borrow -> HIR -> MIR -> LLVM IR). However, when compared to rustc (~600K LOC, 77 crates), there are **fundamental architectural gaps** that prevent TML from competing at the infrastructure level with production languages like Rust.

The key gaps identified in the comparative analysis are:

| Aspect | TML (current) | rustc | Gap |
|--------|---------------|-------|-----|
| **Compilation architecture** | Sequential pipeline | Demand-driven query system | Critical |
| **Incremental granularity** | File-level hash | Query-level red-green | Critical |
| **Backends** | LLVM only | LLVM + Cranelift + GCC | High |
| **Self-hosting** | C++ (external compiler) | Self-hosting (3 stages) | High |
| **Diagnostics** | Basic span-based | Fluent i18n, machine-applicable suggestions | High |
| **Borrow checker** | Basic NLL | NLL + Polonius (next-gen) | Medium |
| **IR layers** | AST -> HIR -> MIR | AST -> HIR -> THIR -> MIR | Medium |
| **Trait solver** | Simple resolver | Old solver + Next-gen (Chalk-inspired) | Medium |
| **Monomorphization** | Inline during codegen | Codegen units with parallel partitioning | Medium |
| **Incremental parsing** | Does not exist | Partial (HIR+ querified) | High |

Without these improvements, TML will remain a functional but non-competitive compiler for large-scale projects. This roadmap defines the path to close these gaps across 8 phases.

## What Changes

### Phase 1: Query System Foundation
Migrate the compiler architecture from a sequential pipeline to a demand-driven query system with memoization. This is the foundation for all other improvements (incremental compilation, parallelism, LSP).

**Components:**
- `QueryContext` (equivalent to rustc's `TyCtxt`) — central hub for all queries
- Query providers: registered functions that compute results on demand
- Memoization cache: query results stored in a hashtable
- Dependency tracking: dependency graph between queries

### Phase 2: Fine-Grained Incremental Compilation
Implement rustc's Red-Green algorithm for incremental compilation at the query level, not just at the file level.

**Components:**
- Serialization of query results to disk
- Hash-based invalidation at the query level
- Red-Green algorithm: determine if queries changed without re-executing
- Disk cache with efficient binary format

### Phase 3: Backend Abstraction Layer
Create an abstraction layer that allows multiple codegen backends (similar to `rustc_codegen_ssa`).

**Components:**
- `CodegenBackend` interface (behavior) with methods for generating instructions
- Abstract `CodegenContext` and `CodegenBuilder`
- Refactor `llvm_ir_gen.cpp` to implement the interface
- Prepare for Cranelift backend (fast debug compilation)

### Phase 4: Cranelift Backend
Add Cranelift as an alternative backend for fast compilation during development.

**Components:**
- Integrate cranelift-codegen via FFI (Cranelift C API)
- Implement `CraneliftBackend` implementing `CodegenBackend`
- MIR -> Cranelift IR translation
- Benchmark: 3-5x faster than LLVM in debug mode

### Phase 5: Advanced Diagnostics System
Restructure the diagnostics system for parity with rustc.

**Components:**
- Structured diagnostics with `DiagnosticBuilder` pattern
- Machine-applicable suggestions (auto-fix)
- Catalogued error codes (T0001-T9999) with `--explain`
- "Did you mean?" suggestions for identifier typos
- Diagnostic rendering with multiline context
- JSON output for IDE integration
- Fluent-based i18n (preparation for translation)

### Phase 6: Polonius Borrow Checker
Implement the Polonius algorithm as a next-gen borrow checker, more precise than NLL.

**Components:**
- Origin-based analysis (tracking where each reference came from)
- Datalog-like constraint system
- Location-insensitive pre-check (fast) + location-sensitive full check
- Subset constraints between origins
- More programs accepted (fewer false positives than NLL)

### Phase 7: THIR Layer + Advanced Trait Solver
Add Typed HIR as an intermediate layer and a more sophisticated trait solver.

**Components:**
- THIR: fully-typed HIR with explicit coercions
- Desugaring: explicit method calls, materialized implicit coercions
- Chalk-inspired trait solver with goals/clauses
- Higher-ranked trait bounds support
- Associated type projection normalization

### Phase 8: Self-Hosting Preparation
Begin the path to self-hosting: the TML compiler written in TML.

**Components:**
- TML-in-TML bootstrap plan
- Rewrite compiler modules in TML (starting with the lexer)
- Stage 0: current C++ compiler compiles the new TML lexer
- Stage 1: TML lexer + rest in C++ compiles the TML parser
- Gradual migration module by module
- Goal: 50% of compiler in TML by end of 2027

## Impact

- Affected specs: compiler architecture, CLI, build system, diagnostics, borrow checker
- Affected code: compiler/src/ (all modules), compiler/include/ (all headers)
- Breaking change: NO (internal improvements, compiler API does not change for the user)
- User benefit: 3-5x faster debug compilation, better diagnostics, more programs accepted by borrow checker, more mature ecosystem

## Risk Assessment

| Phase | Risk | Mitigation |
|-------|------|------------|
| Query System | High - massive refactoring | Implement incrementally, module by module |
| Incremental | Medium - serialization complexity | Start with simple queries, expand gradually |
| Backend Abstraction | Medium - codegen refactoring | Keep LLVM as reference, abstract gradually |
| Cranelift | Low - well-documented FFI | Use stable Cranelift C API |
| Diagnostics | Low - additive improvements | Does not break anything existing |
| Polonius | High - complex algorithm | Implement subset, validate against NLL |
| THIR + Trait Solver | Medium - new IR layer | Implement alongside existing HIR |
| Self-hosting | Very High - requires mature language | Depends on M1-M5 of language-completeness-roadmap |

## Completion Criteria

| Phase | Gate Criteria | Target |
|-------|--------------|--------|
| P1: Query System | 5 core queries working (parse, typecheck, borrowck, hir, mir) | Q2/2026 |
| P2: Incremental | Single-file recompilation < 500ms (vs ~3s current) | Q3/2026 |
| P3: Backend Abstraction | Interface defined, LLVM backend migrated | Q3/2026 |
| P4: Cranelift | Debug build 3x faster than LLVM -O0 | Q4/2026 |
| P5: Diagnostics | Error codes, suggestions, JSON output, --explain | Q2/2026 |
| P6: Polonius | 100% NLL tests pass + 10 new programs accepted | Q4/2026 |
| P7: THIR + Traits | THIR generated, trait solver resolves associated types | Q1/2027 |
| P8: Self-hosting | Lexer + parser in TML, compilable by Stage 0 C++ | Q2/2027 |
