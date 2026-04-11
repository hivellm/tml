# 02 — Alternative Code Generation Backends

**Date**: 2026-04-05
**Status**: Complete
**Companion**: [01-current-llvm-dependency.md](01-current-llvm-dependency.md)

---

## Overview

This document evaluates every viable alternative to LLVM for native code generation in the
TML compiler. The goal is to inform the phased backend strategy: which backend to use for
developer iteration (Phase 1), which to keep for release optimization (Phase 2), and what
to build for zero-dependency self-hosting (Phase 3).

Each candidate is evaluated against eight criteria directly relevant to TML's needs.

---

## Decision Criteria

| Criterion | Weight | What We're Measuring |
|-----------|--------|---------------------|
| **Code Quality** | High | Output performance vs LLVM O3 baseline. Dev builds need ~O1 quality; release builds need O3 quality. |
| **Compilation Speed** | High | Milliseconds from IR/MIR to `.obj`. Target: <15ms per module at dev opt level. |
| **Binary Size** | Medium | Static library size when linked into `tml_codegen_*.dll`. Target: <10 MB. |
| **Debug Info** | Medium | DWARF (Linux/Mac) and PDB/CodeView (Windows) support quality. |
| **Target Support** | Medium | x86_64 required. AArch64 strongly preferred. Others optional. |
| **Maturity** | High | Production usage track record. Compiler segfaults during TML development would be unacceptable. |
| **Integration Effort** | High | Person-months to wire into TML's `CodegenBackend` interface and pass tests. |
| **License** | Critical | Must be Apache-2.0, MIT, BSD, or similar. GPL is incompatible. |

---

## Candidate 1: Cranelift

**Recommendation: ADOPT for Phase 1 (development builds)**

### Background

Cranelift is a code generation library developed by the Bytecode Alliance, primarily as the
JIT compiler for the Wasmtime WebAssembly runtime. It is written in Rust and exposes a
C API through the `cranelift-c-api` crate. As of 2025, it is also used experimentally by
rustc as an alternative backend (`rustc_codegen_cranelift`) for faster debug builds.

TML already has a partial Cranelift integration:
`compiler/src/codegen/cranelift/cranelift_codegen_backend.cpp` (177 lines) implements
`BackendCapabilities`, option translation, and object writing infrastructure. The
`compile_mir()` body is the remaining work.

### Detailed Assessment

**Code Quality**: Cranelift targets approximately LLVM O1 equivalent quality. It performs
register allocation (linear scan by default, regalloc2 in newer versions), basic instruction
selection, and simple peephole optimizations. It does not perform inter-procedural
optimization, vectorization, or aggressive inlining. For development builds — where the
developer will rebuild 10–50 times per hour — this quality level is entirely acceptable.
For release builds distributed to end users, LLVM O3 remains superior.

Benchmarks from the rustc_codegen_cranelift project show that Cranelift-compiled Rust
code runs approximately 15–25% slower than LLVM O0 output, and roughly 40–60% slower than
LLVM O2 for compute-heavy workloads. For I/O-bound programs (most web services, tools),
the gap is smaller — often 5–10%.

**Compilation Speed**: Cranelift is 5–10x faster than LLVM O0 for equivalent input sizes.
In the rustc experiment, switching to Cranelift reduced debug build times from ~45 seconds
to ~12 seconds for medium-sized Rust crates. For TML, estimated per-module time drops from
36–73 ms (LLVM O0) to 8–18 ms (Cranelift).

**Binary Size**: The Cranelift C API library links to approximately 5 MB when statically
compiled. This is 94% smaller than LLVM's ~78 MB contribution. The `tml_codegen_cranelift.dll`
would be approximately 5–6 MB total — small enough to ship as the default backend for
development builds.

**Debug Info**: Cranelift has DWARF support through its `cranelift-debuginfo` crate. DWARF
emission is functional on Linux and macOS. PDB/CodeView (Windows) support is experimental
and incomplete as of 2025. Phase 1 should skip debug info — developers relying on debuggers
can fall back to `--backend=llvm`. Phase 3 (custom backend) is where full PDB support should
be implemented.

**Target Support**: x86_64 fully supported. AArch64 fully supported (used in Wasmtime on
Apple Silicon in production). RISC-V support is in development. s390x is supported.
Windows x86_64 (MSVC ABI) is supported — important for TML's primary development platform.

