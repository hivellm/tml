# 09 — Performance Targets and Benchmarks

**Date**: 2026-04-05
**Status**: Complete
**Companion**: [06-hybrid-strategy.md](06-hybrid-strategy.md),
[docs/analyses/linker/06-performance-targets.md](../linker/06-performance-targets.md)

---

## Overview

This document establishes concrete, measurable performance targets for each backend option
in TML's backend elimination strategy. All targets are expressed in terms the benchmark
suite can verify, with acceptance criteria that gate each phase of development.

The linker analysis (`docs/analyses/linker/06-performance-targets.md`) covers link-time
performance in detail. This document covers the **compilation** phase: from TML source to
native object file, excluding the linker.

---

## 1. Current Baseline: LLVM Backend

### Test Environment

All estimates and measurements use a representative developer machine:
- CPU: 8-core x86_64, 3.5 GHz (Ryzen 7 or Core i7 equivalent)
- RAM: 32 GB DDR4
- Storage: NVMe SSD (~3 GB/s read, ~2 GB/s write)
- OS: Windows 10 x64 (same results expected on Linux with equivalent hardware)
- Build config: Debug build, `TML_DEFAULT_BACKEND=llvm`

### Phase Breakdown for LLVM (Single Module, ~500 Lines of TML)

The full pipeline from source file to `.obj`:

| Phase | Estimated Time | Measurement Point |
|-------|---------------|-------------------|
| File read + tokenize | ~1 ms | QueryContext: `Tokenize` |
| Parse (AST) | ~4 ms | QueryContext: `ParseModule` |
| Type check | ~12 ms | QueryContext: `Typecheck` |
| Borrow check | ~3 ms | QueryContext: `Borrowcheck` |
| HIR lowering | ~5 ms | QueryContext: `HirLower` |
| THIR lowering | ~3 ms | QueryContext: `ThirLower` |
| MIR building | ~4 ms | QueryContext: `MirBuild` |
| MIR passes (mem2reg, etc.) | ~2 ms | `MirPassManager::run` |
| MirCodegen → IR text | ~8 ms | `MirCodegen::generate` |
| LLVM: parse IR text | ~8 ms | `LLVMParseIRInContext` |
| LLVM: optimization (O0) | ~12 ms | `LLVMRunPasses("default<O0>")` |
| LLVM: optimization (O3) | ~180 ms | `LLVMRunPasses("default<O3>")` |
| LLVM: emit to object | ~25 ms | `LLVMTargetMachineEmitToFile` |
| **Total (O0, single module)** | **~87 ms** | Wall clock from source read |
| **Total (O3, single module)** | **~255 ms** | Wall clock from source read |

Note: These are estimates based on pipeline analysis and published LLVM benchmarks.
Actual measurements may vary 20-30% depending on module complexity (generics, function
count, template depth). The key insight is that the **frontend (source → MIR)** accounts
for ~34 ms regardless of backend — this is the irreducible minimum for any backend.

### Multi-Module Build: Where Backend Matters

For a project with N modules compiled in parallel on 8 cores:

| Modules | Frontend total | LLVM O0 backend | LLVM O3 backend | Wall clock (O0) | Wall clock (O3) |
|---------|---------------|----------------|----------------|-----------------|-----------------|
| 1 | 34 ms | 53 ms | 221 ms | 87 ms | 255 ms |
| 10 | 340 ms | 530 ms | 2,210 ms | 113 ms (8-core) | 364 ms (8-core) |
| 50 | 1,700 ms | 2,650 ms | 11,050 ms | 550 ms (8-core) | 1,944 ms (8-core) |
| 100 | 3,400 ms | 5,300 ms | 22,100 ms | 1,088 ms (8-core) | 3,888 ms (8-core) |

The backend fraction grows as module count increases and the frontend becomes a smaller
bottleneck relative to the LLVM O3 optimization time.

### Code Quality Baseline (LLVM O3 = 100%)

LLVM O3 output quality is the reference. All other backends are measured against it:

