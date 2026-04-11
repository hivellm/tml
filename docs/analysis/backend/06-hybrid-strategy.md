# 06 — Hybrid Backend Strategy: Phased LLVM Elimination

**Date**: 2026-04-05
**Status**: Complete
**Companion**: [02-alternative-backends.md](02-alternative-backends.md),
[05-cranelift-integration.md](05-cranelift-integration.md)

---

## Overview

This document defines the recommended strategy for eliminating TML's LLVM dependency. The
approach is not a single migration event but a **five-phase progression** where each phase
delivers standalone value and each subsequent phase builds on the previous.

The key insight from the analysis is that TML already has a clean architectural seam at
the `CodegenBackend` interface. Switching backends requires touching approximately 1,770
lines across 4 files — the rest of the 76,000-line codegen layer is already backend-agnostic.
This makes a phased approach practical: phases can be worked independently, partially
deployed, and rolled back without disrupting the rest of the compiler.

---

## Design Principle: Right Backend for Right Job

No single backend is optimal for every use case. The strategy assigns backends to use cases
based on what each use case actually needs.

| Use Case | Backend | Rationale |
|----------|---------|-----------|
| Dev builds (`tml build`) | Cranelift | 5-10x faster compile, ~85% code quality — acceptable for iteration |
| Release builds (`tml build --release`) | LLVM O3 | Maximum optimization; quality matters more than speed |
| Self-hosting (`tml` compiles `tml`) | Custom | Zero external dependencies; Cranelift requires Rust runtime |
| CI/CD pipeline | Cranelift or LLVM | Operator choice via `--backend=` flag |
| JIT (`tml run --jit`) | Cranelift | Sub-10ms startup; LLVM ORC startup overhead is 30-50ms |
| Cross-compilation | LLVM | Best multi-target support; Cranelift's ARM64 support lags |
| Benchmarks and profiling | LLVM O3 | Need maximum output quality to measure real hotspots |

The result is a model where **Cranelift becomes the default** (fast, good enough) and **LLVM
is an opt-in** (slow, best quality). Over time, a custom TML-written backend replaces
Cranelift for the self-hosting path, eventually making LLVM entirely optional even for
release builds if the custom backend reaches sufficient quality.

---

## Phase 1: Complete Cranelift Backend (2–4 months)

### Goal

Make Cranelift the default backend for all debug builds. LLVM remains available via an
explicit flag. All test suites pass.

### What Already Exists

