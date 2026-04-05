# TML Compiler Self-Hosting Feasibility Analysis

**Date**: 2026-04-05  
**Status**: Complete — 8 analysis documents + index  
**Scope**: Full analysis of TML compiler self-hosting feasibility, migration strategy, risk assessment  
**Total**: ~5,400 lines of analysis across 9 files

---

## Document Index

| # | File | Lines | Description |
|---|------|-------|-------------|
| 0 | [00-executive-summary.md](00-executive-summary.md) | 249 | Executive overview: verdict, timeline, risks, go/no-go criteria |
| 1 | [01-compiler-inventory.md](01-compiler-inventory.md) | 850 | Complete subsystem inventory: LOC, complexity, data structures, dependencies |
| 2 | [02-stdlib-readiness.md](02-stdlib-readiness.md) | 476 | Stdlib gap analysis: what a compiler needs vs what TML provides |
| 3 | [03-language-gaps.md](03-language-gaps.md) | 970 | Language feature assessment: 18 features rated with workarounds |
| 4 | [04-runtime-dependencies.md](04-runtime-dependencies.md) | 431 | C runtime, LLVM, LLD, OS dependency map with elimination strategy |
| 5 | [05-migration-strategy.md](05-migration-strategy.md) | 660 | Phase-by-phase bootstrap plan: Phase 0–5 with timeline |
| 6 | [06-prior-art.md](06-prior-art.md) | 685 | Lessons from Rust, Go, Zig, TypeScript, D, Nim self-hosting |
| 7 | [07-risk-matrix.md](07-risk-matrix.md) | 1,070 | 18 risks scored with mitigations, heat map, go/no-go framework |

---

## Key Findings (TL;DR)

**Verdict: Self-hosting is feasible. Estimated timeline: 24–30 months for a single developer.**

### Why It's Feasible
1. **IR-as-text LLVM interface** — `LLVMParseIRInContext` means TML only needs to generate a string, not use LLVM builder API
2. **Mature stdlib** (535 TML files, 141K+ LOC) — Arena, BitSet, BTreeMap, Deque, HashMap, Regex, file I/O, process spawning all exist
3. **Sufficient language features** — `Heap[T]` for recursive types, `when` pattern matching, generics, closures, `@extern("c")` FFI
4. **Proven migration methodology** — Phase 4 migrated 5,210+ lines of C runtime to pure TML
5. **1,700+ passing tests** as continuous regression safety net

### What's Missing
- **String interning** — needs building (~200 lines TML)
- **Graph data structure** — can use `HashMap[NodeId, List[NodeId]]` as workaround
- **Type checker documentation** — 21K LOC of implicit knowledge must be made explicit before porting
- **MIR path consolidation** — dual HIR→MIR / THIR→MIR paths double the porting scope

### Permanent C++ Boundary (1,593 LOC)
```
TML Compiler (self-hosted) → IR text string → llvm_backend.cpp (550) → lld_linker.cpp (670) → .exe
                                               jit_engine.cpp (373) for `tml run`
```

### Recommended Strategy
1. **Phase 0** — Pre-work: consolidate MIR, build std::intern, document type checker (3–4 months)
2. **Phase 1** — Lexer + Parser: 9.2K LOC → ~6K TML (3–4 months)
3. **Phase 2** — Type Checker: 21K LOC → ~14K TML — HIGHEST RISK (6–9 months)
4. **Phase 3** — HIR + THIR + MIR: ~33K LOC → ~22K TML (4–5 months)
5. **Phase 4** — MIR Codegen: ~76K LOC → ~50K TML — LARGEST (4–6 months)
6. **Phase 5** — Query + CLI + Tooling + Bootstrap verification (2–3 months)

---

## How to Read This Analysis

| You want to... | Read... |
|----------------|---------|
| Make a go/no-go decision | [00-executive-summary.md](00-executive-summary.md) |
| Plan the implementation | [05-migration-strategy.md](05-migration-strategy.md) |
| Understand the risks | [07-risk-matrix.md](07-risk-matrix.md) |
| Know what stdlib work is needed first | [02-stdlib-readiness.md](02-stdlib-readiness.md) |
| Know what language features are missing | [03-language-gaps.md](03-language-gaps.md) |
| Deep-dive into compiler structure | [01-compiler-inventory.md](01-compiler-inventory.md) |
| Understand LLVM/LLD dependency | [04-runtime-dependencies.md](04-runtime-dependencies.md) |
| Learn from other projects | [06-prior-art.md](06-prior-art.md) |