| Backend / Opt Level | Integer throughput | Float throughput | I/O-bound | Memory-bound |
|--------------------|------------------|-----------------|-----------|-------------|
| LLVM O3 (baseline) | 100% | 100% | 100% | 100% |
| LLVM O2 | ~93% | ~88% | ~99% | ~95% |
| LLVM O1 | ~78% | ~70% | ~98% | ~85% |
| LLVM O0 | ~52% | ~45% | ~95% | ~60% |

"I/O-bound" programs (HTTP servers, file processors) see minimal quality difference because
CPU instruction quality is not the bottleneck. "Integer throughput" programs (parsers,
compilers, crypto) see the largest differences.

---

## 2. Target: Cranelift Backend

### Compilation Speed

Cranelift replaces the `LLVMParseIRInContext` + `LLVMRunPasses` + `LLVMTargetMachineEmitToFile`
sequence with its own pipeline. The input is also different: instead of LLVM IR text,
Cranelift receives MIR directly, eliminating the IR generation step entirely.

| Phase | LLVM O0 | Cranelift | Notes |
|-------|---------|-----------|-------|
| Frontend (source → MIR) | 34 ms | 34 ms | Unchanged — shared pipeline |
| IR text generation | 8 ms | 0 ms | Eliminated — Cranelift reads MIR |
| Backend compilation | 45 ms | 8-15 ms | 3-5x faster than LLVM O0 |
| **Total (single module)** | **87 ms** | **42-49 ms** | **1.8-2.1x faster** |

The improvement is more pronounced for projects with many modules:

| Modules | LLVM O0 wall clock | Cranelift wall clock | Speedup |
|---------|-------------------|---------------------|---------|
| 1 | 87 ms | 45 ms | 1.9x |
| 10 (8-core) | 113 ms | 65 ms | 1.7x |
| 50 (8-core) | 550 ms | 270 ms | 2.0x |
| 100 (8-core) | 1,088 ms | 510 ms | 2.1x |

### Code Quality (Cranelift)

Cranelift implements linear-scan register allocation, basic peephole optimization, and
instruction selection without inter-procedural analysis. Published data from
`rustc_codegen_cranelift` on representative Rust benchmarks:

| Benchmark type | Cranelift vs LLVM O0 | Cranelift vs LLVM O3 |
|----------------|---------------------|---------------------|
| Integer-heavy (parser, compiler) | ~95% of LLVM O0 | ~49% of LLVM O3 |
| Floating point (math, simd) | ~88% of LLVM O0 | ~40% of LLVM O3 |
| I/O-bound (HTTP, file ops) | ~99% of LLVM O0 | ~95% of LLVM O3 |
| Memory-bound (collections) | ~92% of LLVM O0 | ~55% of LLVM O3 |
| **Typical TML program** | **~93% of LLVM O0** | **~50% of LLVM O3** |

For development builds (the primary Cranelift use case), 50% of LLVM O3 quality is
acceptable. The developer is iterating on correctness, not measuring performance. When
performance matters, `--release` uses LLVM O3.

For production programs where LLVM O3 is unavailable (minimal distribution, embedded
deployment), Cranelift provides acceptable quality for I/O-bound services (>95% quality)
and marginal quality for compute-intensive code (~50%).

### Cranelift Acceptance Criteria (Phase 1 Gate)

Before merging Phase 1:

| Metric | Target | Measurement |
|--------|--------|------------|
| Single module compile (O0 equivalent) | ≤ 50 ms | `tml-bench compile single` |
| 50-module build (8 cores) | ≤ 280 ms | `tml-bench compile large` |
| Cranelift DLL size | ≤ 6 MB | `dir tml_codegen_cranelift.dll` |
| All core/ tests pass | 100% | `mcp__tml__test suite="core"` |
| All std/ tests pass | 100% | `mcp__tml__test suite="std"` |
| Debug info present | DWARF sections in output | `dumpbin /all output.obj` |

---

## 3. Target: Custom x86_64 Backend

### Compilation Speed

The custom backend eliminates both IR text generation (which Cranelift already eliminates)
and the Cranelift library overhead (initialization, IR verification, register allocator
setup). It operates directly on MIR with no intermediate representation.

