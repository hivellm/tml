# Tasks: Compiler Infrastructure Overhaul

**Status**: In Progress (50%)
**Priority**: MAXIMUM - foundational infrastructure
**Consolidates**: `achieve-rust-compiler-parity` + `embed-llvm-incremental-compilation`

## Phase 1: Embed LLVM as Library (Eliminate clang subprocess) — DONE

- [x] 1.1 Add LLVM libraries as CMake dependencies (~55 static libs: LLVMCore, LLVMTarget, LLVMX86CodeGen, LLVMAArch64CodeGen, LLVMPasses, etc.)
- [x] 1.2 Configure CMake to link TML compiler binary against LLVM static libraries (with CRT mismatch fix for MSVC)
- [x] 1.3 Enable `compiler/src/backend/llvm_backend.cpp` — LLVM C API wrapper (already existed, was disabled)
- [x] 1.4 Implement in-memory IR string → object compilation via `LLVMParseIRInContext` + `LLVMRunPasses` + `LLVMTargetMachineEmitToFile`
- [x] 1.5 Add `compile_ir_string_to_object()` to `object_compiler.hpp/cpp` (LLVM backend with clang fallback)
- [x] 1.6 Update `build.bat` with `--no-llvm` flag and `-DTML_USE_LLVM_BACKEND=ON` default
- [x] 1.7 Remove clang subprocess from default compilation path (clang kept as fallback behind flag)
- [x] 1.8 Verify all 3,632 tests pass with embedded LLVM pipeline
- [x] 1.9 Benchmark: test suite dropped from ~15min to ~17s (50x+ improvement)

## Phase 2: Eliminate Intermediate .ll Files — DONE

- [x] 2.1 Add `compile_ir_string_to_object()` function (IR string → .obj, no .ll on disk)
- [x] 2.2 Update `build.cpp` — direct IR→obj (1 location)
- [x] 2.3 Update `run.cpp` — direct IR→obj (2 locations)
- [x] 2.4 Update `run_profiled.cpp` — direct IR→obj (1 location)
- [x] 2.5 Update `test_runner.cpp` — direct IR→obj (6 locations)
- [x] 2.6 Update `exe_test_runner.cpp` — direct IR→obj (5 locations)
- [x] 2.7 Update `parallel_build.cpp` — direct IR→obj (1 location)
- [x] 2.8 Verify `--emit-ir` CLI flag still produces `.ll` files correctly
- [x] 2.9 Verify MCP `mcp__tml__emit-ir` tool still works
- [x] 2.10 Verify all 3,632 tests pass without intermediate `.ll` files

## Phase 3: Query System Foundation — DONE

