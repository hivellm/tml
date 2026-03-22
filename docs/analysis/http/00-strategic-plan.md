# HTTP Performance Strategic Plan — TML vs Industry

## Current State (2026-03-22)

| Metric | TML | Tokio/hyper | Node.js (cluster) | Go net/http | Nginx |
|--------|-----|-------------|-------------------|-------------|-------|
| Plaintext (bombardier 50c) | **183K** | ~200K | **139K** | 16K | 300K+ |
| Plaintext (autocannon 50c p10) | **6.5K** | **53K** | **77K** | 16K | N/A |
| Pipelining | BROKEN | Full | Full | Partial | Full |
| I/O Model | Thread pool + mutex | async/epoll | Event loop + cluster | Goroutine/netpoller | Worker + epoll |
| Buffer Reuse | None (alloc per req) | BytesMut pool | slab allocator | sync.Pool | Pre-allocated |
| HTTP Parser | Custom (byte scan) | httparse (SIMD) | llhttp (state machine) | Custom | Custom (zero-copy) |
| Zero-copy | Partial (zerocopy method/path) | Full (httparse) | Partial | Partial | Full |

## Top 10 Recommendations (Priority Order)

### 1. FIX PIPELINING BUG (Critical — 6.5K → 50K+)
**Impact**: 8x improvement with autocannon
**Effort**: Medium
**Root Cause**: `dispatch_single` null-terminates at `buf[req_start + req_len]` which overwrites the first byte of the next pipelined request. The `app_zerocopy_method` then fails to parse the corrupted method, returning 501.
**Fix**: Either (a) use a separate copy of each request for dispatch, or (b) don't null-terminate — pass explicit length to all parsing functions instead of relying on C-string null termination.
**Pattern**: Tokio/hyper uses `httparse::Request::parse(&buf[..])` with explicit slice — never null-terminates.

### 2. BUFFER POOL (High — reduce alloc overhead)
**Impact**: 20-30% improvement
**Effort**: Medium
**What**: Pre-allocate N buffers per worker (recv buf, resp buf, params buf) and reuse across requests. Currently each worker allocates once at startup — this is already good. But `app_build_response` allocates a new string per response via `mem_alloc`.
**Pattern**: Go uses `sync.Pool` for `bufio.Reader/Writer`. Tokio uses `BytesMut` with inline storage.
**Fix**: Add a per-worker response buffer that gets reused instead of allocating per response.

### 3. ELIMINATE NULL-TERMINATION IN PARSER (High — enables true zero-copy)
**Impact**: 15-20% improvement + enables pipelining
**Effort**: Large
**What**: Current parser (`app_zerocopy_method`, `app_zerocopy_path`) null-terminates the recv buffer in-place. This is destructive and prevents pipelining. All parsing should use `(ptr, offset, len)` tuples instead of `Str` (which requires null termination).
**Pattern**: Every production HTTP parser (httparse, llhttp, picohttpparser, nginx) uses length-delimited parsing, never null-termination.

### 4. REDUCE SYSCALLS WITH WRITEV (Medium — batch header+body)
**Impact**: 10-15% improvement
**Effort**: Medium
**What**: Currently each `send()` is a separate syscall. Using `writev()` (vectored I/O) can send header+body in a single syscall.
**Pattern**: Nginx uses `writev` for response headers + body. Go buffers header+body before write.
**Fix**: Add `tml_sys_writev` FFI binding and use in response path.

### 5. LOCK-FREE QUEUE (Medium — reduce mutex contention)
**Impact**: 10-15% improvement at high concurrency
**Effort**: Medium
**What**: Replace `mutex + condvar` queue with MPSC lock-free queue using atomic CAS.
**Pattern**: Tokio uses `crossbeam` deque. Go uses a lock-free run queue per P.
**Fix**: Implement lock-free ring buffer with atomic head/tail.

