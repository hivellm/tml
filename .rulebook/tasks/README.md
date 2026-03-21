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
| 4 | **http-production-server** | 97% | 69/71 | 8.3 work-stealing + 10.1 SIMD (deferred) |
| 5 | ~~http-production-framework~~ | **ARCHIVED** | 46/46 | 2026-03-21 |
| 6 | ~~complete-async-coverage~~ | **ARCHIVED** | 25/25 | 2026-03-21 |
| 7 | **developer-tooling** | 58% | 45/78 | Phase 1: doc comment preservation (C++ lexer/parser) |
| 8 | **zig-cc-compiler-integration** | 57% | 8/14 | 3.1 zig cc detection in compiler_setup.cpp |

## Tier 3: Low Priority (incremental improvements)

| # | Task | Status | Progress | Next step |
|---|------|--------|----------|-----------|
| 9 | **optimize-codegen-like-rust** | 73% | 27/37 | Phase 6 exception handling (invoke/cleanuppad) |
| 10 | **implement-reflection** | 41% | 37/90 | Phase 5: get_field, set_field methods |
| 11 | **zig-inspired-test-migration** | 65% | 11/17 | 1.7 fix library_decls_only |
| 12 | **package-manager** | 16% | 7/45 | Phase 1 git dependencies |
| 13 | **language-completeness-roadmap** | 36% | 60/169 | Tracking doc — update with recent progress |

## Tier 4: Future (not started, planning)

| # | Task | Status | Progress | Dependencies |
|---|------|--------|----------|-------------|
| 14 | **self-hosting-compiler** | 3% | 7/234 | Tier 1 cleared — language now ready |
| 15 | **add-compiler-cpp-unit-tests** | 0% | 0/161 | No hard dependencies |
| 16 | **function-contracts** | 0% | 0/16 | Future language feature |
| 17 | **tracy-profiler-integration** | 0% | 0/58 | None |
| 18 | **inspector-diagnostics** | 0% | 0/117 | reflection + developer-tooling |
| 19 | **implement-simd-generic-isa** | 0% | 0/152 | M6 roadmap |
| 20 | **simd-optimization** | 0% | 0/116 | M6 roadmap |
| 21 | **auto-parallel** | 0% | 0/41 | M6 roadmap |
| 22 | **cross-compilation** | 0% | 0/118 | M6 roadmap |
| 23 | **self-hosting-cranelift** | 0% | 0/10 | Cranelift backend must exist |

---

## HTTP Server Phase Summary

| Phase | Status | Items | Description |
|-------|--------|-------|-------------|
| 1. Critical Bugs | **COMPLETE** | 9/9 | Routing, parsing, body reading, shutdown |
| 2. HTTP/1.1 Compliance | **COMPLETE** | 10/10 | Chunked, 100-continue, Date, 405/501, URL decode, idle timeout |
| 3. Middleware & Hooks | **COMPLETE** | 4/4 | onRequest/preHandler/onResponse/onError, custom error handler |
| 4. Event Loop | **COMPLETE** | 4/4 | Non-blocking I/O, body accumulation, multi-thread workers |
| 4b. IOCP | **COMPLETE** | 12/12 | Accept pool 256, slot hint O(1), initial 4K slots |
| 5. HTTP Client | **COMPLETE** | 6/6 | GET/POST/PUT/DELETE, redirect, timeout, chunked, pool |
| 6. Production | **COMPLETE** | 5/5 | SO_REUSEPORT, multi-value headers, WebSocket, request ID, logging |
| 7. Memory Opt | **COMPLETE** | 5/5 | Buffer resize, arena allocator, dynamic slots, Bytes, backpressure |
| 8. Perf Instrumentation | 75% | 3/4 | Latency + timeout + graceful shutdown. Remaining: work-stealing |
| 9. TLS & HTTP/2 | **COMPLETE** | 7/7 | TLS, ALPN, H2 frames/streams/HPACK/flow/server |
| 10. Advanced Opt | 75% | 3/4 | sendfile, vectored I/O, offset parsing. Remaining: SIMD |

## Dependency Graph

```
Tier 1 CLEARED (102/102) ────────────────────────
  All codegen blockers resolved
  Unblocked: HTTP, async futures, self-hosting

http-production-server (69/71) ──────────────────
  Functional: COMPLETE (Phase 1-7, 9)
  Remaining: work-stealing (8.3), SIMD parsing (10.1)

developer-tooling (45/78) ───────────────────────
  Needs: C++ lexer/parser changes for doc comments
  |-> inspector-diagnostics

implement-reflection (37/90) ────────────────────
  |-> inspector-diagnostics
```

## Recently Archived

| Task | Date | Reason |
|------|------|--------|
| complete-async-coverage | 2026-03-21 | 25/25: Pin, Future, Poll, AsyncIter, networking |
| http-production-framework | 2026-03-21 | 46/46: hooks, content-type parser, IOCP, arena |
| tml-language-gaps | 2026-03-21 | 8/8: Bool/i1, dyn, async, templates, nullable, dyn boxing |
| codegen-structural-fixes | 2026-03-21 | 40/40: Pin dispatch, field resolution, future_poll |
| fix-codegen-coverage-blockers | 2026-03-21 | 44/44: generics, intrinsics, closures, Pin |
| fix-struct-codegen-blockers | 2026-03-20 | 18/18: ptr_read/write, field mutation, fnptr, fold[B] |
| async-network-stack | 2026-03-20 | 44/44 items complete |
| refactor-async-use-existing-apis | 2026-03-20 | 38 files refactored |
| user-docs-appendix-rewrite | 2026-03-20 | 15/15 docs written |
| write-user-docs-oop | 2026-03-20 | OOP chapter complete |
| write-user-docs-ownership | 2026-03-20 | Ownership chapter complete |
