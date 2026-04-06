# Milestone Matrix & Independence Scorecard

**Date**: 2026-04-05
**Purpose**: Track progress toward full TML toolchain independence
**Updates**: This document should be updated as milestones are achieved

---

## 1. Independence Scorecard

At each milestone, TML's independence level changes. This scorecard tracks what % of the toolchain is self-owned.

### Current State (April 2026)

| Component | Owner | Status | Independence |
|-----------|-------|--------|-------------|
| TML Compiler Frontend | C++ | Active | 0% (C++ codebase) |
| TML Type Checker | C++ | Active | 0% |
| TML IR Pipeline (HIR/THIR/MIR) | C++ | Active | 0% |
| TML Codegen (MIR → LLVM IR) | C++ | Active | 0% |
| LLVM Backend (IR → .obj) | LLVM Project | Dependency | 0% |
| Linker (.obj → .exe) | LLD (LLVM) | Dependency | 0% |
| C Runtime | C (18,650 LOC) | FFI Bridge | 0% |
| C/C++ Compilation | MSVC/Clang/GCC | External | 0% |
| **Overall Independence** | | | **0%** |

### After Era 1: Self-Hosted (Month 30)

| Component | Owner | Status | Independence |
|-----------|-------|--------|-------------|
| TML Compiler Frontend | **TML** | Self-hosted | **100%** |
| TML Type Checker | **TML** | Self-hosted | **100%** |
| TML IR Pipeline | **TML** | Self-hosted | **100%** |
| TML Codegen | **TML** | Self-hosted | **100%** |
| LLVM Backend | LLVM | Dependency | 0% |
| Linker | LLD | Dependency | 0% |
| C Runtime | C (14,464 LOC) | FFI Bridge | 23% migrated |
| C/C++ Compilation | External | External | 0% |
| **Overall Independence** | | | **50%** |

### After Era 2: Custom Backend (Month 48)

| Component | Owner | Status | Independence |
|-----------|-------|--------|-------------|
| TML Compiler Frontend | TML | Self-hosted | 100% |
| TML Type Checker | TML | Self-hosted | 100% |
| TML IR Pipeline | TML | Self-hosted | 100% |
| TML Codegen | TML | Self-hosted | 100% |
| Native Backend | **TML** | Custom | **100%** |
| Linker | LLD | Dependency | 0% |
| C Runtime | C (~12K LOC) | FFI Bridge | 35% migrated |
| C/C++ Compilation | External | External | 0% |
| **Overall Independence** | | | **70%** |

### After Era 3: Custom Linker (Month 48)

| Component | Owner | Status | Independence |
|-----------|-------|--------|-------------|
| TML Compiler | TML | Self-hosted | 100% |
| Backend | TML | Custom | 100% |
| Linker | **TML** | Custom (tml-link) | **100%** |
| C Runtime | C (~10K LOC) | FFI Bridge | 45% migrated |
| C/C++ Compilation | External | External | 0% |
| **Overall Independence** | | | **85%** |

### After Era 4: Full Independence (Month 60)

| Component | Owner | Status | Independence |
|-----------|-------|--------|-------------|
| TML Compiler | TML | Self-hosted | 100% |
| Backend | TML | Custom | 100% |
| Linker | TML | Custom | 100% |
| C/C++ Frontend | **TML** | Custom | **100%** |
| C Runtime | C (~8K LOC) | Essential FFI | 57% migrated |
| **Overall Independence** | | | **95%** |

*Note: 100% independence is never fully achievable — OS syscalls always require a thin FFI shim. 95% means TML owns every layer of the compilation stack; only OS interface code remains in C.*

---

## 2. Milestone Registry

### Era 1 Milestones