`compiler/src/codegen/cranelift/cranelift_codegen_backend.cpp` (177 lines) implements:
- `BackendCapabilities` query (returns Cranelift feature flags)
- `compile_options` translation (opt level, target triple)
- Object writing infrastructure (COFF/ELF output via Cranelift's `ObjectModule`)

The `compile_mir()` body — MIR basic block translation to Cranelift CLIF — is the remaining
work.

### Work Breakdown

| Task | Effort | Notes |
|------|--------|-------|
| MIR → CLIF instruction translation | 3-5 weeks | One-to-one mapping for most instructions |
| Function signature ABI (sret, byval) | 1 week | Match LLVM's calling convention choices |
| Stack slot allocation (alloca equiv) | 3 days | Cranelift `StackSlot` API |
| Branch/terminator translation | 1 week | `br`, `brif`, `br_table` CLIF instructions |
| Phi nodes → Cranelift block params | 3 days | Cranelift uses explicit block parameters |
| External symbol references | 3 days | `FuncRef` / `GlobalValue` declarations |
| COMDAT / weak linkage | 2 days | Maps to Cranelift `Linkage` enum |
| Exception handling (unwind tables) | 2 weeks | .pdata section, RUNTIME_FUNCTION entries |
| Integration test: all test suites pass | 2 weeks | Fix edge cases as they appear |

**Total: approximately 9–13 weeks of focused work.**

### Build System Changes

```cmake
# New: default backend selection
if(TML_ENABLE_LLVM)
    set(TML_DEFAULT_BACKEND "llvm")
else()
    set(TML_DEFAULT_BACKEND "cranelift")
endif()

# New: separate DLL targets
add_library(tml_codegen_cranelift SHARED ...)    # ~5 MB
add_library(tml_codegen_x86 SHARED ...)          # ~78 MB  (still built, loaded on demand)
```

### CLI Changes

```
tml build foo.tml                  → Cranelift backend (fast, ~85% quality)
tml build foo.tml --release        → LLVM O3 (slow, maximum quality)
tml build foo.tml --backend=llvm   → LLVM O0 (explicit, for debugging codegen issues)
tml build foo.tml --backend=cranelift → Cranelift (explicit)
```

### Milestone Definition

Phase 1 is complete when:
1. All test suites in `lib/core/tests/` and `lib/std/tests/` pass with `--backend=cranelift`
2. The default build (no flags) uses Cranelift
3. `tml_codegen_cranelift.dll` is ≤ 6 MB on the release build
4. Per-file compile time (frontend + backend) is ≤ 120ms for a single-module program
5. `--backend=llvm` still works and passes the same test suite

### Binary Size After Phase 1

```
Before Phase 1:
  tml_codegen_x86.dll      78 MB   (LLVM + LLD, always loaded)
  tml_compiler.dll        104 MB   (compiler pipeline + LLVM glue)

After Phase 1:
  tml_codegen_cranelift.dll   ~5 MB   (default, always loaded)
  tml_codegen_x86.dll        78 MB   (optional, loaded only for --release or --backend=llvm)
  tml_compiler.dll           ~31 MB   (compiler pipeline, no LLVM glue in default path)
```

---

## Phase 2: LLVM as Optional Component (1–2 months)

### Goal

LLVM is no longer required to be present on disk for `tml build` to work. Users who never
pass `--release` never need to download the 78 MB LLVM plugin.

### Architecture

```
Distribution: "tml-minimal" (~20 MB)
  tml.exe                    15 MB   launcher + query system
  tml_compiler.dll           31 MB   compiler pipeline
  tml_codegen_cranelift.dll   5 MB   default backend
  lib/                        --     TML standard library source

Distribution: "tml-full" (~100 MB)
  Everything in tml-minimal, plus:
  tml_codegen_x86.dll        78 MB   LLVM O3 optional backend
```

### Plugin Loading Change

`compiler/src/codegen/backend_loader.cpp` currently hard-codes the DLL name. After Phase 2:

```cpp
auto load_backend(std::string_view name) -> std::unique_ptr<CodegenBackend> {
    // "cranelift" always available — bundled in minimal distribution
    if (name == "cranelift") return load_cranelift_plugin();

    // "llvm" optional — load only if DLL present
    if (name == "llvm") {
        auto path = find_plugin("tml_codegen_x86.dll");
        if (!path) {
            emit_error("LLVM backend not installed. Run: tml install --backend=llvm");
            return nullptr;
        }
        return load_llvm_plugin(*path);
    }

    emit_error("Unknown backend: " + std::string(name));
    return nullptr;
}
```

### Minimal Install Path

After Phase 2, a developer who only writes TML (never deploys optimized binaries) can
work with a 20 MB download. The LLVM plugin is an on-demand install:

```
tml install --backend=llvm     # downloads tml_codegen_x86.dll (~78 MB)
tml install --backend=all      # downloads all optional backends
```

### Milestone Definition

Phase 2 is complete when:
1. `tml build` runs to completion on a machine with no LLVM DLL present
2. `tml build --release` on the same machine prints a clear error directing the user to `tml install --backend=llvm`
3. The CI pipeline on a clean container produces a working binary using only the minimal distribution

---

## Phase 3: Custom x86_64 Backend in TML (12–18 months)

### Goal

Write a native x86_64 code generator in TML. This backend takes `mir::Module` as input and
produces COFF `.obj` bytes directly, with no Cranelift, no LLVM, no Rust runtime dependency.

### Why Write It in TML

1. **Self-hosting prerequisite**: The TML compiler cannot fully self-host while its codegen
   depends on a Rust library (Cranelift). Replacing Cranelift with a TML-written backend
   eliminates the last non-C-FFI runtime dependency.
2. **Correctness confidence**: A TML-written backend can be tested with TML's test framework,
   debugged with `mcp__tml__debug`, and profiled with `mcp__tml__profile`.
3. **No Rust toolchain**: Users building TML from source will not need Rust installed.

### Input: MIR Is Already Ideal

MIR provides exactly what a backend needs:
- SSA form (phi-free after `mem2reg` pass)
- Explicit basic blocks with typed terminators
- Resolved function signatures with ABI info
- Concrete types (no generics after monomorphization)
- Explicit allocas for stack variables

No additional lowering pass is needed between MIR and the custom backend.

### Development Milestones

| Sub-phase | Duration | Deliverable |
|-----------|----------|-------------|
| 3a: x86_64 instruction emitter | 2 months | `emit_mov`, `emit_add`, `emit_call`, etc. in TML |
| 3b: Register allocator (linear scan) | 2 months | Correct but not optimal allocation |
| 3c: MIR → instruction selection | 3 months | All MIR instruction types covered |
| 3d: Object file writer (COFF) | 1 month | Valid .obj output, passes `dumpbin` |
| 3e: Integration with compiler pipeline | 1 month | `CodegenBackend` interface in TML |
| 3f: Test suite pass at O0 | 2 months | Fix edge cases until all tests pass |
| 3g: Basic optimizations (peephole) | 2 months | Reach ~65% of LLVM O1 quality |

**Total: approximately 13–17 months for a production-ready O0 backend.**

### Code Quality Trajectory

The custom backend starts with zero optimization and improves over time:

| Milestone | Code Quality vs LLVM O3 | Achievable When |
|-----------|------------------------|-----------------|
| O0 (direct translation) | ~45-50% | Phase 3f complete |
| + Peephole opts | ~60-65% | Phase 3g complete |
| + Better register alloc | ~70-75% | 6 months after Phase 3 |
| + Instruction scheduling | ~78-82% | 12 months after Phase 3 |
| Mature custom backend | ~85-90% | 24 months after Phase 3 |

The custom backend does not need to match LLVM O3 — Cranelift already provides a better
development experience than LLVM O0, and LLVM O3 remains available for release builds
throughout this period.

### Self-Hosting Unlock

Once Phase 3 is complete, the TML compiler can compile its own backend:

```
Stage 0: C++ tml.exe (current) compiles custom_backend.tml → custom_backend.obj
Stage 1: C++ tml.exe with custom_backend.obj compiles tml.tml → tml_v2.exe
Stage 2: tml_v2.exe compiles tml.tml → tml_v3.exe  (self-hosting verification)
```

---

## Phase 4: Minimal C Compilation Path (6–12 months, parallel with Phase 3)

### Goal

Eliminate the dependency on Zig CC (currently the C compiler used to build the TML C runtime).
After Phase 4, no external C compiler is needed at any point in the TML toolchain.

### The C Runtime Problem

TML's C runtime is approximately 18,650 lines across ~30 files. Today, Zig CC (a Clang 20
wrapper) compiles these files as part of the CMake build. For a fully self-contained toolchain,
this dependency must be eliminated.