| Phase | Cranelift | Custom (O0) | Notes |
|-------|-----------|-------------|-------|
| Frontend (source → MIR) | 34 ms | 34 ms | Unchanged |
| IR text generation | 0 ms | 0 ms | Both skip IR text |
| Backend init overhead | 2-5 ms | 0.1-0.5 ms | No Rust runtime, no Cranelift registry |
| Instruction selection | 4-8 ms | 2-4 ms | Direct MIR → x86_64 |
| Register allocation | 2-4 ms | 1-2 ms | Linear scan, same algorithm |
| Code emission + COFF write | 2-3 ms | 1-2 ms | Simpler than Cranelift's object writer |
| **Total (single module)** | **~45 ms** | **~38 ms** | **~1.2x faster than Cranelift** |

The custom backend's advantage over Cranelift is modest for single modules. It becomes
significant at scale because it has no Rust runtime initialization overhead per invocation,
and its data structures are optimized for TML's specific MIR output patterns.

### Code Quality Evolution

The custom backend starts without optimization and improves over time as optimization
passes are added.

| Sub-phase | Optimization | Quality vs LLVM O3 | Timeframe |
|-----------|--------------|--------------------|-----------|
| 3f: O0 baseline | None | ~42-48% | Phase 3f complete |
| + Copy propagation | Remove redundant mov | ~52-56% | 1 month post-3f |
| + Peephole patterns | ~50 common x86 rewrites | ~58-63% | 2 months post-3f |
| + Better regalloc (regalloc2) | Fewer spills | ~65-70% | 4 months post-3f |
| + Instruction scheduling | Avoid pipeline stalls | ~72-78% | 6 months post-3f |
| + Function inlining | Small functions inlined | ~78-84% | 12 months post-3f |
| Mature (24 months post-3f) | Full peephole + sched | ~85-90% | 24 months |

The 85-90% target at 24 months positions the custom backend alongside LLVM O1 / Cranelift
with inlining. This is sufficient quality for all but the most performance-critical
production deployments.

### Custom Backend Acceptance Criteria (Phase 3f Gate)

| Metric | Target | Measurement |
|--------|--------|------------|
| Single module compile | ≤ 40 ms | `tml-bench compile single` |
| 50-module build (8 cores) | ≤ 240 ms | `tml-bench compile large` |
| Backend binary size | ≤ 3 MB | Custom backend component in `tml.exe` |
| All core/ tests pass | 100% | Same as Cranelift gate |
| All std/ tests pass | 100% | Same as Cranelift gate |
| Code quality vs LLVM O0 | ≥ 80% on `tml-bench perf` | Benchmark suite (integer workloads) |
| COFF output validity | Passes `dumpbin /all` | All output files |
| No memory leaks | 0 leaks on `tml-bench compile large` | `mcp__tml__debug check_leaks=true` |

---

## 4. Acceptable Tradeoffs by Use Case

### Developer Iteration (Most Common Case)

The developer writes code, builds, runs tests, fixes a bug, repeats. A typical cycle:
1. Edit 1-3 source files (~100-500 lines changed)
2. `tml build` — only changed files recompile (incremental)
3. Incremental compile: 1-3 modules × frontend time = ~35-100 ms

For this use case, backend quality is irrelevant. The developer needs correct code, not
fast code. Both Cranelift and the custom backend provide this.

| Requirement | Cranelift | Custom | LLVM O0 |
|-------------|-----------|--------|---------|
| Correct code output | Yes | Yes | Yes |
| Debug info (breakpoints) | Partial | Yes (Phase 3g+) | Yes |
| Incremental compile benefit | Yes | Yes | Yes |
| Speed (incremental) | ~45ms/module | ~38ms/module | ~87ms/module |
| **Verdict** | **Good** | **Best** | **Acceptable** |

### Test Suite Execution

TML's test suite compiles one DLL per test suite (~50 suites) and runs them in parallel.
The compilation phase dominates when running `--no-cache`.

