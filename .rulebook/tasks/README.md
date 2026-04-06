# Tasks Index

**Total**: 31 tasks | **Last updated**: 2026-04-05

---

## Phase 8: Database Drivers

| Status | Task | Progress | Description |
|--------|------|----------|-------------|
| 🔶 | phase8f_db-postgres | 12/14 | PostgreSQL driver via libpq FFI |
| ⬚ | phase8g_db-mongodb | 0/16 | MongoDB driver via libmongoc FFI |
| ⬚ | phase8h_db-redis-mysql | 0/18 | Redis (hiredis) + MySQL (libmysqlclient) drivers |
| 🔶 | phase8x_db-perf-optimization | 5/22 | Database performance optimization |
| ✅ | phase8y_db-typeorm-parity | 40/40 | TypeORM-style ORM parity |

## Phase 9: AI / Machine Learning

| Status | Task | Progress | Description |
|--------|------|----------|-------------|
| ⬚ | phase9d_ia-cuda | 0/22 | CUDA FFI bindings + GPU compute |
| ⬚ | phase9e_ia-model-loading | 0/13 | GGUF/SafeTensors model loading |
| ⬚ | phase9f_ia-inference | 0/15 | LLM inference engine |
| ⬚ | phase9g_ia-lora-finetune | 0/8 | LoRA/QLoRA fine-tuning |
| ⬚ | phase9h_ia-distributed | 0/8 | Multi-GPU distributed training (NCCL) |
| ⬚ | phase9i_ia-bench-serve | 0/12 | AI benchmarks + HTTP serving (OpenAI API) |

## Phase 10: HTTP & Build System

| Status | Task | Progress | Description |
|--------|------|----------|-------------|
| 🔶 | phase10_build-tml-package-system | 31/37 | Rust-style build.tml + native lib resolution |
| 🔴 | phase10-05-http-performance | 7/23 | HTTP performance — fix 183K→8K regression, target 500K |
| ⬚ | phase10-06-http-benchmark | 0/26 | HTTP benchmark: TML vs Go vs Rust vs Node.js |
| 🔶 | phase10-07-db-http-integration | 1/10 | REST APIs + TechEmpower benchmarks |

## Phase 11: Toolchain & Infrastructure

| Status | Task | Progress | Description |
|--------|------|----------|-------------|
| ⬚ | phase11-01-package-manager | — | Package manager (semver, registry, lock file) |
| ⬚ | phase11-02-package-manager-alt | 0/37 | Package manager — alternative design |
| ⬚ | phase11-03-auto-parallel | 0/41 | Automatic parallelization |
| ⬚ | phase11-04-cross-compilation | 0/118 | Cross-compilation targets |
| 🔶 | phase11-05-self-hosting-compiler | 7/234 | Self-hosting compiler (legacy — see Phase 12/13) |
| ⬚ | phase11-06-self-hosting-cranelift | 0/10 | Cranelift backend exploration |

## Phase 12: Self-Hosting Foundation (ERA 1 — Phase 0)

Pre-work infrastructure before any compiler code can be ported to TML.

| Status | Task | Progress | Description |
|--------|------|----------|-------------|
| ⬚ | phase12a_mir-consolidation | 0/22 | Retire legacy HIR→MIR builder (~3.5K LOC removed) |
| ⬚ | phase12b_string-interning | 0/14 | Build `std::intern` module (~200 LOC TML) |
| ⬚ | phase12c_typechecker-invariants | 0/24 | Document type checker invariants (50-100 pages) |
| ⬚ | phase12d_ir-diff-tool | 0/16 | Semantic LLVM IR comparison tool |
| ⬚ | phase12e_ast-serializers | 0/22 | AST/TypeEnv binary serialization |
| ⬚ | phase12f_hybrid-pipeline | 0/18 | C++/TML runtime stage swapping framework |

**Dependency order**: 12a,12b,12c,12d parallel → 12e (after 12a) → 12f (after 12d+12e)

## Phase 13: TML Frontend (ERA 1 — Phase 1)

Port lexer and parser from C++ to TML. First compiler subsystems running in TML.

| Status | Task | Progress | Description |
|--------|------|----------|-------------|
| ⬚ | phase13a_tml-token-ast-types | 0/24 | Token & AST type definitions (~2,300 LOC TML) |
| ⬚ | phase13b_tml-lexer | 0/24 | TML lexer rewrite (~1,800 LOC TML) |
| ⬚ | phase13c_tml-parser | 0/25 | TML parser rewrite — Pratt parsing (~4,100 LOC TML) |
| ⬚ | phase13d_frontend-integration | 0/18 | Wire TML frontend into hybrid pipeline |

**Dependency order**: 13a → 13b → 13c → 13d (sequential)

---

## Legend

| Icon | Meaning |
|------|---------|
| ✅ | Complete |
| 🔶 | In Progress |
| 🔴 | Blocked |
| ⬚ | Planned |

## Roadmap

```
Phase 8-9-10-11: Feature development (DB, AI, HTTP, toolchain)
Phase 12: Self-hosting foundation (ERA 1, Phase 0) — 4 months
Phase 13: TML frontend (ERA 1, Phase 1) — 4 months
Phase 14+: Type checker, IR pipeline, codegen, bootstrap (ERA 1, Phases 2-5)
```

Full independence plan: [docs/analyses/independence-plan/](../../docs/analyses/independence-plan/README.md)