### Three Options

**Option A: Pre-compiled objects (shortest path)**
Compile the runtime once per target triple using any C compiler, and ship the resulting
`.obj`/`.o` files as part of the TML distribution. Users never need a C compiler.
- Effort: 2-4 weeks (build infrastructure + CI)
- Maintenance: re-compile on each C runtime change, keep binaries per target in source
- Limitation: users cannot customize the runtime without a C compiler

**Option B: Embedded TCC (Tiny C Compiler)**
Statically link TCC (~100 KB library) into `tml.exe`. When the runtime needs compilation,
use TCC's API to compile it in-process.
- Effort: 4-6 weeks
- TCC API: `tcc_new()`, `tcc_set_output_type()`, `tcc_compile_string()`, `tcc_output_file()`
- Limitation: TCC produces slow code (O0 equivalent), no optimizations
- License: LGPL 2.1 (acceptable if linked as a shared library)
- C standard: C99 + some C11 extensions

**Option C: Migrate all C runtime to TML (longest path)**
Rewrite the C runtime files that can be expressed in TML. After migration, only ~2,000 lines
of true OS-interface C remain (I/O, OpenSSL FFI, IOCP), which can be pre-compiled.
- Effort: 6-12 months (already in progress per ROADMAP)
- Produces the cleanest long-term result
- Aligns with the C-to-TML migration roadmap

### Recommended Approach

Run all three options in parallel at different scales:
1. **Immediately**: Ship pre-compiled objects for Windows x64 and Linux x64 (Option A)
2. **Within 2 months**: Embed TCC as a fallback for other targets (Option B)
3. **Ongoing**: Continue C-to-TML migration (Option C)

After Option C reduces the C residue to ~2,000 lines, Option A covers it permanently with
a ~50 KB pre-compiled bundle.

### Milestone Definition

Phase 4 is complete when:
1. A fresh `tml build hello.tml` on a machine with no Zig/Clang/GCC/MSVC produces a working
   binary
2. The runtime pre-compiled objects or TCC fallback covers all supported targets
3. The CMake build no longer fails if no C compiler is found on the PATH

---

## Phase 5: Self-Contained Toolchain (3–6 months)

### Goal

Bundle the custom linker (from `docs/analyses/linker/` analysis), compiler, and backend into
a single distribution that needs no external tools to produce working executables.

### Components

After Phases 1–4, the remaining external dependencies are:
- The linker (LLD, still required for linking even with Cranelift backend)
- The Windows SDK import libraries (for linking against Win32 APIs)

Phase 5 addresses both:

**Custom linker**: Already designed in `docs/analyses/linker/04-custom-linker-design.md`.
Phase 3 of the linker analysis (tml-link) eliminates LLD. The linker can be implemented
in C++ first and later rewritten in TML.

