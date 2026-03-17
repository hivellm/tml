# Tasks: HTTP Framework — 500K req/s Target

**Status**: IN_PROGRESS (March 2026) — Currently ~50K req/s

## Critical Bugs (must fix first)

- [ ] B.1 Fix GlobalModuleCache serving stale source code (use hash, not timestamp)
- [ ] B.2 Radix-tree dispatch returns 404 in multi-thread (caused by B.1)

## Phase 1: Zero-Allocation Hot Path (target: 150-200K req/s)

- [x] 1.1 Thread pool with mutex+condvar ring buffer
- [x] 1.2 Middleware pipeline
- [x] 1.3 Timeouts, graceful shutdown, queue overflow
- [x] 1.4 Router radix-tree + inline lookup
- [x] 1.5 Buffer response builder (single alloc)
- [x] 1.6 Zero-copy HTTP parsing (method/path)
- [x] 1.7 Connection: close, HEAD support, error recovery guards
- [ ] 1.8 Per-worker pre-allocated response buffer (eliminate mem_alloc per request)
- [ ] 1.9 Pre-computed status lines as constants ("HTTP/1.1 200 OK\r\n")
- [ ] 1.10 Inline I64-to-ASCII for Content-Length (no to_string() allocation)
- [ ] 1.11 Stack-allocated params_buf and ctx_data (no mem_alloc for radix tree dispatch)

## Phase 2: IOCP on Windows (target: 300-400K req/s)

- [ ] 2.1 C runtime: tml_iocp_create, tml_iocp_associate, tml_iocp_get_completion
- [ ] 2.2 C runtime: tml_iocp_post_accept (AcceptEx with pre-posted buffers)
- [ ] 2.3 C runtime: tml_iocp_post_recv (WSARecv with overlapped I/O)
- [ ] 2.4 C runtime: tml_iocp_post_send (WSASend with overlapped I/O)
- [ ] 2.5 TML: std::net::iocp module (IOCompletionPort, Completion, OverlappedBuffer)
- [ ] 2.6 TML: app.listen() uses IOCP backend (workers block on GetQueuedCompletionStatus)
- [ ] 2.7 Pre-post 4 AcceptEx calls at all times (async accept pipeline)
- [ ] 2.8 Per-connection pre-allocated recv buffer (zero kernel-copy on WSARecv)

## Phase 3: Connection Pipeline (target: 400-500K req/s)

- [ ] 3.1 Non-blocking send with per-connection write buffer
- [ ] 3.2 Scatter/gather I/O (WSASend with header+body buffers, no copy)
- [ ] 3.3 Pre-computed response cache for static routes
- [ ] 3.4 Lock-free MPSC queue per worker (replace mutex ring buffer)
- [ ] 3.5 Request pipelining support
- [ ] 3.6 Connection memory pool (slab allocator for fixed-size conn state)

## Phase 4: Production Hardening

- [ ] 4.1 Graceful shutdown with connection draining
- [ ] 4.2 Signal handler (Ctrl+C)
- [ ] 4.3 Access logging middleware
- [ ] 4.4 CORS middleware
- [ ] 4.5 JSON body parser middleware
- [ ] 4.6 Static file serving
- [ ] 4.7 Rate limiter middleware

## Done (reference)

- [x] Thread pool (32 workers), 50K req/s
- [x] EventLoop abstraction (std::net::eventloop)
- [x] WSAPoll/epoll event loop (22K req/s — limited by WSAPoll O(n))
- [x] Buffer response builder with Server: TML header
- [x] Radix-tree inline lookup (app_find_route)
- [x] AppContext.param() with fixed-size param buffer
- [x] Zero-copy method/path parsing
- [x] Connection: close detection
- [x] HEAD method support with GET fallback
- [x] Null fn_ptr guard (500 response)
- [x] Compiler: GlobalModuleCache timestamp validation (partial fix)