| ID | Milestone | Month | Dependencies | LOC | Eliminates | Binary Impact |
|----|-----------|-------|-------------|-----|-----------|--------------|
| M-01 | Phase 7 complete (Rust parity) | 0 | — | — | — | — |
| M-02 | MIR paths consolidated | 2 | M-01 | -5K C++ | Dual MIR builders | — |
| M-03 | IR-diff tool operational | 3 | M-01 | +500 TML | — | — |
| M-04 | Type checker invariants documented | 4 | M-01 | +100 pages | — | — |
| M-05 | TML lexer passes all tests | 6 | M-02, M-03 | +1,800 TML | C++ lexer | — |
| M-06 | TML parser passes all tests | 8 | M-05 | +4,100 TML | C++ parser | — |
| M-07 | TML type checker passes all tests | 16 | M-06, M-04 | +13,600 TML | C++ type checker | — |
| M-08 | TML MIR pipeline passes all tests | 22 | M-07 | +26,250 TML | C++ HIR/THIR/MIR | — |
| M-09 | TML codegen passes all tests | 26 | M-08 | +49,600 TML | C++ codegen | — |
| M-10 | **SELF-HOSTING: Stage 2 verified** | 30 | M-09 | +9,970 TML | C++ compiler | -184K C++ |

### Era 2 Milestones

| ID | Milestone | Month | Dependencies | LOC | Eliminates | Binary Impact |
|----|-----------|-------|-------------|-----|-----------|--------------|
| M-11 | Debug backend produces working binary | 30 | M-08 | +8,000 TML | — | — |
| M-12 | Register allocator operational | 36 | M-11 | +6,000 TML | — | — |
| M-13 | AArch64 support added | 40 | M-12 | +4,000 TML | — | — |
| M-14 | Production backend (x86_64 + AArch64) | 42 | M-12, M-13 | +12,500 TML | LLVM (optional) | -80 MB |
| M-15 | Debug info (PDB + DWARF) complete | 48 | M-14 | +8,500 TML | **LLVM eliminated** | **-100 MB** |

### Era 3 Milestones

| ID | Milestone | Month | Dependencies | LOC | Eliminates | Binary Impact |
|----|-----------|-------|-------------|-----|-----------|--------------|
| M-16 | PE/COFF linker passes tests | 36 | M-08 | +8,500 TML | — | — |
| M-17 | ELF linker passes tests | 40 | M-16 | +7,000 TML | — | — |
| M-18 | Mach-O linker passes tests | 44 | M-17 | +4,500 TML | — | — |
| M-19 | Incremental linker (< 10ms) | 48 | M-18 | +5,000 TML | **LLD eliminated** | -20 MB |

### Era 4 Milestones

| ID | Milestone | Month | Dependencies | LOC | Eliminates | Binary Impact |
|----|-----------|-------|-------------|-----|-----------|--------------|
| M-20 | C preprocessor complete | 40 | M-08 | +3,800 TML | — | +0.5 MB |
| M-21 | `tml cc` compiles simple C programs | 46 | M-20, M-14 | +16,200 TML | GCC/Clang for C | +1.5 MB |
| M-22 | `tml c++` compiles C++ subset | 54 | M-21 | +35,000 TML | MSVC for C++ | +2 MB |
| M-23 | **FULL INDEPENDENCE** | 60 | M-15, M-19, M-22 | — | All external compilers | **~15 MB total** |

---

## 3. Binary Size Progression

| Milestone | Binary Size | Change | What Changed |
|-----------|-----------|--------|-------------|
| Today | ~140 MB | — | C++ compiler + LLVM + LLD |
| M-10 (Self-hosted) | ~140 MB | — | TML compiler, still uses LLVM/LLD |
| M-14 (Custom backend) | ~60 MB | -80 MB | LLVM optional, custom backend default |
| M-15 (Debug info) | ~40 MB | -20 MB | LLVM fully eliminated |
| M-19 (Custom linker) | ~20 MB | -20 MB | LLD eliminated |
| M-21 (C frontend) | ~22 MB | +2 MB | C compiler added |
| M-23 (Full independence) | ~15 MB | -7 MB | Optimized, trimmed |

---

## 4. Go/No-Go Criteria

### Era 1 → Era 2 Transition (Month ~24)

| Criterion | Required State | Current |
|-----------|---------------|---------|
| Self-hosted compiler passes 100% of tests | Yes | Not started |
| Stage 2 bootstrap verified | Yes | Not started |
| MIR is stable (no breaking changes in 3 months) | Yes | Active development |
| TML stdlib covers backend needs (bitops, encoding) | Yes | ~95% ready |

