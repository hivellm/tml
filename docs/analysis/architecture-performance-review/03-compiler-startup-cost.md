# 03 — Compiler Startup Cost

## Overview

Cold invocation of the TML compiler loads 123 MB of monolithic DLLs, preloads metadata, and compiles with optimization disabled. These fixed per-invocation overheads dominate compile time for small programs.

---

### F-012 — 123 MB of monolithic plugin DLLs load on every invocation; no lazy/mmap load

**Impact: Medium (High when cold)**  
**Status: OPEN**

**Measured sizes:**  
- `tml_compiler.dll` — 71 MB
- `tml_codegen_x86.dll` — 52 MB

Load time: ~30% of a cold compile (`docs/analysis/benchmark/08-compilation.md:29`).

**Current behavior:**  
- Codegen DLL loads even for `check` (type-only) work
- No lazy-load or memory-mapped loading strategy

**Why this matters:**  
In the daemon/warm-state model, this is acceptable (load once, serve many calls). In the cold-compile model (CI, isolated checkers, batch migration scripts), this is a real cost.

**Potential mitigation:**  
Lazy-load the codegen DLL until actually needed, or use memory-mapping for the data sections.

---

### F-013 — `check` eagerly preloads all core/std `.meta` regardless of imports

**Impact: Medium**  
**Status: PARTIALLY RESOLVED (phase42c mtime fast-path; eager preload kept for diagnostic parity, ADR-010)**

`docs/analysis/tooling-performance/03-check-performance.md` (F-015)  
`docs/adr/ADR-010-check-query-routing.md`

**Current state:**  
- Mtime-based fast-path for unchanged `.meta` files added in phase42c
- Eager preload strategy preserved for diagnostic parity (ensures all errors are reported consistently)

**Rationale:**  
The trade-off is deliberate: eager preload guarantees that a `check` on file A reports the same errors as a full-repo `check`, even if A doesn't import X or Y. This is a quality/UX decision.

---

### F-014 — 27× slower compile than Rust, dominated by fixed per-invocation overhead

**Impact: High**  
**Confidence: Medium (debug-mode confound)**

`docs/analysis/benchmark/08-compilation.md:1-37`

**Measured breakdown:**
- DLL load: 30%
- `-O0` compiler: large component (debug mode)
- Single-threaded pipeline
- No PCH on the default Zig toolchain (`docs/analysis/tooling-performance/02-build-performance.md` F-003)

**Benchmark fairness caveat:**  
The benchmark's own note (`benchmark/README.md:42`) flags that *TML-debug-vs-Rust-release* is the largest confound. But the *fixed* overheads (DLL load, meta preload) are real and program-size-independent.

**For production shipping:**  
Rust is compiled `-O2`/`-O3` by default; TML's default is `-O0`. When both are optimized, the gap is smaller, but the fixed overhead component (DLL load) remains.

---

### Summary: The startup-cost floor

The per-invocation floor is dominated by:
1. **DLL load** (~30%, cold)
2. **Metadata preload** (mtime-aware, usually cached)
3. **Compiler optimization level** (tied to debug vs release decision, F-009)

The daemon/warm-state model (phase40a) pushes past this floor by keeping the compiler resident. Cold invocation remains expensive, but is less common in the daily workflow.
