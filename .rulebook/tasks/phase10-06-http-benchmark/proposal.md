# Proposal: HTTP Production Benchmark — TML vs Go vs Rust/Tokio vs Node.js

**Task**: phase10-06-http-benchmark
**Status**: In Progress (0/23)
**Priority**: P1
**Estimated effort**: 2–3 days
**Risk**: Low — benchmark methodology is well-established; main risk is TML regression

## Problem

TML has no objective performance comparison against production HTTP runtimes. Without data,
it is impossible to identify which specific bottlenecks cause TML to underperform Go, Rust/Tokio,
or Node.js, or to measure whether optimizations in phase10-05 are closing the gap. Industry
consumers evaluating TML need this data to make adoption decisions.

## Proposed Solution

Write equivalent minimal HTTP servers in all four languages using stdlib only (no frameworks).
Run identical load scenarios with wrk/bombardier using a standardized harness. Collect req/s,
latency (p50/p99/p999), and error rate. Perform gap analysis by profiling the TML server with
`time_ns()` instrumentation and comparing per-request allocation counts.

**Three test endpoints (all four servers implement the same three)**:
- GET /json → `{"message":"Hello, World!"}` with Content-Type: application/json
- GET /plaintext → `Hello, World!` with Content-Type: text/plain
- POST /echo → read body, write body back verbatim

**Benchmark parameters**: 10s warmup, 30s measurement, 1/10/100/500 concurrent connections,
with and without pipelining. Results saved to `.sandbox/benchmarks/results/`.

After gap analysis, Phase 4 applies targeted fixes to the TML server (buffer reuse, response
precomputation, IOCP tuning, router optimization). Phase 5 publishes a final comparison table
in `docs/benchmarks/` with reproduction steps.

## Key Decisions

- Stdlib only for all reference servers: frameworks (gin, axum, express) hide runtime
  characteristics. Stdlib servers give a fair comparison of the language's I/O primitives.
- Same response format across all servers: enables apples-to-apples comparison. Content-Type
  headers must match exactly.
- Save raw results to `.sandbox/`: benchmark output is not checked into the repo but is
  available for analysis. Only the final report in `docs/benchmarks/` is committed.
- wrk preferred over autocannon: wrk is C-based with lower client overhead, giving more
  accurate server-side numbers at high connection counts.

## Files to Create/Modify

- `.sandbox/benchmarks/go_server.go` — Go net/http stdlib server
- `.sandbox/benchmarks/rust_server.rs` — Rust hyper-based server (tokio runtime)
- `.sandbox/benchmarks/node_server.js` — Node.js http module server
- `.sandbox/benchmarks/tml_server.tml` — TML std::http::App server
- `.sandbox/benchmarks/run_benchmarks.sh` — automated harness (warmup, measure, save)
- `.sandbox/benchmarks/results/` — raw JSON output from each run
- `docs/benchmarks/http-comparison.md` — final report with tables and analysis

## Success Criteria

- All four servers compile and handle all three endpoints correctly
- Benchmark script runs end-to-end without manual intervention
- Results collected at all four concurrency levels (1/10/100/500 connections)
- Gap analysis identifies at least 3 specific TML bottlenecks with supporting data
- Final report published in docs/benchmarks/ with reproduction instructions
- After Phase 4 fixes, TML reaches at least 50% of Go's req/s at 100 concurrent connections

## Dependencies

- Depends on: phase10-05-http-performance Phase 0 (regression fix) — benchmark numbers are
  meaningless at 8K req/s
- Blocks: phase10-07-db-http-integration Phase 3 (cross-language comparison uses this harness)