**Windows SDK stubs**: Generate import library stubs from the Win32 metadata (`.winmd` files,
MIT-licensed, available from the `windows-rs` project). These stubs allow linking against
`kernel32.dll`, `ntdll.dll`, etc. without MSVC's `kernel32.lib`.

### Final Distribution

```
tml-1.0-windows-x64.zip   (~22 MB total)
├── tml.exe                 (15 MB — compiler + Cranelift + tml-link)
├── tml_codegen_x86.dll     (optional download — LLVM O3, +78 MB)
├── lib/
│   ├── core/src/           (TML core library source)
│   ├── std/src/            (TML std library source)
│   └── runtime/
│       ├── windows-x64/    (pre-compiled runtime objects, ~800 KB)
│       └── linux-x64/      (pre-compiled runtime objects, ~650 KB)
└── include/
    └── win32/              (Win32 import stubs, ~200 KB)
```

### Milestone Definition

Phase 5 is complete when:
1. A user downloads the ZIP, unzips it, and runs `tml build hello.tml` with no other
   software installed (other than the OS itself)
2. The resulting `hello.exe` runs correctly
3. Cross-compilation works: `tml build hello.tml --target=linux-x64` on Windows produces
   a Linux ELF binary

---

## Timeline Overview

```
Month  0-4:   Phase 1 — Cranelift backend, dev default
               ├── Weeks  1-9:  MIR → CLIF translation
               ├── Weeks 10-12: ABI + exception tables
               └── Weeks 13-16: Integration tests, polish

Month  4-6:   Phase 2 — LLVM as optional plugin
               ├── Week 1-3:  Backend loader changes
               ├── Week 4-6:  Distribution packaging
               └── Week 7-8:  CI pipeline updates

Month  6-24:  Phase 3 — Custom x86_64 backend in TML (parallel with other phases)
               ├── Months  6-8:  Instruction emitter + register allocator
               ├── Months  8-12: MIR → instruction selection
               ├── Months 12-14: COFF object writer
               ├── Months 14-16: Integration + test suite pass
               └── Months 16-24: Optimizations

Month  6-18:  Phase 4 — Minimal C compilation (parallel with Phase 3)
               ├── Month  6:  Ship pre-compiled objects (Option A)
               ├── Month  8:  Embed TCC fallback (Option B)
               └── Ongoing:   C-to-TML migration (Option C)

Month 24-30:  Phase 5 — Self-contained toolchain
               ├── Month 24-26: Custom linker integration
               ├── Month 26-28: Win32 stub generation
               └── Month 28-30: Cross-compilation + packaging
```

---

## Decision Points

At each phase boundary, evaluate whether to continue:

### After Phase 1
- Are Cranelift dev builds fast enough? (target: ≤120ms single file)
- Does Cranelift code quality suffice for all dev workflows?
- Is LLVM plugin load-on-demand working reliably?

If Phase 1 quality is insufficient, consider Cranelift + a lightweight inliner before
moving to Phase 2.

### After Phase 2
- Is the minimal distribution (no LLVM) actually being used?
- Has the Cranelift backend proven stable over 2+ months of real use?
- Is Phase 3 effort justified given Cranelift meets dev needs?

If Cranelift fully satisfies dev workflows and LLVM O3 remains for release, Phase 3 may
be deferred until after self-hosting (Phase 5 of the self-hosting analysis). The custom
backend only becomes critical on the path to a Rust-free distribution.

### After Phase 3
- Does the custom backend reach ≥65% of LLVM O1 quality?
- Is the code size of the custom backend reasonable (target: <5 MB)?
- Is self-hosting now viable (custom backend compiles itself)?

If quality is below 65%, continue optimization work before deprecating Cranelift. Both
can coexist indefinitely — Cranelift for developers who prioritize speed, custom backend
for the self-hosted distribution.

---

## Relationship to Self-Hosting Analysis

The self-hosting analysis (`docs/analyses/compiler-selfhosting/`) defines which parts of
the compiler must be rewritten in TML. Backends are the last piece:

```
Self-hosting analysis scope:  Lexer → Parser → Types → HIR → MIR  (Phases 1-5 complete)
Backend analysis scope:       MIR → .obj → .exe                     (this document)
```

Phase 3 of this analysis (custom backend in TML) is the **enabling condition** for full
self-hosting. Without it, the TML compiler written in TML must still call a Rust library
(Cranelift) or a C++ library (LLVM) to produce machine code.

```
Self-hosting milestone A:  TML compiler in TML (no codegen) — 24-30 months
Backend Phase 3 milestone:  Custom backend in TML              — 12-18 months (parallel)
Self-hosting milestone B:  TML compiler in TML with TML backend — Phases complete together
```

The timelines are designed to arrive at the same point simultaneously.
