# Tasks: HTTP Performance Optimization — Target 500K req/s

**Status**: In Progress — Phase 1 pipelining FIXED, performance regression under investigation
**Current**: 14K autocannon (was 6.5K broken), 8K bombardier (was 183K — REGRESSION)
**Reference**: docs/analysis/http/00-strategic-plan.md

## Phase 0: Fix Performance Regression (CRITICAL)

- [ ] 0.1 Identify why bombardier dropped from 183K to 8K after profiler instrumentation
- [ ] 0.2 Remove profiler::begin/end from ALL stdlib hot paths (str, option, hashmap, etc.)
- [ ] 0.3 Keep profiler only in non-hot-path code (file I/O, crypto, net connect)
- [ ] 0.4 Verify bombardier returns to 180K+ baseline
- [ ] 0.5 Verify autocannon pipelining still works (3x 200 OK)

## Phase 1: Fix Pipelining — DONE

- [x] 1.1 app_copy_method/app_copy_path — non-destructive parsing for batch path
- [x] 1.2 app_zerocopy_method/app_zerocopy_path — restored fast destructive for single path
- [x] 1.3 dispatch_single uses copy methods (safe for pipelining)
- [x] 1.4 Single-request path inlined with zero-copy destructive parsing
- [x] 1.5 Simplified sequential pipeline loop (removed broken batch code)
- [x] 1.6 Verified: 3 pipelined requests return 3x 200 OK
- [x] 1.7 Autocannon 50c p10: 14K (was 6.5K broken)

## Phase 2: Buffer & Allocation

- [ ] 2.1 Per-worker response buffer reuse
- [ ] 2.2 Static response precomputation API
- [ ] 2.3 Benchmark target: 70K+ autocannon

## Phase 3: I/O Optimization

- [ ] 3.1 writev FFI for vectored I/O
- [ ] 3.2 Lock-free MPSC queue
- [ ] 3.3 Benchmark target: 120K+ autocannon

## Phase 4: Architecture

- [ ] 4.1 Per-worker event loop (epoll/IOCP)
- [ ] 4.2 Fix IOCP mode (currently 13K)
- [ ] 4.3 Benchmark target: 200K+ autocannon

## Phase 5: Parser Optimization

- [ ] 5.1 SIMD scanning for CRLF/colon
- [ ] 5.2 Benchmark target: 250K+ autocannon
