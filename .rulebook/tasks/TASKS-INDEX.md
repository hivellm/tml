# TML Project — Task Index

**Last updated**: 2026-03-25
**Total tasks**: 26 active (4 archived)

## Phase 1 — Foundation & Language

Core language features, compiler infrastructure, and foundational gaps.

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 1-01 | [Language Completeness Roadmap](phase1-01-language-completeness/) | In Progress | HIGH | 79% (128/162) |
| 1-02 | [Function Contracts](phase1-02-function-contracts/) | Planning | MEDIUM | 0% |
| 1-03 | ~~Core FFI Types~~ | **ARCHIVED** 2026-03-25 | HIGH | 100% (20/20) |
| 1-04 | [Std FFI Types](phase1-04-std-ffi-types/) | **NEW** | HIGH | 0% |
| 1-05 | ~~Panic Recovery~~ | **ARCHIVED** 2026-03-25 | HIGH | 100% |
| 1-06 | ~~Compiler Hints~~ | **ARCHIVED** 2026-03-25 | MEDIUM | 100% |
| 1-07 | [Compiler C++ Unit Tests](phase1-07-compiler-unit-tests/) | Proposed | HIGHEST | 0% |
| 1-08 | [Reflection System](phase1-08-reflection/) | In Progress — Phase 5 BLOCKED | MEDIUM | 53% (37/70) |

## Phase 2 — Stdlib Completeness

Collections, sync primitives, math types, and library gaps.

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 2-01 | ~~BinaryHeap / PriorityQueue~~ | **ARCHIVED** 2026-03-25 | HIGH | 100% |
| 2-02 | [Semaphore](phase2-02-semaphore/) | **NEW** | HIGH | 0% |
| 2-03 | [WaitGroup](phase2-03-wait-group/) | **NEW** | MEDIUM | 0% |
| 2-04 | [Seek Behavior](phase2-04-seek-behavior/) | **NEW** | MEDIUM | 0% |
| 2-05 | [BigInt](phase2-05-bigint/) | **NEW** | MEDIUM | 0% |
| 2-06 | [Complex Numbers](phase2-06-complex-numbers/) | **NEW** | LOW | 0% |
| 2-07 | [Trie](phase2-07-trie/) | **NEW** | MEDIUM | 0% |
| 2-08 | [IntervalTree](phase2-08-interval-tree/) | **NEW** | LOW | 0% |
| 2-09 | [Core Net Types](phase2-09-core-net-types/) | **NEW** | LOW | 0% |
| 2-10 | [Migrate Lowlevel to Typed](phase2-10-migrate-lowlevel/) | **COMPLETE** | MEDIUM | 100% |

## Phase 3 — Networking & HTTP

HTTP server performance, benchmarks, and networking improvements.

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 3-01 | [HTTP Performance Optimization](phase3-01-http-performance/) | **BLOCKED** — 183K→8K regression | CRITICAL | 35% (8/23) |
| 3-02 | [HTTP Production Benchmark](phase3-02-http-benchmark/) | Proposed | HIGH | 0% |

## Phase 4 — Tooling & Developer Experience

LSP, debugger, package manager, test infrastructure.

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 4-01 | [Developer Tooling (LSP + VSCode)](phase4-01-developer-tooling/) | In Progress — LSP 0% (no C++ impl) | MEDIUM | ~65% real |
| 4-02 | [Inspector Diagnostics](phase4-02-inspector-diagnostics/) | Proposed | MEDIUM | 0% |
| 4-03 | [Package Manager](phase4-03-package-manager/) | **BLOCKED** — no registry service | MEDIUM | 15% |
| 4-04 | [Test Migration (Zig-inspired)](phase4-04-test-migration/) | In Progress — 2 blockers | MEDIUM | 71% (12/17) |

## Phase 5 — Performance & Optimization

SIMD, auto-parallelization, and compiler optimizations.

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 5-01 | [SIMD Optimization](phase5-01-simd-optimization/) | Planning | HIGH | 0% |
| 5-02 | [SIMD Generic ISA](phase5-02-simd-generic-isa/) | Planning | HIGH | 0% |
| 5-03 | [Auto-Parallel](phase5-03-auto-parallel/) | Proposed | HIGH | 0% |

## Phase 6 — Advanced Features & Self-Hosting

Cross-compilation and compiler self-hosting.

| ID | Task | Status | Priority | Progress |
|----|------|--------|----------|----------|
| 6-01 | [Cross-Compilation](phase6-01-cross-compilation/) | Proposed | HIGH | 0% |
| 6-02 | [Self-Hosting Compiler](phase6-02-self-hosting-compiler/) | Proposed | HIGH | 0% |
| 6-03 | [Self-Hosting Cranelift](phase6-03-self-hosting-cranelift/) | Proposed | LOW | 0% |

---

## Execution Order (Recommended)

### Now (foundation blockers)
1. **1-03** Core FFI Types — unlocks type-safe FFI across all libs
2. **2-01** BinaryHeap — fundamental missing collection
3. **2-02** Semaphore — essential sync primitive

### Next (stdlib completeness)
4. **1-05** Panic Recovery — unlocks robust server error handling
5. **2-04** Seek Behavior — completes the Read/Write/Seek trio
6. **2-03** WaitGroup — ergonomic concurrency
7. **2-05** BigInt — crypto/finance foundation

### Then (specialized collections + math)
8. **2-07** Trie — routing/autocomplete/prefix search
9. **1-04** Std FFI Types — CString, OsStr, OsString
10. **2-06** Complex Numbers — math/science
11. **2-08** IntervalTree — range queries
12. **1-06** Compiler Hints — optimization intrinsics
13. **2-09** Core Net Types — architectural cleanup

### Ongoing (parallel tracks)
- **3-01** HTTP Performance — continuous optimization
- **4-01** Developer Tooling — finishing LSP
- **1-01** Language Completeness — compiler bug fixes
- **1-08** Reflection — completing Phase 3-6

---

## Legacy Names (for reference)

Old directories that were renamed (some may still exist due to file locking):

| Old Name | New Name |
|----------|----------|
| language-completeness-roadmap | phase1-01-language-completeness |
| function-contracts | phase1-02-function-contracts |
| add-compiler-cpp-unit-tests | phase1-07-compiler-unit-tests |
| implement-reflection | phase1-08-reflection |
| migrate-lowlevel-to-typed | phase2-10-migrate-lowlevel |
| http-performance-optimization | phase3-01-http-performance |
| http-production-benchmark | phase3-02-http-benchmark |
| developer-tooling | phase4-01-developer-tooling |
| inspector-diagnostics | phase4-02-inspector-diagnostics |
| package-manager | phase4-03-package-manager |
| zig-inspired-test-migration | phase4-04-test-migration |
| simd-optimization | phase5-01-simd-optimization |
| implement-simd-generic-isa | phase5-02-simd-generic-isa |
| auto-parallel | phase5-03-auto-parallel |
| cross-compilation | phase6-01-cross-compilation |
| self-hosting-compiler | phase6-02-self-hosting-compiler |
| self-hosting-cranelift | phase6-03-self-hosting-cranelift |
