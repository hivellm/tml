# Tasks: Achieve Rust Compiler Infrastructure Parity

**Status**: Proposed (0%)

---

## Phase 1: Query System Foundation (Q2/2026)

**Goal**: Migrate from sequential pipeline to demand-driven compilation

### 1.1 Core Query Infrastructure

- [ ] 1.1.1 Design `QueryContext` struct (equivalent to rustc's `TyCtxt`) with arena allocator
- [ ] 1.1.2 Implement `QueryKey` trait and `QueryValue` trait for generic query definitions
- [ ] 1.1.3 Implement `QueryCache[K,V]` — thread-safe hashtable with memoization
- [ ] 1.1.4 Implement `QueryProvider` registry — map query types to provider functions
- [ ] 1.1.5 Implement dependency tracking: `QueryContext::record_dependency(caller, callee)`
- [ ] 1.1.6 Implement cycle detection: detect circular dependencies between queries
- [ ] 1.1.7 Implement `QueryContext::force(query)` — execute query and cache result

### 1.2 Core Query Definitions

- [ ] 1.2.1 Define `parse_query(file_path) -> AST` — cache parse result per file
- [ ] 1.2.2 Define `type_check_query(func_id) -> TypedAST` — type check per function
- [ ] 1.2.3 Define `borrow_check_query(func_id) -> BorrowResult` — borrow check per function
- [ ] 1.2.4 Define `hir_query(func_id) -> HIR` — HIR lowering per function
- [ ] 1.2.5 Define `mir_query(func_id) -> MIR` — MIR lowering per function
- [ ] 1.2.6 Define `optimized_mir_query(func_id) -> MIR` — optimized MIR per function
- [ ] 1.2.7 Define `codegen_query(func_id) -> LLVM_IR` — codegen per function
- [ ] 1.2.8 Define `type_of_query(expr_id) -> Type` — type of an expression
- [ ] 1.2.9 Define `resolve_behavior_query(type, behavior) -> ImplResult` — resolve impl

### 1.3 Migration

- [ ] 1.3.1 Refactor `dispatcher.cpp` to use `QueryContext` as entry point
- [ ] 1.3.2 Migrate lexer/parser to query-based (per file)
- [ ] 1.3.3 Migrate type checker to query-based (per function)
- [ ] 1.3.4 Migrate borrow checker to query-based (per function)
- [ ] 1.3.5 Migrate HIR builder to query-based
- [ ] 1.3.6 Migrate MIR lowering to query-based
- [ ] 1.3.7 Migrate codegen to query-based
- [ ] 1.3.8 Regression tests: all existing tests must pass
- [ ] 1.3.9 Benchmarks: compare performance before/after migration

**Gate P1**: 9 core queries working, regression tests passing, no performance degradation > 10%

---

## Phase 2: Fine-Grained Incremental Compilation (Q3/2026)

**Goal**: Query-level incremental recompilation with Red-Green algorithm

### 2.1 Query Result Persistence

- [ ] 2.1.1 Implement binary serialization of query results (compact format)
- [ ] 2.1.2 Implement query result hashing (SHA-256 for fingerprinting)
- [ ] 2.1.3 Implement disk cache: save query results + hashes + dependency graph
- [ ] 2.1.4 Implement disk cache loader: load cache from previous build
- [ ] 2.1.5 Design on-disk cache format (header + index + data blobs)

### 2.2 Red-Green Algorithm

- [ ] 2.2.1 Implement `try_mark_green(query)` — verify if previous result is still valid
- [ ] 2.2.2 Implement recursive dependency color propagation (green/red)
- [ ] 2.2.3 Implement selective re-execution: only recompute "red" queries
- [ ] 2.2.4 Implement hash comparison: compare hash of new result with cached
- [ ] 2.2.5 Implement cache eviction: remove orphaned entries after N compilations

### 2.3 Source-Level Change Detection

- [ ] 2.3.1 Implement file-level diff: detect which files changed (mtime + hash)
- [ ] 2.3.2 Implement function-level diff: detect which functions changed within a file
- [ ] 2.3.3 Implement import-level diff: detect changes in transitive dependencies
- [ ] 2.3.4 Implement token-level stability: identical tokens = parse result cache hit

### 2.4 Integration

- [ ] 2.4.1 Integrate incremental with `tml build` (default on)
- [ ] 2.4.2 Integrate incremental with `tml test` (reuse cached results)
- [ ] 2.4.3 CLI flag `--fresh` to force full recompilation
- [ ] 2.4.4 CLI flag `--incremental-info` to show cache hit/miss statistics
- [ ] 2.4.5 Tests: modify 1 function in a 100-file project, recompile < 500ms

**Gate P2**: Single-function change recompilation < 500ms, cache hit rate > 80% on medium projects

---

## Phase 3: Backend Abstraction Layer (Q3/2026)

**Goal**: Generic interface for multiple codegen backends

### 3.1 Backend Interface Design

- [ ] 3.1.1 Design `CodegenBackend` behavior with methods: `compile_function`, `compile_module`, `emit_object`
- [ ] 3.1.2 Design `CodegenContext` behavior: `declare_function`, `define_type`, `get_global`
- [ ] 3.1.3 Design `CodegenBuilder` behavior: `build_add`, `build_call`, `build_branch`, `build_return`
- [ ] 3.1.4 Design `CodegenValue` and `CodegenType` opaque handles
- [ ] 3.1.5 Design `BackendConfig` struct: target triple, optimization level, debug info

### 3.2 LLVM Backend Refactoring

- [ ] 3.2.1 Extract generic interface from `llvm_ir_gen.hpp` (67K) into `codegen_backend.hpp`
- [ ] 3.2.2 Create `LLVMBackendImpl` implementing `CodegenBackend`
- [ ] 3.2.3 Create `LLVMCodegenContext` implementing `CodegenContext`
- [ ] 3.2.4 Create `LLVMCodegenBuilder` implementing `CodegenBuilder`
- [ ] 3.2.5 Refactor MIR -> LLVM IR to use generic interface (not LLVM-specific)
- [ ] 3.2.6 Move LLVM-specific code to `compiler/src/codegen/llvm/`
- [ ] 3.2.7 Move backend-agnostic code to `compiler/src/codegen/common/`
- [ ] 3.2.8 Tests: all existing tests pass with refactored backend

### 3.3 Backend Selection

- [ ] 3.3.1 CLI flag `--backend=llvm|cranelift` (default: llvm)
- [ ] 3.3.2 Backend factory: instantiate correct backend based on flag
- [ ] 3.3.3 Backend capability query: check if backend supports feature X
- [ ] 3.3.4 Fallback: if cranelift doesn't support a feature, use LLVM automatically

**Gate P3**: Interface defined, LLVM backend migrated, zero test regressions

---

## Phase 4: Cranelift Backend (Q4/2026)

**Goal**: Alternative backend for fast compilation during development

### 4.1 Cranelift Integration

- [ ] 4.1.1 Integrate cranelift-codegen via C API (cranelift-c-api or direct FFI)
- [ ] 4.1.2 Implement `CraneliftBackendImpl` — CodegenBackend for Cranelift
- [ ] 4.1.3 Implement `CraneliftCodegenContext` — function and type declarations
- [ ] 4.1.4 Implement `CraneliftCodegenBuilder` — IR builder for Cranelift IR

### 4.2 MIR -> Cranelift Translation

- [ ] 4.2.1 Map MIR types to Cranelift types (I8-I128, F32, F64, pointers)
- [ ] 4.2.2 Map MIR basic blocks to Cranelift blocks (EBBs)
- [ ] 4.2.3 Translate MIR instructions to Cranelift IR instructions
- [ ] 4.2.4 Translate MIR terminators (branch, switch, call, return)
- [ ] 4.2.5 Implement calling conventions (C ABI, TML ABI)
- [ ] 4.2.6 Implement struct layout and field access
- [ ] 4.2.7 Implement enum layout with discriminant
- [ ] 4.2.8 Implement closures and captures

### 4.3 Object File Generation

- [ ] 4.3.1 Cranelift object file emission via `cranelift-object`
- [ ] 4.3.2 Linkage with C runtime (essential.c)
- [ ] 4.3.3 Basic debug info (DWARF via gimli)
- [ ] 4.3.4 Tests: subset of existing tests pass with Cranelift
- [ ] 4.3.5 Benchmark: compare compile time LLVM -O0 vs Cranelift

**Gate P4**: 80% of tests pass with Cranelift, debug build 3x faster than LLVM -O0

---

## Phase 5: Advanced Diagnostics System (Q2/2026)

**Goal**: Professional-grade diagnostics with automatic suggestions

### 5.1 Structured Diagnostics

- [ ] 5.1.1 Design `Diagnostic` struct: level, code, message, spans[], notes[], suggestions[]
- [ ] 5.1.2 Design `DiagnosticBuilder` pattern: `.with_note()`, `.with_suggestion()`, `.emit()`
- [ ] 5.1.3 Implement `DiagnosticRenderer` for terminal (colors, context, arrows)
- [ ] 5.1.4 Implement `DiagnosticRenderer` for JSON (LSP-compatible format)
- [ ] 5.1.5 Implement multi-span support: primary span + secondary labeled spans
- [ ] 5.1.6 Implement code snippet rendering: show source lines with arrows and labels

### 5.2 Error Codes & Explanations

- [ ] 5.2.1 Catalogue all existing errors with codes (T0001-T9999)
- [ ] 5.2.2 Categorize: T01xx (lexer), T02xx (parser), T03xx (types), T04xx (borrow), T05xx (codegen)
- [ ] 5.2.3 Write long-form explanations for each code (examples + suggestions)
- [ ] 5.2.4 Implement `tml explain T0308` — show detailed explanation in terminal
- [ ] 5.2.5 Generate HTML index of all error codes

### 5.3 Machine-Applicable Suggestions

- [ ] 5.3.1 Design `Suggestion` struct: message, span, replacement_text, applicability
- [ ] 5.3.2 Implement applicability levels: `MachineApplicable`, `MaybeIncorrect`, `HasPlaceholders`
- [ ] 5.3.3 Implement `tml fix` — automatically apply `MachineApplicable` suggestions
- [ ] 5.3.4 Suggestion: "did you mean `X`?" for identifier typos (edit distance)
- [ ] 5.3.5 Suggestion: "add `mut`" when assigning to immutable variable
- [ ] 5.3.6 Suggestion: "add type annotation" when inference fails
- [ ] 5.3.7 Suggestion: "borrow instead of move" when borrow checker fails
- [ ] 5.3.8 Suggestion: "add `.duplicate()`" when moving a Copy value
- [ ] 5.3.9 Suggestion: "convert type with `as`" when numeric type mismatch

### 5.4 Diagnostic Quality

- [ ] 5.4.1 Implement error deduplication: don't show errors derived from the same root cause
- [ ] 5.4.2 Implement error cascading prevention: `ErrorType` that propagates without new errors
- [ ] 5.4.3 Implement warning levels: `--warn=all`, `--warn=extra`, `--warn=pedantic`
- [ ] 5.4.4 Implement `--deny=<warning>` to treat warnings as errors
- [ ] 5.4.5 Implement source-line caching to avoid re-reading files during diagnostics
- [ ] 5.4.6 Tests for diagnostic rendering (snapshot tests)

### 5.5 Internationalization Preparation

- [ ] 5.5.1 Extract all error messages to separate message files
- [ ] 5.5.2 Design message catalog format (similar to Fluent)
- [ ] 5.5.3 Implement message lookup by locale
- [ ] 5.5.4 Create English (en-US) catalog as reference

**Gate P5**: Error codes implemented, `tml explain` works, auto-fix suggestions for the 10 most common errors

---

## Phase 6: Polonius Borrow Checker (Q4/2026)

**Goal**: Next-gen borrow checker that accepts more valid programs

### 6.1 Origin Analysis

- [ ] 6.1.1 Design `Origin` type — represent "where a reference came from"
- [ ] 6.1.2 Design `OriginConstraint` — subset relations between origins
- [ ] 6.1.3 Implement origin assignment: each `ref T` gets a unique origin
- [ ] 6.1.4 Implement origin propagation: propagate origins through assignments and calls
- [ ] 6.1.5 Implement origin constraint generation: generate subtyping constraints

### 6.2 Constraint Solving

- [ ] 6.2.1 Implement Datalog-like constraint solver (fixed-point iteration)
- [ ] 6.2.2 Implement `loan_live_at(loan, point)` — determine if borrow is active
- [ ] 6.2.3 Implement `origin_live_at(origin, point)` — determine if origin is live
- [ ] 6.2.4 Implement `origin_contains_loan_at(origin, loan, point)` — membership check
- [ ] 6.2.5 Implement `errors(loan, point)` — detect violations
- [ ] 6.2.6 Implement location-insensitive pre-check (fast, conservative)
- [ ] 6.2.7 Implement location-sensitive full check (precise, slower)

### 6.3 Integration

- [ ] 6.3.1 Implement Polonius as alternative to NLL (flag `--polonius`)
- [ ] 6.3.2 Validate: all programs accepted by NLL also accepted by Polonius
- [ ] 6.3.3 Identify and test programs Polonius accepts but NLL rejects
- [ ] 6.3.4 Problem case 1: conditional returns with references
- [ ] 6.3.5 Problem case 2: borrowing and then moving into a container
- [ ] 6.3.6 Problem case 3: splitting borrows across branches
- [ ] 6.3.7 Benchmark: Polonius vs NLL overhead (target < 2x)
- [ ] 6.3.8 Full regression tests

**Gate P6**: 100% NLL tests pass with Polonius, at least 10 new programs accepted, overhead < 2x

---

## Phase 7: THIR Layer + Advanced Trait Solver (Q1/2027)

**Goal**: Fully-typed intermediate IR and sophisticated trait solver

### 7.1 THIR (Typed HIR)

- [ ] 7.1.1 Design `ThirExpr` — expression with explicit type and materialized coercions
- [ ] 7.1.2 Design `ThirStmt` — statement with all implicit conversions made explicit
- [ ] 7.1.3 Implement HIR -> THIR lowering: materialize all type coercions
- [ ] 7.1.4 Implement method call resolution: `x.method()` -> `Type::method(ref x)` explicit
- [ ] 7.1.5 Implement auto-deref resolution: `ref ref T` -> `T` chains made explicit
- [ ] 7.1.6 Implement operator desugaring: `a + b` -> `Add::add(a, b)` explicit
- [ ] 7.1.7 Implement pattern exhaustiveness checking in THIR
- [ ] 7.1.8 Implement THIR validation: verify invariants before MIR lowering
- [ ] 7.1.9 Migrate MIR lowering to use THIR as input (instead of HIR)

### 7.2 Advanced Trait Solver

- [ ] 7.2.1 Design goal-based trait solving: `Goal = Trait(Type)`, `Goal = Projection(Type, AssocType)`
- [ ] 7.2.2 Implement `Clause` type: `impl`, `where`, `builtin` candidates
- [ ] 7.2.3 Implement candidate selection: filter and rank candidates by specificity
- [ ] 7.2.4 Implement recursive trait resolution with cycle detection
- [ ] 7.2.5 Implement associated type normalization: `T::Output` -> concrete type
- [ ] 7.2.6 Implement higher-ranked behavior bounds: `for[T] Fn(T) -> T`
- [ ] 7.2.7 Implement negative reasoning: `not Impl[T]` for coherence checking
- [ ] 7.2.8 Implement specialization: more specific impl wins (RFC 1210-style)
- [ ] 7.2.9 Tests for trait resolution edge cases

**Gate P7**: THIR generated for all programs, trait solver resolves associated types and HRTB

---

## Phase 8: Self-Hosting Preparation (Q2/2027)

**Goal**: Begin rewriting the TML compiler in TML

### 8.1 TML Bootstrap Infrastructure

- [ ] 8.1.1 Design bootstrap plan: Stage 0 (C++) -> Stage 1 (partial TML) -> Stage 2 (full TML)
- [ ] 8.1.2 Create `compiler-tml/` directory for the TML compiler
- [ ] 8.1.3 Implement build system: Stage 0 C++ compiles Stage 1 TML
- [ ] 8.1.4 Implement test harness: compare output of C++ compiler vs TML compiler

### 8.2 Lexer in TML

- [ ] 8.2.1 Rewrite `lexer_core.cpp` in TML (`compiler-tml/src/lexer/core.tml`)
- [ ] 8.2.2 Rewrite `lexer_string.cpp` in TML
- [ ] 8.2.3 Rewrite `lexer_number.cpp` in TML
- [ ] 8.2.4 Rewrite `lexer_operator.cpp` in TML
- [ ] 8.2.5 Rewrite `lexer_ident.cpp` in TML
- [ ] 8.2.6 Rewrite `lexer_token.cpp` in TML
- [ ] 8.2.7 Tests: TML lexer produces identical tokens to C++ lexer

### 8.3 Parser in TML

- [ ] 8.3.1 Rewrite Pratt parser in TML (`compiler-tml/src/parser/expr.tml`)
- [ ] 8.3.2 Rewrite declaration parser in TML
- [ ] 8.3.3 Rewrite statement parser in TML
- [ ] 8.3.4 Rewrite pattern parser in TML
- [ ] 8.3.5 Rewrite type parser in TML
- [ ] 8.3.6 Tests: TML parser produces identical AST to C++ parser

### 8.4 Validation

- [ ] 8.4.1 Cross-validate: compile test suite with Stage 0 and Stage 1, compare outputs
- [ ] 8.4.2 Benchmark: TML lexer/parser performance vs C++ (target: < 2x overhead)
- [ ] 8.4.3 Document plan for migrating type checker, borrow checker, codegen

**Gate P8**: Lexer + parser in TML compilable by Stage 0 C++, identical output to C++

---

## Tracking: Overall Progress

| Phase | Items | Done | Progress |
|-------|-------|------|----------|
| P1: Query System | 25 | 0 | 0% |
| P2: Incremental | 19 | 0 | 0% |
| P3: Backend Abstraction | 16 | 0 | 0% |
| P4: Cranelift | 18 | 0 | 0% |
| P5: Diagnostics | 26 | 0 | 0% |
| P6: Polonius | 16 | 0 | 0% |
| P7: THIR + Traits | 18 | 0 | 0% |
| P8: Self-Hosting | 17 | 0 | 0% |
| **TOTAL** | **155** | **0** | **0%** |
