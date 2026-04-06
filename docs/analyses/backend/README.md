# TML Backend Analysis — Eliminating the LLVM Dependency

**Date**: 2026-04-05
**Status**: Complete — 2 analysis documents + index
**Scope**: Full analysis of LLVM dependency cost, alternative backends, and a phased
strategy toward a self-contained TML toolchain with no external codegen dependency.
**Total**: ~600+ lines of analysis across 3 files

---

## Purpose

The TML compiler currently depends on LLVM for native code generation. LLVM is statically
linked into `tml_codegen_x86.dll`, contributing approximately **78 MB** to the binary, adding
**35–580 ms** of per-file compilation latency depending on optimization level, and representing
a permanent C++ dependency that prevents the compiler from becoming fully self-hosted.

This analysis investigates three concrete goals:

1. **Eliminate LLVM for development builds** — faster iteration, smaller binaries, no
   /MD vs /MDd CRT mismatch issues during debug compilation.
2. **Keep LLVM optional for release builds** — LLVM's optimization quality is unmatched
   and should remain available when maximum performance output is required.
3. **Enable a native C compilation path** — long-term, TML should be able to compile its
   own C runtime files without any external compiler or library.

---

## Key Finding: The Decoupling Point Already Exists

TML's MIR Codegen layer generates LLVM IR as **pure text via `std::stringstream`**. No LLVM
builder APIs are used — not `IRBuilder`, not `LLVMBuildXXX`, not any LLVM in-memory
representation. The IR text string is then handed to `llvm_backend.cpp` which parses it back
into an LLVM `Module` using `LLVMParseIRInContext`.

This architecture means:

- The 40+ codegen files in `compiler/src/codegen/` are completely LLVM-agnostic.
- The **only coupling point** is `llvm_backend.cpp` (550 lines) and the `LLVMCodegenBackend`
  wrapper (158 lines).
- Replacing LLVM requires implementing one 2-method C++ interface: `CodegenBackend`.
- A new backend can accept the same IR text string and feed it to Cranelift, QBE, or a
  custom emitter — without touching any codegen files.

The `CodegenBackend` abstraction already exists. `CraneliftCodegenBackend` is already
partially implemented at `compiler/src/codegen/cranelift/cranelift_codegen_backend.cpp`.

---

## Recommended Strategy: Three Phases

```
Phase 1 — Cranelift dev backend (2–4 months)
  Replace LLVM for debug/dev builds with Cranelift.
  Result: 5-10x faster compilation, 73MB smaller DLL, no /MD mismatch.
  LLVM remains available via --backend=llvm for release optimization.

Phase 2 — Dual-backend build system (1–2 months)
  Gate LLVM behind TML_ENABLE_LLVM=ON (off by default for dev).
  CI uses LLVM for release; developer machines use Cranelift.
  Result: 10-second cold builds, LLVM O3 still available for release.

Phase 3 — Custom native backend in TML (12–18 months)
  Implement a TML-written MIR-to-x86_64 backend.
  No Cranelift, no LLVM, no Rust runtime dependency.
  Result: true zero-dependency self-hosting.
```

---

## Document Index

| # | File | Description |
|---|------|-------------|
| 1 | [01-current-llvm-dependency.md](01-current-llvm-dependency.md) | Complete inventory of LLVM API usage, library list, binary cost, and what must be replaced |
| 2 | [02-alternative-backends.md](02-alternative-backends.md) | Analysis of Cranelift, QBE, custom backend, C transpilation, libgccjit, and MIR Project |

---

## Decision Matrix

| Use Case | Recommended Backend | Reason |
|----------|-------------------|--------|
| Developer iteration (debug build) | Cranelift | 5-10x faster, 73MB smaller |
| CI release build | LLVM O3 | Maximum optimization quality |
| Embedded / single-file distribution | Custom (Phase 3) | Zero external deps |
| Cross-compilation (ARM from x86) | LLVM | Best cross-target support |
| JIT / `tml run` | LLVM ORC | Only production-quality JIT option |
| Self-hosting bootstrap | Cranelift → Custom | Progressively eliminate all deps |

---

## Binary Size Impact

```
Current (LLVM statically linked):
  tml_codegen_x86.dll     78 MB   (LLVM + LLD)
  tml_compiler.dll       104 MB   (all compiler code)

After Phase 1 (Cranelift default):
  tml_codegen_cranelift.dll   ~5 MB   (Cranelift statically linked)
  tml_compiler.dll           ~31 MB   (compiler without LLVM)
  tml_codegen_x86.dll        78 MB   (still available, loaded on demand)

After Phase 3 (custom backend):
  tml.exe (monolithic)       ~8–12 MB   (compiler + custom backend, no LLVM)
  tml_codegen_x86.dll        78 MB   (optional, for optimized release builds)
```

---

## Compilation Speed Impact

| Stage | LLVM O0 | LLVM O3 | Cranelift | Custom (est.) |
|-------|---------|---------|-----------|---------------|
| IR parsing | 5–10 ms | 5–10 ms | N/A | N/A |
| Optimization | 10–20 ms | 100–500 ms | 3–8 ms | 2–5 ms |
| Code emission | 20–50 ms | 20–50 ms | 10–25 ms | 5–15 ms |
| **Total per file** | **35–80 ms** | **125–560 ms** | **13–33 ms** | **7–20 ms** |
| Speedup vs LLVM O0 | 1x | 0.3x | 3–5x | 5–10x |

---

## Relationship to Self-Hosting Analysis

The self-hosting analysis (`docs/analyses/compiler-selfhosting/`) identifies LLVM as a
"Layer 4: Keep Permanently" dependency — a C++ wrapper that the TML-written compiler will
call through the same IR-text interface. That analysis is correct for the near term.

This backend analysis extends beyond self-hosting to address the **zero-dependency** goal:
a TML compiler that ships as a single binary with no LLVM, no Cranelift, no external
linker. Phase 3 (custom backend) achieves this at the cost of 12–18 months of engineering.

The two analyses are complementary:

```
Self-hosting analysis answer:  "Can TML be written in TML?"         → YES (24–30 months)
Backend analysis answer:       "Can TML compile without LLVM?"      → YES (2–4 months partial,
                                                                           18+ months full)
```

---

## Timeline Overview

| Phase | Duration | Milestone |
|-------|----------|-----------|
| Phase 1: Cranelift dev backend | 2–4 months | Dev builds 5x faster, 73MB smaller |
| Phase 2: Dual-backend CI | 1–2 months | LLVM opt-in, Cranelift default |
| Phase 3a: Custom x86_64 backend | 6–9 months | Working but unoptimized native backend |
| Phase 3b: Custom backend optimizations | 6–9 months | 80%+ of LLVM O1 quality |
| Phase 3c: Custom backend production-ready | 3–6 months | Debug info, all edge cases |

Phases 1 and 2 are **independent** of the self-hosting timeline and can start immediately.
Phase 3 should begin after Phase 5 of self-hosting (query + CLI complete), so the backend
can be written in TML rather than C++.
