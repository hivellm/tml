# Tasks: HTTP Performance Optimization — Target 500K req/s

**Status**: In Progress
**Baseline**: 183K bombardier, 6.5K autocannon (pipelining broken)
**Reference**: docs/analysis/http/00-strategic-plan.md

## Phase 1: Fix Pipelining (6.5K → 50K+ autocannon)

- [ ] 1.1 Replace null-termination in app_zerocopy_method with length-delimited return
- [ ] 1.2 Replace null-termination in app_zerocopy_path with length-delimited return
- [ ] 1.3 Update dispatch_single to use length-delimited method/path
- [ ] 1.4 Update app_dispatch to accept length-delimited method/path
- [ ] 1.5 Update app_is_known_method for length-delimited strings
- [ ] 1.6 Verify: 3 pipelined requests return 3x 200 OK (not 501)
- [ ] 1.7 Benchmark: autocannon -c 50 -p 10 → target 50K+
- [ ] 1.8 Agent review: code-reviewer validates no regressions
- [ ] 1.9 Agent consensus: all agents approve Phase 1 before proceeding

## Phase 2: Buffer & Allocation (50K → 70K autocannon)

- [ ] 2.1 Per-worker response buffer reuse (avoid mem_alloc per app_build_response)
- [ ] 2.2 Static response precomputation API: app.static_response(path, bytes)
- [ ] 2.3 Benchmark: autocannon -c 50 -p 10 → target 70K+
- [ ] 2.4 Agent review + consensus before Phase 3

## Phase 3: I/O Optimization (70K → 120K autocannon)

- [ ] 3.1 Add tml_sys_writev FFI binding for vectored I/O
- [ ] 3.2 Use writev in response path (header + body in 1 syscall)
- [ ] 3.3 Implement lock-free MPSC queue replacing mutex+condvar
- [ ] 3.4 Benchmark: autocannon -c 50 -p 10 → target 120K+
- [ ] 3.5 Agent review + consensus before Phase 4

## Phase 4: Architecture (120K → 200K autocannon)

- [ ] 4.1 Per-worker event loop with epoll (Linux) or IOCP (Windows)
- [ ] 4.2 Each worker does own accept + non-blocking I/O
- [ ] 4.3 Fix IOCP mode (currently 13K, target 200K+)
- [ ] 4.4 Benchmark: autocannon -c 50 -p 10 → target 200K+
- [ ] 4.5 Agent review + consensus before Phase 5

## Phase 5: Parser Optimization (200K → 250K+ autocannon)

- [ ] 5.1 SIMD-optimized scanning for CRLF and colon in headers
- [ ] 5.2 Benchmark: autocannon -c 50 -p 10 → target 250K+
- [ ] 5.3 Final agent review + consensus
- [ ] 5.4 Update docs/benchmarks with final comparison table
