# Tasks — Priority Table

**Updated**: 2026-03-21
**Active**: 17 tasks | **Archived**: 11
**Test baseline**: 1599+ tests, ~110/119 HTTP, 52/52 iter, 73/73 collections
**Version**: 0.2.1

---

## Tier 1: High Priority — ALL CLEARED

| # | Task | Status | Progress | Archived |
|---|------|--------|----------|----------|
| 1 | ~~fix-struct-codegen-blockers~~ | **ARCHIVED** | 18/18 | 2026-03-20 |
| 2 | ~~codegen-structural-fixes~~ | **ARCHIVED** | 40/40 | 2026-03-21 |
| 3 | ~~fix-codegen-coverage-blockers~~ | **ARCHIVED** | 44/44 | 2026-03-21 |

## Tier 2: Medium Priority (product features)

| # | Task | Status | Progress | Next step |
|---|------|--------|----------|-----------|
| 4 | **http-production-server** | 97% | 69/71 | Remaining: 8.3 work-stealing, 10.1 SIMD (both deferred to specialized tasks) |
| 5 | ~~http-production-framework~~ | **ARCHIVED** | 46/46 | Archived 2026-03-21 |
| 6 | ~~complete-async-coverage~~ | **ARCHIVED** | 25/25 | Archived 2026-03-21 |
| 7 | **developer-tooling** | 75% | 52/70 | Phase 1 doc comment preservation in lexer/parser |
| 8 | **zig-cc-compiler-integration** | 45% | 8/18 | 3.1 zig cc detection in compiler_setup.cpp |

## Tier 3: Low Priority (incremental improvements)

| # | Task | Status | Progress | Next step |
|---|------|--------|----------|-----------|
| 9 | **optimize-codegen-like-rust** | 75% | 28/33 | Phase 6 exception handling (invoke/cleanuppad) |
| 10 | **implement-reflection** | 48% | 33/70 | 3.1.4 get_field method |
| 11 | **zig-inspired-test-migration** | 60% | 12/20 | 1.7 fix library_decls_only for all functions |
| 12 | **package-manager** | 20% | 7/40 | Phase 1 git dependencies |
| 13 | **language-completeness-roadmap** | 48% | 70/172 | Update with recent progress (12 codegen fixes) |

## Tier 4: Future (not started, planning)

| # | Task | Status | Progress | Dependencies |
|---|------|--------|----------|-------------|
| 14 | **self-hosting-compiler** | 0% | 5/200+ | Tier 1 cleared — language now ready |
| 15 | **add-compiler-cpp-unit-tests** | 0% | 0/93 | No hard dependencies |
| 16 | **function-contracts** | 0% | 0/12 | Future language feature |
| 17 | **tracy-profiler-integration** | 0% | 0/50+ | None |
| 18 | **inspector-diagnostics** | 0% | 0/66+ | reflection + developer-tooling |
| 19 | **implement-simd-generic-isa** | 0% | 0/100+ | M6 roadmap |
| 20 | **simd-optimization** | 0% | 0/100+ | M6 roadmap |
| 21 | **auto-parallel** | 0% | 0/32 | M6 roadmap |
| 22 | **cross-compilation** | 0% | 0/85+ | M6 roadmap |
| 23 | **self-hosting-cranelift** | 0% | 0/10 | Cranelift backend must exist |

---

## HTTP Server Phase Summary

| Phase | Status | Items | Description |
|-------|--------|-------|-------------|
| 1. Critical Bugs | **COMPLETE** | 9/9 | Routing, parsing, body reading, shutdown |
| 2. HTTP/1.1 Compliance | **COMPLETE** | 10/10 | Chunked, 100-continue, Date, 405/501, URL decode, idle timeout |
| 3. Middleware & Hooks | **COMPLETE** | 4/4 | onRequest/preHandler/onResponse/onError, custom error handler |
| 4. Event Loop | **COMPLETE** | 4/4 | Non-blocking I/O, body accumulation, multi-thread workers |
| 4b. IOCP | 83% | 10/12 | Windows async I/O, remaining: scaling >100/500 connections |
| 5. HTTP Client | 83% | 5/6 | GET/POST/PUT/DELETE, redirect, timeout, chunked, pool structure |
| 6. Production | **COMPLETE** | 5/5 | SO_REUSEPORT, multi-value headers, WebSocket, request ID, logging |
| 7. Memory Opt | 20% | 1/5 | Buffer resize done, remaining: arena, Bytes, backpressure |
| 8. Perf Instrumentation | 50% | 2/4 | Latency + timeout done, remaining: work-stealing, graceful shutdown |
| 9. TLS & HTTP/2 | 86% | 6/7 | TLS, ALPN, H2 frames/streams/HPACK/flow/server done |
| 10. Advanced Opt | 0% | 0/4 | SIMD, sendfile, vectored I/O, offset parsing |

## Dependency Graph

```
Tier 1 CLEARED ──────────────────────────────────
  All codegen blockers resolved (102/102 items)
  Unblocked: HTTP middleware, async futures, self-hosting

http-production-server (59/71)
  |-> Optimization phases remain (IOCP, memory, SIMD)

developer-tooling (52/70)
  |-> inspector-diagnostics (depends on LSP + reflection)

implement-reflection (33/70)
  |-> inspector-diagnostics
```

## Recently Archived

| Task | Date | Reason |
|------|------|--------|
| complete-async-coverage | 2026-03-21 | 25/25: Pin, Future, Poll, AsyncIter, networking all verified |
| http-production-framework | 2026-03-21 | 46/46: hooks, content-type parser, IOCP, arena, all phases complete |
| tml-language-gaps | 2026-03-21 | 6/6 gaps + 2 dyn fixes: Bool/i1, dyn, async, templates, nullable, pattern |
| codegen-structural-fixes | 2026-03-21 | 40/40: Pin dispatch, cross-module field resolution, future_poll |
| fix-codegen-coverage-blockers | 2026-03-21 | 44/44: generics, intrinsics, closures, Pin, field resolution |
| fix-struct-codegen-blockers | 2026-03-20 | 18/18: ptr_read/write, field mutation, fnptr coercion, fold[B] |
| async-network-stack | 2026-03-20 | 44/44 items complete |
| refactor-async-use-existing-apis | 2026-03-20 | 38 files refactored |
| user-docs-appendix-rewrite | 2026-03-20 | 15/15 docs written |
| write-user-docs-oop | 2026-03-20 | OOP chapter complete |
| write-user-docs-ownership | 2026-03-20 | Ownership chapter complete |