| Metric | LLVM O0 | Cranelift | Custom |
|--------|---------|-----------|--------|
| 50-suite full compile (8 cores) | ~550 ms | ~270 ms | ~240 ms |
| Per-suite recompile (1 change) | ~87 ms | ~45 ms | ~38 ms |
| DLL link time (LLD) | ~50 ms/DLL | ~50 ms/DLL | ~5 ms/DLL (tml-link Phase 3) |
| **Total: no-cache run** | **~27.5 s** | **~16 s** | **~14 s** |

Cranelift plus tml-link (Phase 3 of linker analysis) cuts total test suite wall time from
~27.5 seconds to ~16 seconds for a no-cache run. The custom backend adds another ~2 seconds
of improvement.

### Release Build (Distribution Binary)

A release build compiles everything from scratch with maximum optimization. Quality
matters; speed is secondary.

| Backend | Build time (100 modules) | Code quality | Recommended |
|---------|--------------------------|--------------|-------------|
| LLVM O3 | ~40 min (single thread) | 100% | Yes — for shipping |
| LLVM O2 | ~8 min | ~93% | Acceptable |
| LLVM O1 | ~4 min | ~78% | Marginal |
| Cranelift | ~0.9 min | ~50% | No — too slow code |
| Custom mature | ~1.2 min | ~88% | Maybe — at 24 months |

LLVM O3 remains the right choice for release builds throughout the foreseeable future.
The custom backend's quality trajectory suggests it could replace LLVM O1/O2 for some
use cases by 24 months post-Phase 3, but LLVM O3 will remain superior for
compute-intensive programs indefinitely.

---

## 5. Backend Selection Algorithm

The compiler selects a backend according to:

```
Step 1: Check explicit --backend= flag
  --backend=llvm      → use LLVM (any opt level)
  --backend=cranelift → use Cranelift
  --backend=custom    → use custom backend

Step 2: Check --release flag
  --release (no --backend) → use LLVM O3 if available, else error

Step 3: Default (dev build)
  if custom backend available:
      use custom backend
  elif cranelift available:
      use cranelift
  else:
      use LLVM O0 (fallback)

Step 4: Availability check
  "available" means: the backend DLL is loadable on this machine
  LLVM: tml_codegen_x86.dll present and loadable
  Cranelift: tml_codegen_cranelift.dll present and loadable
  Custom: built into tml.exe (always available in Phase 3+)
```

### Phase-by-Phase Defaults

| Phase | Default for `tml build` | Default for `tml build --release` |
|-------|------------------------|----------------------------------|
| Today | LLVM O0 | LLVM O3 |
| After Phase 1 | Cranelift | LLVM O3 |
| After Phase 3 | Custom (O0) | LLVM O3 |
| After Phase 3 + mature custom | Custom (O1 equivalent) | LLVM O3 or custom (user choice) |

---

## 6. Benchmark Suite Design

### Synthetic Benchmark Programs

The benchmark suite measures compilation time and code quality for four synthetic programs
that span the realistic range of TML program complexity.

**`bench-tiny`** — 1 module, ~50 functions, ~600 lines of TML
- Purpose: measure irreducible overhead (frontend min, backend init)
- Functions: arithmetic, string formatting, simple structs
- Expected frontend time: ~20 ms
- Expected backend time: Cranelift ~6 ms, Custom ~5 ms, LLVM O0 ~22 ms

**`bench-single`** — 1 module, ~200 functions, ~2,500 lines of TML
- Purpose: measure backend scaling with function count
- Functions: full standard library module equivalent
- Expected frontend time: ~34 ms
- Expected backend time: Cranelift ~12 ms, Custom ~9 ms, LLVM O0 ~45 ms

**`bench-medium`** — 10 modules, ~80 functions each, ~800 total functions
- Purpose: measure incremental compilation and module-level parallelism
- Expected total (8 cores): Cranelift ~65 ms, Custom ~55 ms, LLVM O0 ~113 ms

**`bench-large`** — 50 modules, ~50 functions each, ~2,500 total functions
- Purpose: simulate a full standard library compilation
- Expected total (8 cores): Cranelift ~270 ms, Custom ~240 ms, LLVM O0 ~550 ms

### Code Quality Benchmark Programs

Code quality is measured by running programs and comparing output performance:

