# TML Project — Task Index

**Last updated**: 2026-04-06
**Active tasks**: 58 | **Archived**: 7+

---

## Phase 0 — Codegen Architecture Fixes

Structural fixes to MIR codegen based on Rust/Go/Clang comparison.

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 0a | [Eliminate i32 Fallbacks](phase0a_codegen-eliminate-i32-fallbacks/) | Planned | CRITICAL | 0/18 |
| 0b | [ABI Module](phase0b_codegen-abi-module/) | Planned | HIGH | 0/20 |
| 0c | [CGValue Wrapper](phase0c_codegen-cgvalue-wrapper/) | Planned | HIGH | 0/20 |
| 0d | [Table-Driven Intrinsics](phase0d_codegen-table-driven-intrinsics/) | Planned | MEDIUM | 0/18 |
| 0e | [Unit Type Cleanup](phase0e_codegen-unit-type-cleanup/) | Planned | MEDIUM | 0/12 |
| 0f | [Typed Emit Helpers](phase0f_codegen-typed-emit-helpers/) | Planned | MEDIUM | 0/15 |

## Phase 0 — JIT Execution Engine

LLVM ORC JIT integration for scripting and fast iteration.

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 0a-jit | [JIT CMake Integration](phase0a_jit-cmake-integration/) | Planned | CRITICAL | 0/7 |
| 0b-jit | [JIT Engine Core](phase0b_jit-engine-core/) | Planned | CRITICAL | 0/12 |
| 0c-jit | [JIT Runtime Symbols](phase0c_jit-runtime-symbols/) | Planned | HIGH | 0/10 |
| 0d-jit | [JIT CLI Integration](phase0d_jit-cli-integration/) | Planned | HIGH | 0/14 |

## Phase 1 — Foundation & Language

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 1-01 | [Language Completeness](phase1-01-language-completeness/) | In Progress | HIGH | 128/162 |
| 1-02 | [Function Contracts](phase1-02-function-contracts/) | Planned | MEDIUM | 0% |
| 1-08 | [Reflection System](phase1-08-reflection/) | Blocked (Phase 5) | MEDIUM | 37/70 |

## Phase 2 — Stdlib Completeness

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 2-03 | [WaitGroup](phase2-03-wait-group/) | Planned | MEDIUM | 0% |
| 2-04 | [Seek Behavior](phase2-04-seek-behavior/) | Planned | MEDIUM | 0% |
| 2-05 | [BigInt](phase2-05-bigint/) | Planned | MEDIUM | 0% |
| 2-06 | [Complex Numbers](phase2-06-complex-numbers/) | Planned | LOW | 0% |
| 2-07 | [Trie](phase2-07-trie/) | Planned | MEDIUM | 0% |
| 2-08 | [IntervalTree](phase2-08-interval-tree/) | Planned | LOW | 0% |
| 2-09 | [Core Net Types](phase2-09-core-net-types/) | Planned | LOW | 0% |

## Phase 3 — Networking & HTTP

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 3-01 | [HTTP Performance](phase3-01-http-performance/) | **BLOCKED** — 183K→8K regression | CRITICAL | 8/23 |
| 3-02 | [HTTP Benchmark](phase3-02-http-benchmark/) | Planned | HIGH | 0% |

## Phase 4 — Tooling & Developer Experience

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 4-00 | [MCP Docs Coverage](mcp-docs-complete-coverage/) | Planned | HIGHEST | 0% |
| 4-01 | [Developer Tooling (LSP)](phase4-01-developer-tooling/) | In Progress | MEDIUM | ~65% |
| 4-02 | [Inspector Diagnostics](phase4-02-inspector-diagnostics/) | Planned | MEDIUM | 0% |
| 4-03 | [Package Manager](phase4-03-package-manager/) | Blocked | MEDIUM | 15% |
| 4-04 | [Test Migration](phase4-04-test-migration/) | In Progress | MEDIUM | 12/17 |
| 4-05 | [Fix Legacy Codegen ABI](fix-legacy-codegen-abi-bugs/) | Planned | HIGH | 0/14 |
| 4-06 | [Split Large Files](refactor-split-large-files/) | Planned | MEDIUM | 0/23 |

## Phase 5 — Performance & Optimization

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 5-01 | [SIMD Optimization](phase5-01-simd-optimization/) | Planned | HIGH | 0% |
| 5-02 | [SIMD Generic ISA](phase5-02-simd-generic-isa/) | Planned | HIGH | 0% |
| 5-03 | [Auto-Parallel](phase5-03-auto-parallel/) | Planned | HIGH | 0% |

---

## ERA 1: Self-Hosted Compiler (Phases 12–17)

### Phase 12 — Foundation (ERA 1, Phase 0)

