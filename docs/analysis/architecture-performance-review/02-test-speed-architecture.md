# 02 — Test-Speed Architecture

## Overview

Test speed is the most-felt symptom of the architectural conflicts. Most *tactical* fixes landed in phases 40–43; the remaining bottlenecks are structural.

---

### F-005 — Default was one native EXE per test file (~2066 link cycles)

**Impact: Very High**  
**Status: RESOLVED (phase41a, aggregation 25/EXE, 11.7× fewer links)**

**Evidence and resolution:**  
- `docs/analysis/tooling-performance/04-test-framework-performance.md` (F-005)
- `compiler/src/cli/commands/cmd_test.cpp`, `testing_compile.cpp`

**What changed:**

The test framework now aggregates 25 test files per native EXE instead of one-per-file, reducing link cycles from ~2066 to ~176 (11.7× fewer).

- Compile-failure and crash/timeout isolation preserved via per-file codegen + shared linking
- Parity verified: `test_aggregation.sh` 16/16
- Full-run now feasible as a single job without manual `--suite-mode` flag

---

### F-006 — Each test EXE re-emitted the full stdlib (~5000 fns)

**Impact: Very High**  
**Status: RESOLVED (phase43a v0.3.79, "Option B") — but the *reason* it was hard is F-001**

The shared-stdlib fast-path was disabled for months. The blocker was F-001: the AST path's eager monomorphization produced a "K001 in a different never-exercised body" each time one was patched — a treadmill documented in `docs/analysis/tooling-performance/04-test-framework-performance.md:14-28`.

This is dual-path debt manifesting as test-speed debt.

---

### F-007 — Tests execute as OS subprocesses (per-suite), each loading ~100 MB of runtime DLLs

**Impact: Medium-High (was), structural**  
**Status: OPEN**

**Forced architecture:**  
`docs/analysis/compiler-internals/single-binary-test-compilation.md:68-72` is explicit:

> In-process thread-per-test (the Go/Rust/Zig model) "Requires TML to support thread-level panic isolation, which doesn't exist."

The subprocess model is forced by a **language/runtime gap**, not chosen for speed.

**Current mitigation:**  
Aggregation (F-005) cut process count ~12×. But the per-spawn + DLL-load cost per suite remains and cannot be removed without in-process panic isolation.

**Remaining cost:**  
When suite-count ≥ worker-count, multiple suites serialize, and the ~500 ms per-spawn (DLL load + initialization) is not recoverable at the subprocess level.

---

### F-008 — Per-file codegen inside a suite is forced single-threaded

**Impact: Medium**  
**Status: OPEN, blocked on F-006-class restructuring**

`testing_compile.cpp` historically hardcoded `num_compile_threads = 1`. Parallelism exists only at suite level (≤8 concurrent suites).

**Critical path scenario:**  
When suite-count ≥ worker-count, a 25-file chunk with heavy generics becomes a long serial bottleneck. Measured: `std_stream_1` = 779 s cold.

This is due to LLVM global state (no per-thread IR contexts); lifting this requires the shared-stdlib restructuring from F-006.

---

### F-009 — Debug `-O0` compiler was the daily driver

**Impact: High**  
**Status: RESOLVED (phase40b, release build ~2× faster)**

A release build (`-O2`/`-O3`) is now available via `scripts\build.bat release`.

**Decision made (documented, not flipped):**  
MCP/daemon deliberately stay on debug for reasons documented in `docs/analysis/tooling-performance/02-build-performance.md:22-28`:

- Cache is debug-anchored (alternating builds thrashes the 837 MB EXE cache)
- Incremental debug rebuild is the daily C++ fix loop
- Marginal benefit: daemon already serves cache-hit `check` in ~7 ms

**Interactive workflow impact:**  
A user doing `scripts\build.bat release && /test ...` workflow gets ~2× speedup; a typical agent workflow using MCP stays on debug (~460 ms cold `check`, ~7 ms warm).

---

### F-010 — MCP tool calls spawned a fresh cold `tml.exe` and never used the warm daemon

**Impact: Very High**  
**Status: RESOLVED (phase40a, 452 ms → 6.6 ms warm `check`)**

`docs/analysis/tooling-performance/05-mcp-warm-state.md`

MCP now connects to the warm daemon, turning repeated `check` calls from ~460 ms cold into ~7 ms warm-hit.

---

### F-011 — Result cache was fingerprinted by the 71 MB DLL's mtime:size

**Impact: High**  
**Status: RESOLVED (phase41c, content-addressed CRC + VERSION)**

Every compiler rebuild wiped 837 MB of cached EXEs because the cache key was keyed on the binary's mtime/size.

**Worth calling out as an anti-pattern:**  
A cache keyed on the wrong thing made the dominant workflow (rebuild-then-test) always cold. The cache that should make reruns free was *structurally guaranteed cold*.

The fix (phase41c) uses content-addressed CRC + VERSION file, so cache survives rebuild if the binary didn't actually change.
