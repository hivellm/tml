# Tasks: HTTP Production Benchmark — TML vs Tokio vs Go vs Node

**Status**: In Progress

## Phase 1: Reference Servers

- [ ] 1.1 Write Go benchmark server (net/http, stdlib only)
- [ ] 1.2 Write Rust/Tokio benchmark server (axum or hyper)
- [ ] 1.3 Write Node.js benchmark server (http module, no framework)
- [ ] 1.4 Write TML benchmark server (std::http::App)
- [ ] 1.5 All servers: JSON `{"message":"Hello, World!"}` on GET /json
- [ ] 1.6 All servers: plaintext "Hello, World!" on GET /plaintext
- [ ] 1.7 All servers: 1KB body echo on POST /echo

## Phase 2: Benchmark Harness

- [ ] 2.1 Install wrk or bombardier for HTTP load testing
- [ ] 2.2 Create benchmark script: 10s warmup + 30s measurement
- [ ] 2.3 Test scenarios: 1/10/100/500 concurrent connections
- [ ] 2.4 Test scenarios: with and without pipelining
- [ ] 2.5 Collect: req/s, latency p50/p99/p999, errors
- [ ] 2.6 Save results to .sandbox/benchmarks/

## Phase 3: Gap Analysis

- [ ] 3.1 Identify TML bottlenecks from benchmark results
- [ ] 3.2 Profile TML server with time_ns() instrumentation
- [ ] 3.3 Compare per-request allocation count
- [ ] 3.4 Document gaps

## Phase 4: Fix TML HTTP Gaps

- [ ] 4.1 Reduce per-request allocations (reuse buffers, arena)
- [ ] 4.2 Optimize response serialization (pre-compute headers)
- [ ] 4.3 Fix IOCP inefficiencies from profiling
- [ ] 4.4 Optimize router hot path
- [ ] 4.5 Reduce syscall count per request
- [ ] 4.6 Connection reuse optimization

## Phase 5: Final Report

- [ ] 5.1 Run all benchmarks after fixes
- [ ] 5.2 Create comparison table in docs/benchmarks/
- [ ] 5.3 Document reproduction steps

## 1. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 1.1 Update or create documentation covering the implementation
- [ ] 1.2 Write tests covering the new behavior
- [ ] 1.3 Run tests and confirm they pass