Pre-work infrastructure before any compiler code can be ported to TML.

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 12a | [MIR Consolidation](phase12a_mir-consolidation/) | Planned | P0 | 0/22 |
| 12b | [String Interning](phase12b_string-interning/) | Planned | P1 | 0/14 |
| 12c | [TypeChecker Invariants](phase12c_typechecker-invariants/) | Planned | P0 | 0/24 |
| 12d | [IR-Diff Tool](phase12d_ir-diff-tool/) | Planned | P0 | 0/16 |
| 12e | [AST Serializers](phase12e_ast-serializers/) | Planned | P0 | 0/22 |
| 12f | [Hybrid Pipeline](phase12f_hybrid-pipeline/) | Planned | P0 | 0/18 |

**Order**: 12a,12b,12c,12d parallel → 12e (after 12a) → 12f (after 12d+12e)

### Phase 13 — TML Frontend (ERA 1, Phase 1)

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 13a | [Token & AST Types](phase13a_tml-token-ast-types/) | Planned | P0 | 0/24 |
| 13b | [TML Lexer](phase13b_tml-lexer/) | Planned | P0 | 0/24 |
| 13c | [TML Parser](phase13c_tml-parser/) | Planned | P0 | 0/25 |
| 13d | [Frontend Integration](phase13d_frontend-integration/) | Planned | P0 | 0/18 |

**Order**: 13a → 13b → 13c → 13d

### Phase 14 — Type Checker (ERA 1, Phase 2) ⚠️ CRITICAL PATH

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 14a | [Type Registration](phase14a_typechecker-registration/) | Planned | P0 | 0/22 |
| 14b | [Module Resolution](phase14b_typechecker-module-resolution/) | Planned | P0 | 0/20 |
| 14c | [Type Inference](phase14c_typechecker-inference/) | Planned | P0 | 0/26 |
| 14d | [Behavior Dispatch](phase14d_typechecker-behavior-dispatch/) | Planned | P0 | 0/22 |

**Order**: 14a → 14b → 14c → 14d

### Phase 15 — IR Pipeline (ERA 1, Phase 3)

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 15a | [HIR Lowering](phase15a_hir-lowering/) | Planned | P0 | 0/24 |
| 15b | [THIR Lowering](phase15b_thir-lowering/) | Planned | P0 | 0/16 |
| 15c | [MIR Builder](phase15c_mir-builder/) | Planned | P0 | 0/24 |
| 15d | [MIR Passes (52)](phase15d_mir-passes/) | Planned | P0 | 0/25 |

**Order**: 15a → 15b → 15c → 15d

### Phase 16 — Codegen (ERA 1, Phase 4)

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 16a | [Types & Declarations](phase16a_codegen-types-decls/) | Planned | P0 | 0/25 |
| 16b | [Instructions](phase16b_codegen-instructions/) | Planned | P0 | 0/25 |
| 16c | [Calls & ABI](phase16c_codegen-calls-abi/) | Planned | P0 | 0/25 |
| 16d | [Legacy LLVM Codegen](phase16d_codegen-legacy-llvm/) | Planned | P0 | 0/25 |

**Order**: 16a → 16b → 16c → 16d

### Phase 17 — Bootstrap (ERA 1, Phase 5) 🎯 SELF-HOSTING

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 17a | [Query System](phase17a_query-system/) | Planned | P0 | 0/18 |
| 17b | [CLI & Tooling](phase17b_cli-tooling/) | Planned | P0 | 0/24 |
| 17c | [Bootstrap Verification](phase17c_bootstrap-verification/) | Planned | P0 | 0/16 |

**Order**: 17a → 17b → 17c. Completion = **TML COMPILES ITSELF**

---

## ERA 2: Custom Native Backend (Phases 18–21)

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 18a | [MachIR Lowering](phase18a_debug-backend-machir/) | Planned | P1 | 0/20 |
| 18b | [x86_64 Encoder](phase18b_x86-encoder/) | Planned | P1 | 0/22 |
| 18c | [PE/COFF Object Emission](phase18c_pe-object-emission/) | Planned | P1 | 0/20 |
| 19a | [Register Allocator](phase19a_register-allocator/) | Planned | P1 | 0/22 |
| 20a | [Production x86_64](phase20a_production-backend-x86/) | Planned | P1 | 0/22 |
| 20b | [AArch64 Backend](phase20b_aarch64-backend/) | Planned | P1 | 0/21 |
| 21a | [Debug Info (PDB+DWARF)](phase21a_debug-info-pdb-dwarf/) | Planned | P1 | 0/24 |

**Order**: 18a → 18b+18c → 19a → 20a+20b → 21a. Completion = **LLVM ELIMINATED**

## ERA 3: Custom Linker (Phase 22)

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 22a | [PE/COFF Linker](phase22a_pe-coff-linker/) | Planned | P2 | 0/22 |
| 22b | [ELF Linker](phase22b_elf-linker/) | Planned | P2 | 0/20 |
| 22c | [Mach-O Linker](phase22c_macho-linker/) | Planned | P2 | 0/18 |
| 22d | [Incremental Linker](phase22d_incremental-linker/) | Planned | P2 | 0/18 |

