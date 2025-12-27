# Proposal: Auto-Parallelization

## Why

Modern CPUs have multiple cores that remain underutilized when running sequential code. Manual parallelization is error-prone and requires expertise in concurrent programming. TML should automatically detect and parallelize suitable code patterns, enabling developers to write simple sequential code that executes efficiently on multi-core systems. Inspired by Bend's automatic parallelization but adapted for TML's imperative style with safety guarantees.

## What Changes

### New Components

1. **Purity Analyzer** (`src/analysis/purity.hpp`, `src/analysis/purity.cpp`)
   - Effect tracking (IO, mutation, channels)
   - Pure function detection
   - Side-effect analysis

2. **Dependency Analyzer** (`src/analysis/dependencies.hpp`, `src/analysis/dependencies.cpp`)
   - Loop-carried dependency detection (RAW, WAR, WAW)
   - Alias analysis for arrays/pointers
   - Data flow analysis

3. **Parallelizer** (`src/transform/parallelizer.hpp`, `src/transform/parallelizer.cpp`)
   - Pattern detection (parallel loops, map/reduce)
   - Work partitioning strategies
   - Parallel IR generation

4. **Parallel Runtime** (`runtime/tml_parallel.c`, `runtime/tml_parallel.h`)
   - Thread pool with work stealing
   - Parallel for implementation
   - Synchronization primitives
   - CPU core detection and binding

5. **Parallel Annotations** (lexer/parser extensions)
   - `@parallel` - Force parallelization
   - `@sequential` - Prevent parallelization
   - `@pure` - Assert function purity

### Integration Points

- Type checker: Add purity tracking to function signatures
- LLVM backend: Generate parallel loop constructs
- Runtime: Thread pool and work distribution
- CLI: `--parallel` flag, `TML_THREADS` env var

## Impact

- **Affected specs**: 05-SEMANTICS.md (effects), 16-COMPILER-ARCHITECTURE.md (passes)
- **Affected code**: New `src/analysis/`, `src/transform/`, `runtime/tml_parallel.c`
- **Breaking change**: NO (opt-in feature, auto-detection is conservative)
- **User benefit**: Automatic multi-core utilization, up to Nx speedup on N cores
- **Dependencies**: Requires LLVM backend, threading runtime (already implemented)

## Success Criteria

1. Loops without dependencies are automatically parallelized
2. Parallel execution produces same results as sequential (determinism)
3. No data races in generated code
4. Speedup of 3-4x on 4-core CPU for suitable workloads
5. Fallback to sequential for small workloads (avoid overhead)
6. `@parallel` and `@sequential` annotations work correctly
7. Thread count respects CPU cores and environment variable
8. Test coverage ≥90%
