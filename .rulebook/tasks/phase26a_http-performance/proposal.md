# Proposal: HTTP Performance Optimization — Target 500K req/s

**Task**: phase10-05-http-performance
**Status**: BLOCKED — Phase 0 regression uninvestigated. 35% (8/23)
**Priority**: P0 — active performance regression is a blocker for all HTTP work
**Estimated effort**: 5–7 days
**Risk**: High — regression root cause unknown; may be a codegen issue unrelated to profiler

## Problem

The HTTP server regressed from 183K req/s to 8K req/s after profiler instrumentation was added
to stdlib hot paths. The root cause has not been investigated. This 23x regression makes TML's
HTTP server unusable for any performance-sensitive workload and blocks the benchmark comparison
task (phase10-06). Autocannon pipelining was fixed in Phase 1 (14K), but the bombardier
single-request regression is unresolved.

## Proposed Solution

**Phase 0 (CRITICAL — must complete first)**: Instrument the profiler call sites to confirm
whether `profiler::begin/end` in hot paths (str, option, hashmap, http connection loop) accounts
for the regression. Remove profiler calls from all stdlib hot paths. Keep profiler only in
non-hot-path code (file I/O, crypto, net connect). Verify bombardier returns to 180K+ baseline
before proceeding to any other phase.

**Phase 1 (done)**: HTTP pipelining fixed — 3 pipelined requests return 3x 200 OK.

**Phases 2–5 (after regression is fixed)**:
- Phase 2: Per-worker response buffer reuse, static response precomputation API (target 70K+)
- Phase 3: writev FFI for vectored I/O, lock-free MPSC queue (target 120K+)
- Phase 4: Per-worker event loop, IOCP worker count = CPU core count (target 200K+)
- Phase 5: SIMD CRLF/colon scanning in HTTP parser (target 250K+)

## Key Decisions

- Profiler must be opt-in only: never call profiler::begin/end inside tight loops or
  frequently-called stdlib methods (str operations, option unwrap, hashmap get).
- Per-worker buffers: allocate response buffers once per worker thread, reuse across
  requests. Eliminates cross-thread allocation contention.
- Static response API: precompute headers for fixed responses (health check, plaintext).
  Avoids repeated serialization of identical bytes on every request.
- IOCP worker count = logical CPU count: current IOCP mode runs at 13K, indicating
  the worker count or completion port configuration is wrong.

## Files to Create/Modify

- `lib/std/src/http/server.tml` — remove profiler calls, per-worker buffer reuse
- `lib/std/src/http/connection.tml` — SIMD CRLF scanning, vectored I/O (writev FFI)
- `lib/std/src/http/response.tml` — static response precomputation API
- `compiler/runtime/net/iocp.c` — per-worker event loop, IOCP configuration fix

## Success Criteria

- Phase 0: bombardier returns to 180K+ req/s baseline (recovered from 8K regression)
- Phase 2: autocannon reaches 70K+ req/s with per-worker buffer reuse
- Phase 3: autocannon reaches 120K+ req/s with vectored I/O
- Phase 4: autocannon reaches 200K+ req/s with per-worker event loop
- Phase 5: autocannon reaches 250K+ req/s with SIMD parser
- All phases: zero error rate under load, pipelining continues to return 3x 200 OK

## Dependencies

- Depends on: std::http (existing), compiler/runtime/net/iocp.c (existing)
- Blocks: phase10-06-http-benchmark (benchmark comparison meaningless during regression)