**Order**: 22a → 22b → 22c → 22d. Completion = **LLD ELIMINATED**

## ERA 4: C/C++ Frontend (Phase 23)

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 23a | [C Preprocessor](phase23a_c-preprocessor/) | Planned | P2 | 0/20 |
| 23b | [C17 Frontend](phase23b_c-frontend/) | Planned | P2 | 0/24 |
| 23c | [C++ Subset Frontend](phase23c_cpp-subset-frontend/) | Planned | P2 | 0/22 |

**Order**: 23a → 23b → 23c. Completion = **FULL TOOLCHAIN INDEPENDENCE**

---

## Phase 24 — Database Drivers

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 24a | [MongoDB Driver](phase24a_db-mongodb/) | Planned | P2 | 0/16 |
| 24b | [Redis + MySQL Drivers](phase24b_db-redis-mysql/) | Planned | P2 | 0/18 |
| 24c | [DB Perf Optimization](phase24c_db-perf-optimization/) | In Progress | P1 | 5/22 |
| 24d | [DB TypeORM Parity](phase24d_db-typeorm-parity/) | **Complete** | P1 | 40/40 |

## Phase 25 — AI / Machine Learning

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 25a | [CUDA FFI Bindings](phase25a_ia-cuda/) | Planned | HIGH | 0/22 |
| 25b | [Model Loading](phase25b_ia-model-loading/) | Planned | HIGH | 0/13 |
| 25c | [Inference Engine](phase25c_ia-inference/) | Planned | HIGH | 0/15 |
| 25d | [LoRA/QLoRA Fine-Tuning](phase25d_ia-lora-finetune/) | Planned | MEDIUM | 0/8 |
| 25e | [Multi-GPU Distributed](phase25e_ia-distributed/) | Planned | LOW | 0/8 |
| 25f | [Benchmarks + HTTP Serving](phase25f_ia-bench-serve/) | Planned | LOW | 0/12 |

## Phase 26 — HTTP & Integration

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 26a | [HTTP Performance](phase26a_http-performance/) | **BLOCKED** | P0 | 7/23 |
| 26b | [HTTP Benchmark](phase26b_http-benchmark/) | In Progress | P1 | 0/26 |
| 26c | [DB + HTTP Integration](phase26c_db-http-integration/) | In Progress | P1 | 1/10 |

## Phase 27 — Toolchain & Infrastructure

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 27a | [Package Manager](phase27a_package-manager/) | Planned | P1 | — |
| 27b | [Package Manager Alt](phase27b_package-manager-alt/) | Planned | P1 | 0/37 |
| 27c | [Auto-Parallel](phase27c_auto-parallel/) | Planned | P2 | 0/41 |
| 27d | [Cross-Compilation](phase27d_cross-compilation/) | Planned | P2 | 0/118 |
| 27e | [Self-Hosting (legacy)](phase27e_self-hosting-legacy/) | Superseded by Phase 12-17 | P2 | 7/234 |
| 27f | [Cranelift Backend](phase27f_cranelift-backend/) | Planned | P2 | 0/10 |

## Research

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| R-01 | [LLM IR Debugging](llm-ir-debugging-research/) | Data Collection | MEDIUM | 70% |

---

## Archived Tasks

| Task | Archived | Result |
|------|----------|--------|
| Core FFI Types (1-03) | 2026-03-25 | 100% — 20/20 |
| Std FFI Types (1-04) | 2026-03-25 | 100% |
| Panic Recovery (1-05) | 2026-03-25 | 100% |
| Compiler Hints (1-06) | 2026-03-25 | 100% |
| Compiler C++ Unit Tests (1-07) | 2026-03-25 | 100% — 82 files |
| BinaryHeap (2-01) | 2026-03-25 | 100% |
| Semaphore (2-02) | 2026-03-25 | 100% |
| Migrate Lowlevel (2-10) | 2026-03-25 | 100% |
| Build.tml Package System (10) | 2026-04-06 | 100% — 32/32 |
| PostgreSQL Driver (8f) | 2026-04-06 | 100% — 8/8 tests |

---

## Roadmap Summary

```
Active:       Phase 0-5 (compiler fixes, language, stdlib, HTTP, tooling, perf)
ERA 1:        Phase 12-17 (self-hosting) — 25 tasks, 544 items
ERA 2:        Phase 18-21 (custom backend) — 7 tasks, 151 items
ERA 3:        Phase 22 (custom linker) — 4 tasks, 78 items
ERA 4:        Phase 23 (C/C++ frontend) — 3 tasks, 66 items
Features:     Phase 24-27 (DB, AI, HTTP, toolchain)

TOTAL: 58 active tasks, ~1500 checklist items
```
