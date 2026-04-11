# TML Full Independence Plan

**Date**: 2026-04-05
**Status**: Strategic planning — master plan for complete toolchain independence
**Scope**: Self-hosted compiler + custom backend + custom linker + C/C++ compiler
**Horizon**: 4–6 years for full independence (many phases parallel)

---

## Vision

TML becomes a **complete, self-sufficient toolchain** that can:

```
tml build hello.tml       → compiles TML to native x86_64/AArch64 binary
tml build main.c          → compiles C to native binary (like zig cc)
tml build main.cpp        → compiles C++ to native binary
tml build project/        → builds mixed TML/C/C++ projects
tml test                  → runs all tests with zero external dependencies
```

**Zero dependencies on LLVM, LLD, Clang, MSVC, or GCC.** TML owns every layer of the compilation stack.

---

## Four Pillars of Independence

| Pillar | Description | Eliminates | Timeline |
|--------|-------------|-----------|----------|
| **1. Self-Hosted Compiler** | TML compiler written in TML | C++ compiler codebase (184K LOC) | 24–30 months |
| **2. Custom Backend** | MIR → native x86_64/AArch64 | LLVM dependency (100MB+) | 15–22 months |
| **3. Custom Linker** | tml-link: PE, ELF, Mach-O | LLD dependency | 6–18 months |
| **4. C/C++ Frontend** | Parse C/C++ → MIR → native | Clang/GCC/MSVC dependency | 12–24 months |

**Current state**: 1,593 LOC C++ is the permanent LLVM/LLD boundary. After full independence: **0 LOC C++ required**.

---

## Document Index

| # | File | Lines | Description |
|---|------|-------|-------------|
| 0 | README.md | — | This file — vision, index, key numbers |
| 1 | [01-vision-and-architecture.md](01-vision-and-architecture.md) | ~400 | Architecture layers, pillar details, comparison with Zig/Go/Rust |
| 2 | [02-phased-roadmap.md](02-phased-roadmap.md) | ~600 | 4 eras, 17 phases, Gantt timeline, dependency graph |
| 3 | [03-c-cpp-compiler-strategy.md](03-c-cpp-compiler-strategy.md) | ~400 | How TML becomes a C/C++ compiler — 4 strategy options |
| 4 | [04-milestone-matrix.md](04-milestone-matrix.md) | ~300 | Independence scorecard, milestone dependencies, go/no-go |

---

## Key Numbers

| Metric | Value | Source |
|--------|-------|--------|
| Current compiler C++ | 184K LOC | [compiler-selfhosting/01](../compiler-selfhosting/01-compiler-inventory.md) |
| Estimated TML compiler | ~120K LOC | 35% reduction via language expressiveness |
| Permanent C++ today | 1,593 LOC | LLVM backend (550) + LLD (670) + JIT (373) |
| After full independence | **0 LOC C++** | Custom backend + linker replace all |
| TML stdlib maturity | 535 files, 141K LOC | Covers 95%+ of compiler needs |
| Custom backend estimate | 30–50K LOC TML | x86_64 + AArch64 |
| Custom linker estimate | 15–25K LOC TML | PE/COFF + ELF + Mach-O |
| C frontend estimate | 16–25K LOC TML | Preprocessor + parser + type checker |
| C++ frontend estimate | 30–50K LOC TML | Subset (structs, classes, templates) |
| Binary size today | ~140 MB | LLVM + LLD statically linked |
| Binary size after independence | ~10–15 MB | Custom backend + linker |
| Total new TML code | ~210–270K LOC | All pillars combined |

---

## Timeline Overview

```
2026       2027       2028       2029       2030       2031
  │          │          │          │          │          │
  ├──Era 1: Self-Hosting────────────┤
  │  (24-30 months)                 │
  │                    ├──Era 2: Custom Backend──────────┤
  │                    │  (15-22 months)                 │
  │                          ├──Era 3: Custom Linker─────┤
  │                          │  (6-18 months)            │
  │                                ├──Era 4: C/C++ Frontend──────┤
  │                                │  (12-24 months)             │
  │                                                              │
  ◆ Phase 7 done                   ◆ Self-hosted    ◆ LLVM-free  ◆ Full Independence
  (Apr 2026)                       (Dec 2027)       (2029)        (2031)
```

---

## Reading Guide

| You want to... | Read... |
|----------------|---------|
| Understand the big picture | [01-vision-and-architecture.md](01-vision-and-architecture.md) |
| See the detailed phase plan | [02-phased-roadmap.md](02-phased-roadmap.md) |
| Understand the C/C++ compiler strategy | [03-c-cpp-compiler-strategy.md](03-c-cpp-compiler-strategy.md) |
| Track milestones and independence % | [04-milestone-matrix.md](04-milestone-matrix.md) |
| Review self-hosting details | [../compiler-selfhosting/](../compiler-selfhosting/README.md) |
| Review linker design | [../linker/](../linker/) |
| Review backend options | [../codegen/](../codegen/) |

---

## Comparison with Other Languages

| Capability | TML (goal) | Zig | Go | Rust | D |
|-----------|-----------|-----|-----|------|---|
| Self-hosted compiler | ✅ planned | ✅ | ✅ | ✅ | ✅ |
| Custom backend | ✅ planned | ✅ (+ LLVM) | ✅ | ❌ (LLVM only) | ✅ |
| Custom linker | ✅ planned | ✅ | ✅ | ❌ (LLD) | ✅ |
| C compiler | ✅ planned | ✅ (Clang wrapper) | ❌ | ❌ | ❌ |
| C++ compiler | ✅ planned | ✅ (Clang wrapper) | ❌ | ❌ | ❌ |
| No LLVM dependency | ✅ planned | ⚠️ (optional) | ✅ | ❌ | ✅ |
| Binary size | ~15 MB (goal) | ~45 MB | ~15 MB | ~200 MB | ~40 MB |