### 6. RESPONSE PRECOMPUTATION (Medium — static content fast path)
**Impact**: 20% for static responses (already done for benchmark)
**Effort**: Small
**What**: Already implemented with `const RESP_PLAINTEXT`. Extend to framework: `app.static_response("/path", response_bytes)`.

### 7. ACCEPT-PER-WORKER WITH EPOLL (Medium-High)
**Impact**: 30-50% at high concurrency
**Effort**: Large
**What**: Instead of 1 accept thread + mutex queue, each worker has its own epoll/IOCP and does its own accept. This is the Nginx/Go model.
**Pattern**: Nginx workers each call `epoll_wait` + `accept`. Go netpoller distributes across P.
**Note**: Our earlier accept-per-worker attempt with blocking I/O regressed. The correct approach needs non-blocking I/O (epoll/IOCP).

### 8. HTTP PARSER OPTIMIZATION (Low-Medium)
**Impact**: 5-10%
**Effort**: Medium
**What**: Current parser scans byte-by-byte with `rd(buf, i)`. SIMD-optimized scanning for `\r\n` and `:` can process 16 bytes at once.
**Pattern**: httparse uses SIMD for header scanning. picohttpparser uses SSE4.2.

### 9. CONNECTION KEEP-ALIVE OPTIMIZATION (Low)
**Impact**: 5%
**Effort**: Small
**What**: Set idle timeout via SO_RCVTIMEO (already done). Also: reuse the same buffer/params across keep-alive requests on the same connection (already done).

### 10. IOCP MODE FIX (Windows-specific, High)
**Impact**: Potentially 2x on Windows
**Effort**: Large
**What**: Current IOCP mode gets 13K (vs 183K thread pool). Need to use `GetQueuedCompletionStatusEx` (batch dequeue), fix the completion handling, and optimize the accept pool.
**Pattern**: Tokio on Windows uses IOCP with `mio::Poll` wrapping AFD (not direct IOCP).

## Implementation Order

```
Phase 1: Fix Pipelining (1-2 days)
  └─ #1 Fix null-termination bug
  └─ #3 Length-delimited parsing (partial — method/path first)
  └─ Re-benchmark: target 50K+ autocannon

Phase 2: Buffer & Allocation (1-2 days)
  └─ #2 Per-worker response buffer pool
  └─ #6 Static response precomputation API
  └─ Re-benchmark: target 60K+ autocannon, 200K+ bombardier

Phase 3: I/O Optimization (2-3 days)
  └─ #4 writev for response batching
  └─ #5 Lock-free MPSC queue
  └─ Re-benchmark: target 100K+ autocannon

Phase 4: Architecture (3-5 days)
  └─ #7 Per-worker event loop (epoll on Linux, IOCP on Windows)
  └─ #10 Fix IOCP mode
  └─ Re-benchmark: target 200K+ autocannon, 400K+ bombardier

Phase 5: Parser Optimization (2-3 days)
  └─ #8 SIMD header scanning
  └─ Re-benchmark: target 250K+ autocannon, 500K+ bombardier
```

## Target Milestones

| Milestone | Autocannon (50c, p10) | Bombardier (50c) | Key Change |
|-----------|----------------------|-------------------|------------|
| Current | 6.5K | 183K | Baseline |
| After Phase 1 | 50K | 185K | Pipelining fix |
| After Phase 2 | 70K | 200K | Buffer reuse |
| After Phase 3 | 120K | 250K | Lock-free + writev |
| After Phase 4 | 200K | 400K | Event-driven I/O |
| After Phase 5 | 250K+ | 500K+ | SIMD parser |

## Key Industry Patterns TML Should Adopt

1. **Length-delimited parsing** — Never null-terminate network buffers
2. **Buffer pools** — Reuse allocations across requests (sync.Pool / BytesMut)
3. **Batch I/O** — writev for multi-buffer sends, read-ahead for pipelining
4. **Per-core event loop** — Each worker owns its I/O, no shared queue
5. **Zero-copy responses** — Static content served from pre-computed buffers