**Maturity**: Cranelift is production-quality for WebAssembly JIT. Its use in `tml run`
(JIT execution of TML programs) would make it the primary runtime backend. Its use in AOT
compilation (the main `tml build` path) is less battle-tested but has been validated by the
rustc experiment. The regalloc2 register allocator used since Cranelift 0.100 is formally
verified — a rare correctness guarantee.

**Integration Effort**: The `CodegenBackend` interface is already defined and the partial
implementation exists. Completing the integration requires:
1. Wiring `compile_mir()` to serialize TML MIR to Cranelift IR (or LLVM IR text for
   Cranelift's LLVM IR importer)
2. Calling the Cranelift C API to compile and emit object bytes
3. Writing object bytes to a `.obj` file and returning `CodegenResult`
4. Running the existing test suite to identify failures

Estimated effort: **2–4 months** for one engineer, including test debugging and Windows
ABI edge cases.

**License**: Apache-2.0 with LLVM exception. Fully compatible.

### Integration Path

The cleanest integration uses Cranelift's LLVM IR importer (`cranelift-llvm-importer`),
which accepts LLVM IR text as input — the exact format TML's MirCodegen already produces.
This avoids the need to write a TML MIR → Cranelift IR translator.

```
MirCodegen::generate()
  → std::string (LLVM IR text, exactly as today)
  → cranelift_llvm_import(ir_text)   ← new path
  → Cranelift Module
  → cranelift_compile_to_object()
  → .obj bytes
```

Alternatively, a TML MIR → Cranelift IR translator can be written for a tighter
integration that avoids the LLVM IR text format entirely. This produces slightly better
Cranelift output (avoids losing Cranelift-specific hints in the LLVM IR round-trip) but
requires more implementation work.

---

## Candidate 2: QBE

**Recommendation: VIABLE but lower priority than Cranelift**

### Background

QBE (Quick Backend) is a lightweight compiler backend written by Quentin Carbonneaux in
approximately 12,000 lines of C. It is used as the backend for the Hare programming
language compiler and the `cproc` C compiler. QBE accepts a simple SSA intermediate
representation (not LLVM IR) and emits assembly for x86_64, AArch64, and RISC-V.

### Detailed Assessment

**Code Quality**: QBE produces code comparable to GCC O1 for typical code patterns. It
performs copy elimination, simple dead code elimination, and passes to POSIX `as` (system
assembler) for final encoding. It does not perform vectorization, function inlining across
compilation units, or aggressive alias analysis. For developer builds, this quality is
acceptable. For compute-heavy code, output may be noticeably slower than Cranelift.

**Compilation Speed**: QBE is the fastest option available. It is specifically designed
for simplicity and speed — the Hare compiler builds multi-thousand-line programs in under
100 ms on a modern machine. Per-file overhead is approximately 2–8 ms, making it 5–20x
faster than LLVM O0.

**Binary Size**: QBE is a single C source file (12,000 lines). When compiled as a static
library, it links to approximately 400–500 KB. This is the smallest possible option.

**Debug Info**: QBE emits basic DWARF via assembly directives. The quality is sufficient
for `gdb` stack traces but lacks the metadata richness that LLVM and Cranelift provide.
Line information is present; type information is minimal.

**Target Support**: x86_64 (stable), AArch64 (stable), RISC-V (in development). No Windows
MSVC ABI support — QBE targets System V AMD64 ABI on Linux/macOS. This is a significant
limitation for TML's primary development platform (Windows x86_64 MSVC).

**Maturity**: QBE is production-ready for its target use case (simple compiled languages).
Hare uses it in production. However, it is maintained by a single developer, has no C API
(the interface is text-based via the QBE IR format piped through stdin/stdout), and
receives updates at the pace of hobby project development.

**Integration Effort**: QBE requires more integration work than Cranelift for TML because:
1. No C API — must either embed QBE as a library (requires extracting a linkable interface)
   or run QBE as a subprocess (adds fork/exec overhead)
2. QBE uses its own IR format — requires writing a TML MIR → QBE IR translator
3. Windows MSVC ABI is unsupported — would require porting QBE or restricting QBE backend
   to Linux/macOS builds only

Estimated effort: **4–7 months** (3–5 months for QBE IR integration + 1–2 months for
Windows compatibility workaround or build-system gating).

**License**: MIT. Fully compatible.

### Verdict

QBE is an excellent option for Linux/macOS TML builds where binary size is the top
priority. For TML's primary Windows x86_64 platform, the lack of MSVC ABI support makes
it a non-starter for Phase 1. QBE could be the Linux/macOS backend in Phase 2 while
Cranelift handles Windows — but this adds complexity. Cranelift supports all three
platforms from a single integration and is the cleaner choice.

---

## Candidate 3: Custom Native Backend

**Recommendation: ADOPT for Phase 3 (zero-dependency self-hosting)**

### Background

A custom backend takes TML's MIR directly to machine code without any intermediate format
or third-party library. This is the approach taken by the Go compiler (`gc`), the V
language compiler, and (in progress) Zig's self-hosted backend.

### Detailed Assessment

**Code Quality**: A custom backend written by one engineer starts at approximately 60–70%
of LLVM O1 quality for typical code. With 1–2 years of optimization work, it can reach
80–90% of LLVM O1 quality for common patterns. It will never match LLVM O3 for compute-
intensive code without significant investment in auto-vectorization and global optimization.
For most TML programs (web services, tools, compilers), 80% of LLVM O1 is sufficient for
release builds.

**Compilation Speed**: A custom backend can be the fastest possible option because it
avoids any intermediate format and can be tightly integrated with TML's MIR representation.
Estimated per-file time: 5–15 ms, similar to Cranelift. The Go compiler achieves
exceptional build speeds with a custom backend — the entire Go standard library compiles
in 3–4 seconds on a laptop.

**Binary Size**: Zero additional static library dependency. The backend is part of the
compiler binary. Total size contribution: whatever code the TML backend itself is —
estimated 200–500 KB for a basic x86_64 backend.

**Debug Info**: Must be implemented from scratch. DWARF on Linux/macOS and PDB/CodeView
on Windows. Implementing PDB/CodeView correctly is approximately 4–6 months of work by
itself. The linker analysis already covers the PE/COFF object format; DWARF and PDB
sections can be generated using the same infrastructure.

**Target Support**: One target at a time. Start with x86_64 (Windows MSVC ABI). AArch64
(Apple Silicon) can be added in a subsequent phase. Unlike LLVM and Cranelift, there is no
"free" cross-target support — each new target requires dedicated engineering.

**Maturity**: A new custom backend starts immature. Correctness requires extensive testing
across all TML language features: closures, generics, SIMD, variadics, exceptions,
destructors, thread-local storage, and more. The TML test suite (1,659+ tests) provides a
regression baseline, but new failure modes specific to x86 encoding, ABI edge cases, and
exception table layout will appear during development.

**Integration Effort**: This is the largest investment of any option.

Phase 3a (Working but unoptimized backend): 6–9 months
- x86_64 instruction encoding library
- Register allocation (linear scan, then regalloc2-style)
- Basic MIR → x86_64 instruction selection
- COFF object file writer (shared with linker analysis work)
- All TML language constructs handled

Phase 3b (Optimization): 6–9 months
- Peephole optimization pass
- Simple inlining
- SIMD instruction selection for TML's vector operations
- Stack frame optimization (eliminate unnecessary spills)

Phase 3c (Production-ready): 3–6 months
- DWARF debug info
- PDB/CodeView debug info (Windows)
- Code coverage support
- Full fuzz testing and correctness verification

Total: 15–24 months for production quality.

**Enabling self-hosting**: A custom backend written in C++ can be ported to TML in
Phase 5 of the self-hosting roadmap. This completes the zero-dependency chain: TML
compiles itself using its own frontend, its own backend, and its own linker — no LLVM,
no Cranelift, no external tools.

**License**: N/A — TML-owned code.

### Prior Art

| Project | Approach | Time to Production Quality |
|---------|----------|--------------------------|
| Go compiler (`gc`) | Custom backend, x86/ARM/etc. | ~3 years to match C quality |
| V compiler | Custom C backend initially | ~1 year (via C transpilation) |
| Zig self-hosted | Custom x86_64 + AArch64 | ~2 years to partial parity |
| Lua 5.x | Register VM + JIT (LuaJIT separate) | Not directly comparable |
| Muon language | QBE backend (outsourced) | 3–6 months via QBE |

The Go compiler experience is most relevant: Andrew Gerrand estimated the custom backend
took ~3 years to reach GCC O1 equivalent quality, but the Go team had the advantage of
a simple type system (no generics initially) and a controlled runtime (goroutines).
TML's type system is more complex, but TML can reuse LLVM for release optimization
indefinitely — the custom backend only needs to match LLVM O0/O1 quality, not O3.

---

## Candidate 4: C as Intermediate Language

**Recommendation: VIABLE for bootstrapping only — not for production**

### Background

Generating C source code instead of LLVM IR is the oldest compiler trick for targeting
multiple platforms. Chicken Scheme, Nim (early versions), Cython, and early Haskell GHC
all used C as a portable assembly language. The TML compiler would emit `.c` files from
MIR and invoke an external C compiler (GCC, Clang, or TML's own `tml-cc`).

### Detailed Assessment

**Code Quality**: Depends entirely on the C compiler used. With GCC O3 or Clang O3,
output quality is comparable to LLVM O3. With GCC O0, output is similar to Cranelift.
The C intermediate typically introduces some overhead from the C abstraction model
(extra temporaries, constrained alias analysis), so it is rarely as good as a direct
LLVM IR path even with the same underlying compiler.

**Compilation Speed**: Total compilation time = TML's MIR→C generation time (~5 ms) +
C compiler time (~200–500 ms per file for GCC/Clang, or ~50 ms for TCC). This is slower
than LLVM direct compilation for any optimization level. It is faster than LLVM O3 only
when using a fast-but-low-quality C compiler (TCC, 8cc).

**Binary Size**: Zero backend dependency in the TML binary — but requires an external
C compiler installed on the system. This trades binary size for a system dependency,
which is the wrong trade for a self-contained toolchain.

**Integration Effort**: 3–5 months to implement a robust C emitter from TML's MIR.
Challenges include:
- Mapping TML's type system to C types (closures, ADTs, generics all need encoding)
- Handling TML-specific ABI conventions in C notation
- Ensuring no undefined behavior in generated C (signed overflow, pointer aliasing)
- Managing C reserved identifiers and name mangling

**License**: N/A — TML-owned code.

### Verdict

C transpilation is an excellent **bootstrapping technique**: when TML is first being
self-hosted, compiling TML→C→native via GCC is a valid way to produce a working
TML-written compiler before the custom native backend is ready. Nim used exactly this
approach. However, for TML's long-term production build pipeline, C transpilation is
inferior to Cranelift (slower, external dependency) and inferior to a custom backend
(slower, no control over output quality). It should be considered as a **temporary
bridge**, not a permanent backend.

---

## Candidate 5: libgccjit

**Recommendation: DO NOT ADOPT**

### Background

GCC exposes a JIT compilation interface as `libgccjit.so` — a shared library that allows
calling GCC's code generation pipeline from application code. It accepts a GCC IR
(not LLVM IR) and emits native code in memory.

### Detailed Assessment

**Code Quality**: GCC-quality output, comparable to LLVM O2–O3 for most code.

**Compilation Speed**: Similar to GCC batch compilation, which is generally slower than
LLVM for equivalent optimization levels. Not designed for fast incremental compilation.

**Binary Size**: `libgccjit.so` is a shared library — it cannot be statically linked. The
dependency model is fundamentally different from LLVM: instead of a self-contained DLL,
TML would require GCC to be installed on the host system. This is worse than the LLVM
dependency, not better.

**Target Support**: Whatever GCC targets are installed. Not portable in the way LLVM or
Cranelift are — target support depends on the host system's GCC build configuration.

**Platform Support**: Linux and macOS only. Windows support via MinGW exists but is
extremely non-standard. The MSVC ABI (TML's primary Windows target) is unsupported.

**License**: GCC Runtime Library Exception applies to the generated code, but `libgccjit`
itself is GPL-3.0. **GPL is incompatible with TML's distribution model.**

**Integration Effort**: 3–5 months plus ongoing compatibility maintenance as GCC versions
change.

### Verdict

The combination of GPL license, no Windows MSVC ABI support, shared-library-only model,
and no meaningful advantage over LLVM makes libgccjit a clear non-starter.

---

## Candidate 6: MIR Project (by Vladimir Makarov)

**Recommendation: MONITOR — too immature for adoption today**

### Background

MIR (Medium Internal Representation) is a lightweight JIT compilation framework written by
Vladimir Makarov (GCC developer) in ~40,000 lines of C. It defines its own IR format,
performs optimizations, and emits native code. It is used as the JIT backend in MRuby
(Ruby for embedded systems) and CRuby's experimental JIT compiler.

### Detailed Assessment

**Code Quality**: MIR includes a JIT optimizer and register allocator. Output quality is
approximately 70–80% of LLVM O1 for simple loops and function calls. For complex patterns
(closures, generics, dynamic dispatch), the quality gap with LLVM is wider.

**Compilation Speed**: Very fast — designed specifically for JIT compilation where low
latency matters more than optimization quality. Per-module time estimated at 3–10 ms.

**Binary Size**: Approximately 50 KB for the core MIR library. Extremely lightweight.

**Debug Info**: None. No DWARF or PDB support as of 2025.

**Target Support**: x86_64 (production quality), AArch64 (work in progress, unstable),
PowerPC (partial). Windows MSVC ABI: untested, likely non-functional.

**Maturity**: Experimental. MRuby uses MIR in production, but for a very constrained
workload (Ruby method bodies). Compiling TML's full language — generics, closures, ADTs,
SIMD — would push MIR into unexplored territory.

**Integration Effort**: MIR has no C++ API and no stable IR format. Would require writing
a TML MIR → MIR IR translator (the two IRs are different despite sharing a name). AArch64
instability makes it unsuitable for Apple Silicon builds.

### Verdict

MIR is technically interesting but not mature enough for TML's needs. Revisit in 12–18
months if AArch64 support stabilizes and Windows ABI support is added.

---

## Comparison Matrix

| Backend | Code Quality | Compile Speed | Binary Size | Debug Info | x86_64 | AArch64 | Windows | Effort | License |
|---------|-------------|--------------|-------------|-----------|--------|---------|---------|--------|---------|
| **LLVM (current)** | O3 ★★★★★ | ★★☆☆☆ | 78 MB | Full ★★★★★ | ✅ | ✅ | ✅ | N/A | Apache-2 ✅ |
| **Cranelift** | O1 ★★★★☆ | ★★★★☆ | ~5 MB | Partial ★★★☆☆ | ✅ | ✅ | ✅ | 2–4 mo | Apache-2 ✅ |
| **QBE** | O1 ★★★☆☆ | ★★★★★ | ~0.5 MB | Basic ★★☆☆☆ | ✅ | ✅ | ❌ | 4–7 mo | MIT ✅ |
| **Custom backend** | O1* ★★★☆☆ | ★★★★★ | ~0.5 MB | Custom ★★★★★* | ✅ | ★☆☆☆☆ | ✅ | 15–24 mo | N/A ✅ |
| **C transpilation** | O3* ★★★★★ | ★★☆☆☆ | 0 MB | Via CC ★★★☆☆ | ✅* | ✅* | ✅* | 3–5 mo | N/A ✅ |
| **libgccjit** | O3 ★★★★★ | ★★★☆☆ | Shared only | Full ★★★★★ | ✅ | ✅ | ❌ | 3–5 mo | GPL ❌ |
| **MIR Project** | O1- ★★★☆☆ | ★★★★★ | ~0.05 MB | None ★☆☆☆☆ | ✅ | ★★☆☆☆ | ❌ | 6–12 mo | MIT ✅ |

*Quality or target depends on external compiler or incomplete implementation

---

## Recommended Phased Strategy

### Phase 1 — Cranelift Development Backend (2–4 months)

Complete the existing Cranelift integration. Gate it as the default backend for debug
builds (`TML_BACKEND=cranelift`). LLVM remains available via `--backend=llvm` or the
`TML_BACKEND=llvm` environment variable.

**Result**: Developer iteration is 5–10x faster. `tml_codegen_cranelift.dll` replaces the
78 MB `tml_codegen_x86.dll` as the default loaded plugin. The /MD vs /MDd CRT mismatch
issue is eliminated for debug builds.

**Milestone criteria**:
- All existing TML test suite tests pass with Cranelift backend
- `tml build hello.tml` produces a working executable
- `tml build --release` still uses LLVM O3

### Phase 2 — Dual-Backend Build System (1–2 months)

Update `scripts/build.bat` and CMakeLists to make LLVM opt-in:
```
TML_ENABLE_LLVM=OFF    → build only Cranelift backend (fast, 5 MB DLL)
TML_ENABLE_LLVM=ON     → build both (CI, release, 78 MB LLVM DLL available)
```

The CMake backend selection at runtime:
```
tml build              → Cranelift (fast, default)
tml build --release    → LLVM O3 (if TML_ENABLE_LLVM=ON or llvm DLL present)
tml build --backend=llvm → LLVM explicitly
```

**Result**: Developer machines build 10x faster with no LLVM. CI continues to use LLVM
O3 for release artifacts. The binary size regression (78 MB DLL) is hidden behind an
opt-in flag.

### Phase 3 — Custom Native Backend in TML (15–24 months)

After the TML compiler is self-hosted (per the self-hosting analysis timeline), implement
a custom x86_64 backend in TML. This backend:

1. Accepts TML MIR directly (no LLVM IR text intermediate)
2. Performs linear-scan register allocation
3. Emits x86_64 machine code
4. Writes COFF/ELF object files (sharing code with the custom linker)
5. Emits DWARF debug info (Linux/macOS) and PDB/CodeView (Windows)

Starting the custom backend in TML (rather than C++) means it is immediately available
across all platforms when TML's own compiler builds it, without requiring a C++ toolchain.

**Result**: Zero external dependencies. A single `tml.exe` binary compiles TML programs
to native code on x86_64 and AArch64 without LLVM, Cranelift, GCC, or any other external
tool. LLVM remains optionally available (as a pre-built DLL) for `--release --opt=3`
builds where maximum optimization quality is required.

---

## Risk Assessment

| Phase | Risk | Severity | Mitigation |
|-------|------|----------|------------|
| Phase 1: Cranelift | ABI edge cases on Windows | MEDIUM | Run full test suite; Windows MSVC ABI is supported by Cranelift |
| Phase 1: Cranelift | LLVM IR importer missing features | MEDIUM | Fall back to MIR→Cranelift IR translator for missing constructs |
| Phase 1: Cranelift | Debug info gaps | LOW | Skip debug info for Phase 1; fall back to LLVM for debugging sessions |
| Phase 2: Dual backend | Build system complexity | LOW | CMake flag is straightforward; test matrix doubles but is mechanical |
| Phase 3: Custom backend | Correctness for edge cases | HIGH | Extensive fuzzing; use LLVM as oracle to compare outputs |
| Phase 3: Custom backend | Optimization quality gap | MEDIUM | Accept gap for dev builds; keep LLVM for release |
| Phase 3: Custom backend | Windows PDB implementation | HIGH | Use existing PDB research; reference LLVM's CodeView emitter |
| Phase 3: Custom backend | AArch64 support lag | MEDIUM | Implement x86_64 first; AArch64 follows 6–12 months later |

---

## Conclusion

The evidence strongly supports a phased approach:

1. **Cranelift immediately** — it is production-quality, already partially integrated,
   eliminates 73 MB from developer DLLs, and makes the iteration cycle 5–10x faster. The
   integration effort (2–4 months) is well-justified by the daily benefit to every engineer
   working on TML.

2. **LLVM kept optionally** — LLVM's optimization quality is irreplaceable for release
   builds. Keeping it as an opt-in backend costs nothing after Phase 1.

3. **Custom backend eventually** — the zero-dependency goal requires a custom backend, and
   the best time to implement it is after TML is self-hosted, so the backend itself can be
   written in TML. This is a 15–24 month investment with a clear payoff: a compiler that
   ships as a single binary with no runtime dependencies whatsoever.

The alternatives (QBE, libgccjit, MIR Project, C transpilation) are all viable in specific
contexts but inferior to the Cranelift → Custom Backend path for TML's specific constraints:
Windows-first, MSVC ABI, single-binary distribution, and long-term zero-dependency goal.