**Go if**: All criteria met. Self-hosting is stable and MIR format is frozen.
**No-Go if**: Self-hosting not yet complete or MIR still changing.

### Era 2 → Era 3 Transition (Month ~30)

| Criterion | Required State |
|-----------|---------------|
| Debug backend produces correct code for all tests | Yes |
| Register allocator handles all MIR instruction types | Yes |
| x86_64 encoding covers 80%+ of instruction set | Yes |

**Go if**: Backend can compile TML stdlib without falling back to LLVM.
**No-Go if**: Backend still needs LLVM fallback for common patterns.

### Era 3 → Era 4 Transition (Month ~36)

| Criterion | Required State |
|-----------|---------------|
| Custom linker handles all TML test suite outputs | Yes |
| Incremental linking demonstrates < 100ms (not yet < 10ms) | Yes |
| PE, ELF, or Mach-O format (at least one) fully working | Yes |

### Full Independence Declaration (Month ~60)

| Criterion | Required State |
|-----------|---------------|
| `tml build` works without LLVM, LLD, or Clang installed | Yes |
| `tml cc` passes C compilation test suite (e.g., csmith) | Yes |
| `tml c++` compiles the 1,593 LOC C++ shim (bootstrap) | Yes |
| Binary size < 20 MB | Yes |
| Cross-compilation for x86_64 + AArch64 works | Yes |

---

## 5. Dependency Graph (Visual)

```
                    ┌─────────────────────────────────────────────────┐
                    │              FULL INDEPENDENCE (M-23)            │
                    │  tml build .tml/.c/.cpp → native binary         │
                    │  Binary: ~15 MB, zero external deps             │
                    └───────────┬──────────┬──────────┬───────────────┘
                                │          │          │
                    ┌───────────▼──┐  ┌────▼────┐  ┌─▼──────────────┐
                    │ C++ Frontend │  │ Custom  │  │ Custom Linker  │
                    │ (M-22)       │  │ Backend │  │ (M-19)         │
                    │ 35K LOC      │  │ (M-15)  │  │ 25K LOC        │
                    └───────┬──────┘  │ 39K LOC │  └────┬───────────┘
                            │         └────┬────┘       │
                    ┌───────▼──────┐       │      ┌─────▼───────────┐
                    │ C Frontend   │       │      │ PE/ELF/Mach-O   │
                    │ (M-21)       │       │      │ (M-16,17,18)    │
                    │ 20K LOC      │       │      └─────────────────┘
                    └───────┬──────┘       │
                            │         ┌────▼────────┐
                            │         │ Reg Alloc   │
                            │         │ (M-12)      │
                            │         └────┬────────┘
                            │              │
                    ┌───────▼──────────────▼───────────────────────────┐
                    │           SELF-HOSTED COMPILER (M-10)            │
                    │  TML compiler written in TML, ~120K LOC          │
                    │  Stage 2 bootstrap verified                      │
                    └──────────────────────┬──────────────────────────┘
                                           │
                    ┌──────────────────────▼──────────────────────────┐
                    │            FOUNDATION (M-01 through M-04)        │
                    │  Phase 7 done, MIR consolidated, IR-diff ready   │
                    └──────────────────────────────────────────────────┘
```

---

## 6. Total Investment Summary

| Era | New TML LOC | Duration | Developers | Eliminates |
|-----|-------------|----------|-----------|-----------|
| Era 1: Self-Hosting | ~120,000 | 24–30 months | 1–2 | 184K C++ |
| Era 2: Custom Backend | ~39,000 | 15–22 months | 1 | LLVM (~100 MB) |
| Era 3: Custom Linker | ~25,000 | 6–18 months | 1 | LLD (~20 MB) |
| Era 4: C/C++ Frontend | ~55,000 | 12–24 months | 1–2 | Clang/GCC/MSVC |
| **Total** | **~239,000** | **48–60 months** | — | **All external deps** |

**With 2 developers working in parallel (Eras 2+3 overlap)**: 36–48 months total
**With 3 developers**: 30–40 months total

The result: a ~15 MB binary that compiles TML, C, and C++ to native x86_64 and AArch64 code, with zero external dependencies.
