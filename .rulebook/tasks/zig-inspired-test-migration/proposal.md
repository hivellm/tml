# Proposal: zig-inspired-test-migration

## Why

The current test system (v3, rewrite-test-system) solved the critical problems (coverage hangs, crash isolation, DLL loading) but kept the architecture of **206 independent binaries**, each re-generating stdlib IR, linking separately, and executing as isolated subprocesses. Result: **~10min for 1452 tests** (cached), **~50-70min uncached**.

Zig runs ~1000 tests in **1-3 seconds** because:
1. Compiles everything into a **single compilation unit** (stdlib processed once)
2. Produces **1 binary** (1 link step, not 206)
3. Uses a **native backend** (not LLVM) for debug builds
4. Runs **in-process** (no subprocess overhead)

The gap is **200-400x**. Even without a native backend (#3), optimizations #1 and #2 alone can reduce 10min to **<1min**.

### Diagnosis: Where Time Goes Today

| Stage | Time | Repetitions | Total | % |
|-------|------|-------------|-------|---|
| Stdlib codegen (IR) | ~8s/suite | 206 suites | ~1648s | **55%** |
| LLVM backend (IR→obj) | ~5s/suite | 206 suites | ~1030s | **34%** |
| LLD link | ~1.5s/suite | 206 suites | ~309s | **10%** |
| Subprocess spawn+NDJSON | ~0.1s/suite | 206 suites | ~21s | **<1%** |

**Stdlib codegen runs 206 times identically.** Each `QueryContext::codegen_unit()` re-generates IR for `core::str`, `core::fmt`, `core::iter`, etc. This is the equivalent of compiling libc 206 times.

## What Changes

Complete replacement of test compilation and execution architecture in 6 phases:

### Phase 1: Stdlib Pre-Compiled Object Cache (Quick Win — highest ROI)
- Compile the entire stdlib to `.obj` **once** before tests
- Each suite links against pre-compiled `.obj` instead of re-generating IR
- Eliminates 205 redundant codegen passes (~1648s → ~8s)
- **Expected speedup: 3-5x** (10min → 2-3min)

### Phase 2: Suite Aggregation (Mega-Binary)
- Group suites by parent module: `core` (1 binary), `std` (1 binary), `compiler` (1 binary)
- 206 links → 3-5 links
- 206 dispatchers → 3-5 mega-dispatchers
- **Expected speedup: 2-3x additional** (2-3min → 1min)

### Phase 3: Incremental Object Cache
- Cache `.obj` per test file (IR fingerprint)
- On second run, only recompile files that changed
- Combined with stdlib pre-compiled: changing 1 test = recompile 1 `.obj` + 1 re-link
- **Expected speedup: 10-50x for incremental runs** (1min → 2-5s)

### Phase 4: In-Process Execution Mode
- Option `--in-process`: load test as DLL and execute in same process
- Eliminates subprocess overhead for non-crashing tests
- Fallback to subprocess on crash (retry isolation)
- **Expected speedup: 1.5-2x** (subprocess overhead eliminated)

### Phase 5: Unified Test Binary (Zig Model)
- Compile ALL tests + stdlib into **1 single executable**
- 1 codegen pass, 1 link step, 1 process
- Mega-dispatcher with table of 1452 tests
- Crash isolation via SEH/signal handler (not subprocess)
- **Target: <30s full suite, <5s incremental run**

### Phase 6: Optional Native Backend (Future)
- Lightweight codegen backend (like Zig's x86 backend) for debug builds
- Bypass LLVM entirely for tests
- **Target: <5s full suite** (Zig-equivalent)

## Impact
- Affected specs: docs/specs/09-CLI.md, docs/specs/10-TESTING.md
- Affected code: compiler/src/testing/ (all 9 files), compiler/include/testing/ (all 9 headers)
- Breaking change: NO (CLI interface preserved, internal improvement)
- User benefit: 10-100x faster tests, instant feedback loop during development

## Supersedes
- `single-binary-test-compilation` — Phase 1 (--run-all) already implemented, Phases 2-3 absorbed here as Phases 2 and 5
- Performance items from `improve-test-infrastructure` Phase 8
