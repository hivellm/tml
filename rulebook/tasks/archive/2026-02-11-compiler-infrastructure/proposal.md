# Proposal: Compiler Infrastructure Overhaul (PRIORITY: MAXIMUM)

## Status: PROPOSED

**Consolidates**: `achieve-rust-compiler-parity` + `embed-llvm-incremental-compilation`

## Why

The TML compiler has ~182K LOC in C++ with a complete pipeline (lexer -> parser -> types -> borrow -> HIR -> MIR -> LLVM IR), but suffers from **fundamental architectural gaps** that make it slow and non-competitive:

1. **External LLVM via subprocess**: TML generates `.ll` text files on disk, then spawns `clang -c` as a subprocess for each one. This adds ~10-20ms process spawn + ~50ms clang startup + disk I/O per file. With 100 test files, ~3 seconds wasted on process management alone.

2. **No incremental compilation**: When a single function changes, the entire file is re-lexed, re-parsed, re-typechecked, re-codegen'd. Rust's query-based red-green algorithm recompiles only affected codegen units in ~200ms vs ~3-10s.

3. **Sequential pipeline**: The compiler runs phases sequentially without memoization. A demand-driven query system (like rustc's `TyCtxt`) enables caching, parallelism, and incremental builds.

4. **System linker overhead**: Using `link.exe`/`ld` as subprocess adds 200ms-2s per link. LLD as a library is 2-7x faster.

5. **Single codegen backend**: LLVM produces excellent optimized code but is slow for debug builds. A Cranelift backend would give 3-5x faster debug compilation.

6. **Basic diagnostics**: No error codes, no machine-applicable suggestions, no `--explain`, no auto-fix.

### Comparative Analysis

| Aspect | TML (current) | rustc | Gap |
|--------|---------------|-------|-----|
| LLVM usage | External (clang subprocess) | Embedded as library | Critical |
| Incremental granularity | File-level hash | Query-level red-green (256 CGUs) | Critical |
| Compilation architecture | Sequential pipeline | Demand-driven query system | Critical |
| Backends | LLVM only | LLVM + Cranelift + GCC | High |
| Linker | System (subprocess) | LLD embedded | High |
| Diagnostics | Basic span-based | Fluent i18n, auto-fix suggestions | High |
| Borrow checker | Basic NLL | NLL + Polonius (next-gen) | Medium |
| IR layers | AST -> HIR -> MIR | AST -> HIR -> THIR -> MIR | Medium |
| Trait solver | Simple resolver | Goal-based (Chalk-inspired) | Medium |
| Self-hosting | C++ (external compiler) | Self-hosting (3 stages) | High |
| Process spawns per compile | 1 per file (clang) | 0 | Critical |
| IR written to disk | Always (.ll files) | Never (in-memory) | High |
| Recompile after 1-line change | Entire file (~3-10s) | 1-2 CGUs (~1.7s) | Critical |

## What Changes

### Tier 1: Performance-Critical (Phases 1-5)

**Phase 1: Embed LLVM as Library**
- Link against LLVM libraries directly in the TML compiler binary
- Use LLVM C/C++ API to build IR in-memory (replace textual IR generation)
- Emit object files directly via `TargetMachine::emit()` without `.ll` intermediates
- `--emit-ir` flag continues to write `.ll` files when explicitly requested

**Phase 2: Eliminate Intermediate .ll Files**
- Refactor `LLVMIRGen::generate()` to return `llvm::Module*` instead of `std::string`
- Keep `generate_text()` for `--emit-ir` and MCP `emit-ir` tool
- Remove all `.ll` file I/O from default compilation path

**Phase 3: Query System Foundation**
- Restructure compiler as demand-driven queries with memoization
- `QueryContext` stores cached results and dependency graph
- Core queries: `parse_tokens`, `parse_module`, `typecheck_module`, `borrowcheck_module`, `codegen_unit`
- Each query fingerprinted (128-bit hash) for incremental invalidation

**Phase 4: Red-Green Incremental Compilation**
- Implement red-green coloring algorithm for query invalidation
- Persist dependency graph and fingerprints to disk between compilations
- Single-function change recompiles only affected queries (~200ms vs ~3s)
- No-op rebuild verifies fingerprints only (~50-100ms)

**Phase 5: Codegen Unit Partitioning**
- Split modules into N independent codegen units (64 incremental, 16 release)
- Each CGU produces an independent object file, cached by fingerprint
- On incremental rebuild, only re-emit changed CGUs via LLVM

### Tier 2: Backend & Toolchain (Phases 6-8)

**Phase 6: Embed LLD Linker**
- Link against LLD libraries (lldELF, lldCOFF, lldMachO)
- In-process linking, 2-7x faster than system linker
- Keep system linker as `--linker=system` fallback

**Phase 7: Backend Abstraction Layer**
- `CodegenBackend` interface with methods: `compile_function`, `emit_object`
- Refactor `llvm_ir_gen.cpp` to implement the interface
- CLI flag `--backend=llvm|cranelift`

**Phase 8: Cranelift Backend**
- Integrate cranelift-codegen via C API / FFI
- MIR -> Cranelift IR translation
- Target: debug builds 3x faster than LLVM -O0

### Tier 3: Quality & Safety (Phases 9-11)

**Phase 9: Advanced Diagnostics System**
- Structured `Diagnostic` with codes (T0001-T9999), spans, notes, suggestions
- `DiagnosticBuilder` pattern with `.with_note()`, `.with_suggestion()`, `.emit()`
- Machine-applicable auto-fix suggestions (`tml fix`)
- `tml explain T0308` for detailed explanations
- JSON output for LSP integration
- "Did you mean?" suggestions for typos

**Phase 10: Polonius Borrow Checker**
- Origin-based analysis (where each reference came from)
- Datalog-like constraint solver
- More programs accepted than NLL (fewer false positives)
- Flag `--polonius` to enable

**Phase 11: THIR Layer + Advanced Trait Solver**
- Typed HIR with explicit coercions and desugared method calls
- Goal-based trait solver with associated type normalization
- Higher-ranked behavior bounds support

### Tier 4: Self-Hosting (Phase 12)

**Phase 12: Self-Hosting Preparation**
- Stage 0 (C++) compiles Stage 1 (TML lexer + parser)
- Gradual migration module by module
- Cross-validate output between C++ and TML compilers

### Additional Optimizations

- **Test Result Caching (Go-style)**: Cache pass/fail results, skip execution on cache hit
- **Parallel LLVM Backend**: Use `llvm::ThreadPool` for CGU compilation within a module
- **Lazy Module Loading**: Only typecheck imported symbols actually used
- **Function-Level IR Cache**: Reuse IR for unchanged functions within a module
- **In-Memory Object Passing**: Skip disk I/O for non-cached objects passed to linker

## Architecture

### Current Pipeline
```
source.tml
    → Lexer → Tokens
    → Parser → AST
    → TypeChecker → TypeEnv
    → BorrowChecker → Verified
    → LLVMIRGen → String (textual LLVM IR)
    → write to .ll file (DISK I/O)
    → spawn clang -c (SUBPROCESS)
        → clang reads .ll → LLVM passes → write .obj
    → spawn linker (SUBPROCESS)
        → link → .exe/.dll
```

### New Pipeline
```
source.tml
    → Lexer → Tokens                    (query: parse_tokens)
    → Parser → AST                      (query: parse_module)
    → TypeChecker → TypeEnv             (query: typecheck_module)
    → BorrowChecker → Verified          (query: borrowcheck_module)
    → LLVMIRGen → llvm::Module*         (query: codegen_unit[N])
        → IN-PROCESS LLVM passes
        → IN-PROCESS emit .obj to memory/disk
    → LLD link (IN-PROCESS)
        → .exe/.dll

    All queries memoized with red-green invalidation.
    Codegen units cached independently.
    --emit-ir writes .ll on demand.
    --backend=cranelift for fast debug builds.
```

## Impact

- Affected specs: compiler architecture, CLI, build system, diagnostics, borrow checker
- Affected code: `compiler/src/` (all modules), `compiler/include/` (all headers), `CMakeLists.txt`
- Breaking change: NO (internal improvements, CLI/API unchanged for users)
- User benefit: 3-10x faster incremental builds, 20-30% faster clean builds, better diagnostics, more programs accepted by borrow checker, fast debug backend

## Risk Assessment

| Phase | Risk | Mitigation |
|-------|------|------------|
| Embed LLVM | **High** - Build system change, LLVM API version-sensitive | LLVM C API (stable ABI), pin version, keep clang fallback |
| Eliminate .ll | **Low** - Straightforward once LLVM embedded | `--emit-ir` preserved |
| Query System | **High** - Massive refactoring | Implement incrementally, module by module |
| Red-Green | **High** - Complex algorithm | Start with 2-3 queries, expand gradually |
| CGU Partitioning | **Medium** - Deterministic mapping needed | Hash-based assignment |
| LLD | **Medium** - Platform-specific behavior | System linker fallback |
| Backend Abstraction | **Medium** - Codegen refactoring | Keep LLVM as reference |
| Cranelift | **Low** - Well-documented FFI | Stable Cranelift C API |
| Diagnostics | **Low** - Additive improvements | No breaking changes |
| Polonius | **High** - Complex algorithm | Validate against NLL tests |
| THIR + Traits | **Medium** - New IR layer | Alongside existing HIR |
| Self-hosting | **Very High** - Requires mature language | Depends on language completeness |

## Completion Criteria

| Phase | Gate Criteria | Target |
|-------|--------------|--------|
| P1: Embed LLVM | No clang subprocess, in-process .obj emission | Q2/2026 |
| P2: No .ll files | Normal builds produce no .ll files, --emit-ir works | Q2/2026 |
| P3: Query System | 5 core queries working, all tests pass | Q2/2026 |
| P4: Red-Green | Single-function recompilation < 500ms | Q3/2026 |
| P5: CGU Partition | Changed function recompiles 1-2 CGUs only | Q3/2026 |
| P6: LLD | In-process linking, no system linker subprocess | Q3/2026 |
| P7: Backend Abstraction | Interface defined, LLVM backend migrated | Q3/2026 |
| P8: Cranelift | Debug build 3x faster than LLVM -O0 | Q4/2026 |
| P9: Diagnostics | Error codes, --explain, auto-fix for top 10 errors | Q2/2026 |
| P10: Polonius | 100% NLL tests pass + 10 new programs accepted | Q4/2026 |
| P11: THIR + Traits | THIR generated, trait solver resolves associated types | Q1/2027 |
| P12: Self-hosting | Lexer + parser in TML, compilable by Stage 0 C++ | Q2/2027 |
