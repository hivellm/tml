# TML Project — Task Index

**Last updated**: 2026-04-05
**Active tasks**: 35 | **Archived**: 5+

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

## Phase 8 — Database Drivers

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 8f | [PostgreSQL Driver](phase8f_db-postgres/) | In Progress | P1 | 12/14 |
| 8g | [MongoDB Driver](phase8g_db-mongodb/) | Planned | P2 | 0/16 |
| 8h | [Redis + MySQL Drivers](phase8h_db-redis-mysql/) | Planned | P2 | 0/18 |
| 8x | [DB Perf Optimization](phase8x_db-perf-optimization/) | In Progress | P1 | 5/22 |
| 8y | [DB TypeORM Parity](phase8y_db-typeorm-parity/) | **Complete** | P1 | 40/40 |

## Phase 9 — AI / Machine Learning

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 9d | [CUDA FFI Bindings](phase9d_ia-cuda/) | Planned | HIGH | 0/22 |
| 9e | [Model Loading](phase9e_ia-model-loading/) | Planned | HIGH | 0/13 |
| 9f | [Inference Engine](phase9f_ia-inference/) | Planned | HIGH | 0/15 |
| 9g | [LoRA/QLoRA Fine-Tuning](phase9g_ia-lora-finetune/) | Planned | MEDIUM | 0/8 |
| 9h | [Multi-GPU Distributed](phase9h_ia-distributed/) | Planned | LOW | 0/8 |
| 9i | [Benchmarks + HTTP Serving](phase9i_ia-bench-serve/) | Planned | LOW | 0/12 |

## Phase 10 — HTTP & Build System

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 10 | [Build.tml Package System](phase10_build-tml-package-system/) | In Progress | HIGH | 31/37 |
| 10-05 | [HTTP Performance](phase10-05-http-performance/) | **BLOCKED** | P0 | 7/23 |
| 10-06 | [HTTP Benchmark](phase10-06-http-benchmark/) | In Progress | P1 | 0/26 |
| 10-07 | [DB + HTTP Integration](phase10-07-db-http-integration/) | In Progress | P1 | 1/10 |

## Phase 11 — Toolchain & Infrastructure

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 11-01 | [Package Manager](phase11-01-package-manager/) | Planned | P1 | — |
| 11-02 | [Package Manager Alt](phase11-02-package-manager-alt/) | Planned | P1 | 0/37 |
| 11-03 | [Auto-Parallel](phase11-03-auto-parallel/) | Planned | P2 | 0/41 |
| 11-04 | [Cross-Compilation](phase11-04-cross-compilation/) | Planned | P2 | 0/118 |
| 11-05 | [Self-Hosting (legacy)](phase11-05-self-hosting-compiler/) | Superseded by Phase 12/13 | P2 | 7/234 |
| 11-06 | [Cranelift Backend](phase11-06-self-hosting-cranelift/) | Planned | P2 | 0/10 |

## Phase 12 — Self-Hosting Foundation (ERA 1, Phase 0)

Pre-work infrastructure before any compiler code can be ported to TML. See [independence plan](../../docs/analyses/independence-plan/).

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 12a | [MIR Consolidation](phase12a_mir-consolidation/) | Planned | P0 | 0/22 |
| 12b | [String Interning](phase12b_string-interning/) | Planned | P1 | 0/14 |
| 12c | [TypeChecker Invariants](phase12c_typechecker-invariants/) | Planned | P0 | 0/24 |
| 12d | [IR-Diff Tool](phase12d_ir-diff-tool/) | Planned | P0 | 0/16 |
| 12e | [AST Serializers](phase12e_ast-serializers/) | Planned | P0 | 0/22 |
| 12f | [Hybrid Pipeline](phase12f_hybrid-pipeline/) | Planned | P0 | 0/18 |

**Order**: 12a,12b,12c,12d parallel → 12e (after 12a) → 12f (after 12d+12e)

## Phase 13 — TML Frontend (ERA 1, Phase 1)

Port lexer and parser from C++ to TML — first compiler subsystems in TML.

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 13a | [Token & AST Types](phase13a_tml-token-ast-types/) | Planned | P0 | 0/24 |
| 13b | [TML Lexer](phase13b_tml-lexer/) | Planned | P0 | 0/24 |
| 13c | [TML Parser](phase13c_tml-parser/) | Planned | P0 | 0/25 |
| 13d | [Frontend Integration](phase13d_frontend-integration/) | Planned | P0 | 0/18 |

**Order**: 13a → 13b → 13c → 13d (sequential)

## Phase 14 — Type Checker in TML (ERA 1, Phase 2) ⚠️ CRITICAL PATH

Port the type checker (~21K LOC C++) to TML. Highest-risk, longest phase (8 months). See [invariant doc](phase12c_typechecker-invariants/).

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 14a | [Type Registration](phase14a_typechecker-registration/) | Planned | P0 | 0/22 |
| 14b | [Module Resolution](phase14b_typechecker-module-resolution/) | Planned | P0 | 0/20 |
| 14c | [Type Inference](phase14c_typechecker-inference/) | Planned | P0 | 0/26 |
| 14d | [Behavior Dispatch](phase14d_typechecker-behavior-dispatch/) | Planned | P0 | 0/22 |

**Order**: 14a → 14b → 14c → 14d (sequential, 14a/14b can partially overlap)

## Research

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| R-01 | [LLM IR Debugging](llm-ir-debugging-research/) | Data Collection | MEDIUM | 70% |

---

## Archived Tasks

| Task | Archived | Result |
|------|----------|--------|
| Core FFI Types (1-03) | 2026-03-25 | 100% — 20/20 items |
| Std FFI Types (1-04) | 2026-03-25 | 100% |
| Panic Recovery (1-05) | 2026-03-25 | 100% |
| Compiler Hints (1-06) | 2026-03-25 | 100% |
| Compiler C++ Unit Tests (1-07) | 2026-03-25 | 100% — 82 test files |
| BinaryHeap (2-01) | 2026-03-25 | 100% |
| Semaphore (2-02) | 2026-03-25 | 100% |
| Migrate Lowlevel (2-10) | 2026-03-25 | 100% |

---

## Roadmap Summary

```
Active now:   Phase 1 (language), Phase 4 (tooling), Phase 8 (DB), Phase 10 (HTTP/build)
Next:         Phase 12 (self-hosting foundation) — can start immediately
Then:         Phase 13 (TML frontend) — after Phase 12 complete
Future:       Phase 14+ (type checker, IR pipeline, codegen, bootstrap)
Long-term:    Custom backend, custom linker, C/C++ frontend (see independence plan)
```
