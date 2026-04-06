# Proposal: phase20a — Extended x86_64 Production Backend

## Why

Phase 18 + 19 produce a functional native backend for integer programs with register allocation. Phase 20a extends this to a production-quality backend: floating point (SSE2), SIMD vectors (SSE4.2/AVX2), atomics (for the concurrent runtime), and classic compiler optimizations (peephole, constant propagation). After phase 20a, the native backend becomes the default on Windows x86_64.

The motivation for native over LLVM is: (1) elimination of the 500MB LLVM binary dependency, (2) faster compilation — no LLVM IR text → LLVM IR parse → LLVM optimization passes → MC layer round-trip, (3) full control over code generation strategy without fighting LLVM's opinionated optimization model.

## What Changes

- Extended `compiler/native/x86_encode.tml` — SSE2, SSE4.2, AVX2, and atomic instruction encoding
- New TML module `compiler/native/cpuid.tml` — CPUID feature detection (SSE2/SSE4.1/SSE4.2/AVX2 flags)
- New TML module `compiler/native/peephole.tml` — peephole optimization pass over MachInst list
- New TML module `compiler/native/const_prop.tml` — constant propagation at MachIR level
- CLI default changed: `--backend=native` on Windows x86_64

## Design Decisions

**CPUID at startup, not compile time**: Feature flags (SSE4.2, AVX2) are detected at program startup via CPUID, stored in thread-local flags, and checked before emitting extended instructions. This allows the same binary to run on older x86_64 CPUs without AVX2. Alternative: compile-time specialization with multiple code paths — deferred to phase 20a+.

**Peephole after register allocation**: Peephole runs on the MachInst list after register assignment. This is the correct position: before allocation, the operands are virtual and patterns (like redundant MOVs) are not visible. After allocation, physical register identities are known and the optimizer can see actual redundancies.

**SSE2 baseline (not AVX-only)**: Every x86_64 CPU supports SSE2 (it is part of the x86_64 spec). SSE4.2 and AVX2 are enabled only when CPUID confirms support. Float arithmetic always uses SSE2 (scalar ADDSD/ADDSS), never x87.

**Benchmark target (2x of LLVM -O2)**: This is realistic for an unoptimized native backend with peephole only. LLVM -O2 applies dozens of IR-level passes (LICM, GVN, SLP vectorization, inlining, etc.) that a phase 20a backend does not yet implement. The goal is not to beat LLVM -O2 — it is to produce code fast enough that users prefer the faster compile time of the native backend.

## Impact

- Affected specs: docs/specs/native-backend.md (SSE/AVX encoding, peephole patterns, cpuid section)
- Affected code: compiler/native/ (extended), compiler/src/cli/ (default backend change)
- Breaking change: YES (behavior change) — `--backend=native` becomes default on Windows. `--backend=llvm` remains available for opt-out. All existing tests must pass.
- User benefit: TML compiler no longer requires LLVM on Windows for typical programs; compile times drop significantly; floating point and SIMD work in native-compiled code

## Risk

MEDIUM-HIGH. SSE encoding involves VEX prefixes (for AVX), which are distinct from REX and interact poorly with legacy SSE. CPUID detection must be correct — a false positive for AVX2 on an old CPU causes illegal instruction crashes. The benchmark requirement (task 6.1-6.3) is the primary quality gate; if the native backend is more than 2x slower than LLVM -O2, it is not production-ready.

## Reference

- Intel SDM Vol 2C — SSE/SSE4.2/AVX2 instruction encoding tables
- Intel Intrinsics Guide — human-readable SSE/AVX reference
- CPUID specification: Intel SDM Vol 2A, CPUID instruction §
- Peephole patterns: Engineering a Compiler (Cooper & Torczon) §11.1
- AMD64 ABI §10 (SSE calling convention) — XMM register usage rules