- [x] 3.1 Implement `QueryContext` class (equivalent to rustc's `TyCtxt`) with `force<R>()` template, cache, deps, and provider registry
- [x] 3.2 Implement `QueryKey` variant (8 key types) and typed result structs with `shared_ptr` wrapping
- [x] 3.3 Implement `QueryCache` — thread-safe hashtable with `shared_mutex` memoization and `invalidate_dependents()`
- [x] 3.4 Implement `QueryProviderRegistry` — `std::array<ProviderFn, COUNT>` with O(1) lookup by `QueryKind`
- [x] 3.5 Implement dependency tracking: stack-based `DependencyTracker` with `push_active`/`pop_active`/`record_dependency`
- [x] 3.6 Implement cycle detection: detect circular dependencies via active stack inspection
- [x] 3.7 Implement `QueryContext::force<R>(key)` — check cache → detect cycle → execute provider → cache result
- [x] 3.8 Define 8 core queries: `read_source`, `tokenize`, `parse_module`, `typecheck_module`, `borrowcheck_module`, `hir_lower`, `mir_build`, `codegen_unit`
- [x] 3.9 Implement 128-bit query fingerprinting using existing `tml::crc32c()` (two 64-bit halves)
- [x] 3.10 Wrap Preprocessor+FileRead in `provide_read_source` query provider
- [x] 3.11 Wrap Lexer in `provide_tokenize` query provider
- [x] 3.12 Wrap Parser in `provide_parse_module` query provider
- [x] 3.13 Wrap TypeChecker in `provide_typecheck_module` query provider
- [x] 3.14 Wrap BorrowChecker in `provide_borrowcheck_module` query provider
- [x] 3.15 Wrap HirBuilder+MirBuilder+Codegen in `provide_hir_lower`, `provide_mir_build`, `provide_codegen_unit` providers
- [x] 3.16 Add `run_build_with_queries()` to `build.cpp` — full query-based build path alongside existing pipeline
- [x] 3.17 Add `tml_query` static library to CMakeLists.txt, linked into `tml_cli`
- [x] 3.18 Verify all 3,632 tests pass with query system compiled and linked

## Phase 4: Red-Green Incremental Compilation — DONE

- [x] 4.1 Implement dependency graph serialization to disk (binary format)
- [x] 4.2 Implement query result fingerprint serialization
- [x] 4.3 Implement `try_mark_green(query)` — verify if previous result is still valid
- [x] 4.4 Implement recursive dependency color propagation (green/red)
- [x] 4.5 Implement selective re-execution: only recompute "red" queries
- [x] 4.6 Implement hash comparison: compare hash of new result with cached
- [x] 4.7 Implement incremental cache directory (`build/{profile}/.incr-cache/`)
- [x] 4.8 Implement cache loading on compiler startup and saving on shutdown
- [x] 4.9 Implement file-level diff: detect which files changed (mtime + hash)
- [x] 4.10 Implement function-level diff: detect which functions changed within a file
- [x] 4.11 Add `--incremental` flag (enabled by default for debug builds, controlled via `--no-cache`)
- [x] 4.12 Add `--no-incremental` / `--fresh` flag to force full recompilation (via `--no-cache`)
- [x] 4.13 Implement cache eviction: remove orphaned entries after N compilations
- [x] 4.14 Verify no-op rebuild completes in < 100ms
- [x] 4.15 Verify single-function change recompiles only affected queries
- [x] 4.16 Verify all tests pass with incremental compilation enabled

## Phase 5: Codegen Unit Partitioning — DONE

- [x] 5.1 Implement function-to-CGU mapping (hash-based deterministic assignment via `std::hash<string>{}(func.name) % num_cgus`)
- [x] 5.2 Split module codegen into N independent codegen units (default 16 debug, 4 release; capped at min(max_cgus, num_functions))
- [x] 5.3 Each CGU produces an independent object file (via `MirCodegen::generate_cgu()` + `compile_ir_string_to_object()`)
- [x] 5.4 Cache CGU object files independently (content-addressed: `.cache/<module>.cgu<N>.<fingerprint_12>.obj`)
- [x] 5.5 On incremental rebuild, only re-emit changed CGUs via LLVM (fingerprint mismatch triggers recompile)
- [x] 5.6 Reuse cached object files for unchanged CGUs (verified: second build → all CGUs cache hit)
- [x] 5.7 Link all CGU objects together (unchanged + recompiled) with `internal` linkage on preamble functions
- [x] 5.8 Verify single-function change recompiles only affected CGUs (hash-based partitioning isolates changes)
- [x] 5.9 Integrated in both `run_build_impl()` (legacy) and `run_build_with_queries()` (query pipeline)

## Phase 6: Embed LLD Linker — DONE

- [x] 6.1 Add LLD libraries as CMake dependencies (lldCOFF, lldCommon, lldELF, lldMachO, lldMinGW, lldWasm + transitive LLVM deps)
- [x] 6.2 Implement LLD in-process API in `compiler/src/backend/lld_linker.cpp` using `lld::lldMain()`
- [x] 6.3 Implement ELF linking via LLD for Linux targets (lld::elf::link driver)
- [x] 6.4 Implement COFF linking via LLD for Windows targets (lld::coff::link driver)
- [x] 6.5 Implement Mach-O linking via LLD for macOS targets (lld::macho::link driver)
- [x] 6.6 Update `link()` to use in-process LLD by default (subprocess fallback when TML_HAS_LLD_EMBEDDED not defined)
- [x] 6.7 System linker fallback via subprocess path (no LLD executable needed)
- [x] 6.8 Verify all 3,632 tests pass with in-process LLD linking
- [x] 6.9 Refactored command builders from string to argv vector for dual in-process/subprocess use

## Phase 7: Backend Abstraction Layer

- [ ] 7.1 Design `CodegenBackend` behavior with methods: `compile_function`, `compile_module`, `emit_object`
- [ ] 7.2 Design `CodegenContext` behavior: `declare_function`, `define_type`, `get_global`
- [ ] 7.3 Design `CodegenBuilder` behavior: `build_add`, `build_call`, `build_branch`, `build_return`
- [ ] 7.4 Design `CodegenValue` and `CodegenType` opaque handles
- [ ] 7.5 Extract generic interface from `llvm_ir_gen.hpp` into `codegen_backend.hpp`
- [ ] 7.6 Create `LLVMBackendImpl` implementing `CodegenBackend`
- [ ] 7.7 Refactor MIR -> LLVM IR to use generic interface
- [ ] 7.8 Move LLVM-specific code to `compiler/src/codegen/llvm/`
- [ ] 7.9 CLI flag `--backend=llvm|cranelift` (default: llvm)
- [ ] 7.10 Backend factory and capability query
- [ ] 7.11 Verify all tests pass with refactored backend

## Phase 8: Cranelift Backend

- [ ] 8.1 Integrate cranelift-codegen via C API / FFI
- [ ] 8.2 Implement `CraneliftBackendImpl` — CodegenBackend for Cranelift
- [ ] 8.3 Map MIR types to Cranelift types (I8-I128, F32, F64, pointers)
- [ ] 8.4 Map MIR basic blocks to Cranelift blocks
- [ ] 8.5 Translate MIR instructions to Cranelift IR instructions
- [ ] 8.6 Translate MIR terminators (branch, switch, call, return)
- [ ] 8.7 Implement calling conventions (C ABI, TML ABI)
- [ ] 8.8 Implement struct/enum layout and field access
- [ ] 8.9 Cranelift object file emission via `cranelift-object`
- [ ] 8.10 Linkage with C runtime (essential.c)
- [ ] 8.11 Verify 80%+ of tests pass with Cranelift
- [ ] 8.12 Benchmark: compile time LLVM -O0 vs Cranelift (target: 3x faster)

## Phase 9: Advanced Diagnostics System

- [ ] 9.1 Design `Diagnostic` struct: level, code, message, spans[], notes[], suggestions[]
- [ ] 9.2 Design `DiagnosticBuilder` pattern: `.with_note()`, `.with_suggestion()`, `.emit()`
- [ ] 9.3 Implement `DiagnosticRenderer` for terminal (colors, context, arrows)
- [ ] 9.4 Implement `DiagnosticRenderer` for JSON (LSP-compatible format)
- [ ] 9.5 Implement multi-span and code snippet rendering
- [ ] 9.6 Catalogue all errors with codes (T0001-T9999): T01xx lexer, T02xx parser, T03xx types, T04xx borrow, T05xx codegen
- [ ] 9.7 Write long-form explanations for each code
- [ ] 9.8 Implement `tml explain T0308` — show detailed explanation in terminal
- [ ] 9.9 Design `Suggestion` struct: message, span, replacement_text, applicability
- [ ] 9.10 Implement `tml fix` — automatically apply MachineApplicable suggestions
- [ ] 9.11 Suggestions: "did you mean?", "add mut", "add type annotation", "borrow instead of move"
- [ ] 9.12 Implement error deduplication and cascading prevention
- [ ] 9.13 Implement warning levels: `--warn=all|extra|pedantic`, `--deny=<warning>`
- [ ] 9.14 Extract error messages to separate message catalog (i18n preparation)

## Phase 10: Polonius Borrow Checker

- [ ] 10.1 Design `Origin` type — represent "where a reference came from"
- [ ] 10.2 Design `OriginConstraint` — subset relations between origins
- [ ] 10.3 Implement origin assignment and propagation through assignments and calls
- [ ] 10.4 Implement Datalog-like constraint solver (fixed-point iteration)
- [ ] 10.5 Implement `loan_live_at`, `origin_live_at`, `origin_contains_loan_at`, `errors`
- [ ] 10.6 Implement location-insensitive pre-check (fast) + location-sensitive full check (precise)
- [ ] 10.7 Implement Polonius as alternative to NLL (flag `--polonius`)
- [ ] 10.8 Validate: all NLL tests pass + identify programs Polonius accepts but NLL rejects
- [ ] 10.9 Benchmark: Polonius vs NLL overhead (target < 2x)

## Phase 11: THIR Layer + Advanced Trait Solver

- [ ] 11.1 Design `ThirExpr` — expression with explicit type and materialized coercions
- [ ] 11.2 Implement HIR -> THIR lowering: materialize all type coercions
- [ ] 11.3 Implement method call resolution, auto-deref, operator desugaring in THIR
- [ ] 11.4 Implement pattern exhaustiveness checking in THIR
- [ ] 11.5 Migrate MIR lowering to use THIR as input (instead of HIR)
- [ ] 11.6 Design goal-based trait solver: `Goal = Trait(Type)`, `Goal = Projection(Type, AssocType)`
- [ ] 11.7 Implement candidate selection, recursive resolution, cycle detection
- [ ] 11.8 Implement associated type normalization: `T::Output` -> concrete type
- [ ] 11.9 Implement higher-ranked behavior bounds: `for[T] Fn(T) -> T`

## Phase 12: Self-Hosting Preparation

- [ ] 12.1 Design bootstrap plan: Stage 0 (C++) -> Stage 1 (partial TML) -> Stage 2 (full TML)
- [ ] 12.2 Create `compiler-tml/` directory
- [ ] 12.3 Rewrite lexer in TML (core, string, number, operator, ident, token)
- [ ] 12.4 Tests: TML lexer produces identical tokens to C++ lexer
- [ ] 12.5 Rewrite parser in TML (Pratt parser, declarations, statements, patterns, types)
- [ ] 12.6 Tests: TML parser produces identical AST to C++ parser
- [ ] 12.7 Cross-validate: compile test suite with Stage 0 and Stage 1, compare outputs
- [ ] 12.8 Benchmark: TML lexer/parser performance vs C++ (target: < 2x overhead)

## Phase 13: Additional Optimizations

- [ ] 13.1 Implement test result caching (Go-style): skip execution when binary hash unchanged, report `(cached)`
- [ ] 13.2 Implement parallel LLVM backend (use `llvm::ThreadPool` for CGU compilation)
- [ ] 13.3 Implement lazy module loading (only typecheck imported symbols actually used)
- [ ] 13.4 Implement function-level IR cache (reuse IR for unchanged functions within a module)
- [ ] 13.5 Implement in-memory object passing to linker (skip disk I/O for non-cached objects)
- [ ] 13.6 Profile and optimize QueryContext lookup and dep graph serialization

## Phase 14: Cleanup & Validation

- [ ] 14.1 Remove legacy clang subprocess code (keep behind `--legacy-backend` flag)
- [x] 14.2 Remove mandatory `.ll` file generation from default paths
- [x] 14.3 Update CLAUDE.md build documentation
- [x] 14.4 Update README.md, compiler/README.md, CHANGELOG.md with infrastructure changes
- [x] 14.5 Run full test suite: verify zero regressions
- [ ] 14.6 Run full test suite with `--coverage`: verify coverage still works
- [ ] 14.7 Benchmark clean build: target 20-30% faster than baseline
- [ ] 14.8 Benchmark incremental build (1 function change): target < 500ms
- [ ] 14.9 Benchmark no-op rebuild: target < 100ms
- [ ] 14.10 Benchmark cached test suite: target < 1 second