**`perf-fib`** — Fibonacci (recursive, integer-heavy)
- Measures: basic integer arithmetic quality, tail call optimization
- LLVM O3: ~100 ns/call. Cranelift: ~190 ns. Custom initial: ~215 ns.

**`perf-sha256`** — SHA-256 of 1 MB of data
- Measures: integer pipeline, loop optimization, memory access patterns
- LLVM O3: ~2.1 ms. Cranelift: ~3.8 ms. Custom initial: ~4.2 ms.

**`perf-json-parse`** — Parse a 1 MB JSON file
- Measures: branch prediction, string handling, allocation patterns
- LLVM O3: ~12 ms. Cranelift: ~14 ms (I/O bound, small difference). Custom: ~14.5 ms.

**`perf-http-serve`** — Serve 100,000 HTTP/1.1 requests (loopback)
- Measures: I/O-bound workload, syscall overhead dominates
- LLVM O3: ~60K RPS. Cranelift: ~58K RPS. Custom initial: ~57K RPS.

### Benchmark Harness

```
build/debug/bin/tml-bench compile <program> [--iterations=N] [--warmup=M] [--backend=X]
build/debug/bin/tml-bench perf <program> [--iterations=N] [--backend=X]
```

The harness reports:
- Median wall-clock time (compilation) over N iterations (default: 30) after M warmup (default: 5)
- P95 and P99 wall-clock time
- Peak RSS during compilation
- Code quality ratio vs LLVM O3 (for `perf` benchmarks)
- Backend name and version

Results written to `.benchmark-history.json` in NDJSON format for trend tracking.

---

## 7. Regression Gates

### Phase 1 Regression Gate (Cranelift Default)

A regression check runs on every commit touching `compiler/src/codegen/cranelift/`:

| Check | Pass Condition |
|-------|---------------|
| `bench-single` Cranelift median | ≤ 52 ms |
| `bench-large` Cranelift median (8 cores) | ≤ 295 ms |
| `bench-single` LLVM O0 median | ≤ 92 ms (LLVM must not regress) |
| All test suites | 100% pass |
| `tml_codegen_cranelift.dll` size | ≤ 6.5 MB |

### Phase 3 Regression Gate (Custom Backend)

| Check | Pass Condition |
|-------|---------------|
| `bench-single` Custom median | ≤ 45 ms |
| `bench-large` Custom median (8 cores) | ≤ 260 ms |
| `perf-sha256` Custom vs LLVM O0 | ≥ 78% |
| `perf-fib` Custom vs LLVM O0 | ≥ 80% |
| All test suites | 100% pass |
| Custom backend component size | ≤ 3.5 MB |

### Regression Detection Rule

A regression is flagged when the median time for any benchmark increases by more than
**8%** vs the rolling 7-day baseline stored in `.benchmark-history.json`. The threshold
is 8% rather than 5% to account for machine-to-machine variance in CI environments.

---

## 8. Success Criteria Summary

The overall backend strategy is successful when these metrics are achieved simultaneously:

| Metric | Current | Phase 1 Target | Phase 3 Target |
|--------|---------|---------------|---------------|
| Default dev build speed (single module) | 87 ms | 50 ms | 40 ms |
| 50-module build (8 cores) | 550 ms | 280 ms | 240 ms |
| Release build quality | LLVM O3 (100%) | LLVM O3 (100%) | LLVM O3 (100%) |
| Compiler binary size (default backend DLL) | 78 MB | 5-6 MB | Built-in to tml.exe |
| tml.exe total size (without optional LLVM) | ~119 MB | ~51 MB | ~12-15 MB |
| Test suite no-cache run (total) | ~27.5 s | ~16 s | ~14 s |
| External deps for end user | Zig CC (build) | None | None |
| External deps for dev (build from source) | Zig CC | Zig CC + Rust | Zig CC |
| Cross-compilation support | Via LLVM | Via LLVM | Via Cranelift + custom |

The Phase 1 targets are achievable within the 2-4 month timeline described in
`06-hybrid-strategy.md`. The Phase 3 targets depend on the custom backend reaching
production stability, estimated at 12-18 months of development.
