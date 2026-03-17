# Tasks: HTTP Framework — Production-Ready

**Status**: IN_PROGRESS (March 2026)

## Phase 1: Wire Router + Buffer Responses (target: 80K req/s)

- [x] 1.1 Thread pool with mutex+condvar ring buffer
- [x] 1.2 Middleware pipeline (app.use_middleware)
- [x] 1.3 Read/write/idle timeouts via SO_RCVTIMEO/SO_SNDTIMEO
- [x] 1.4 Graceful shutdown flag in shared state
- [x] 1.5 AppContext.html(), .redirect() response helpers
- [x] 1.6 Queue overflow protection (503 when full)
- [x] 1.7 Wire Router radix-tree to app_dispatch (hybrid: exact match fast path + radix tree fallback)
- [x] 1.8 AppContext.param("name") working with RouteMatch (stack-allocated, zero heap)
- [x] 1.9 Buffer-based response builder (single alloc + copy_nonoverlapping)
- [x] 1.10 Pre-built common headers (Server: TML, Connection: keep-alive)
- [x] 1.11 Request size limits (BUF_SIZE=8KB header cap, MAX_BODY_SIZE=1MB constant)
- [ ] 1.12 Error recovery (catch handler panics, return 500)

## Phase 2: Zero-Copy Parsing + Arena (target: 150K req/s)

- [ ] 2.1 Zero-copy HTTP parser (parse method/path/headers as slices into recv buffer)
- [ ] 2.2 Arena allocator for per-request allocations (bulk free on request end)
- [ ] 2.3 writev() for scatter-gather response writes (headers + body without copy)
- [ ] 2.4 Pipelined request support (parse multiple requests from single recv)
- [ ] 2.5 Connection: close handling (honor client request)
- [ ] 2.6 Transfer-Encoding: chunked request body parsing
- [ ] 2.7 Expect: 100-continue support
- [ ] 2.8 HEAD method support (send headers only, no body)

## Phase 3: Async I/O (target: 400K req/s)

- [x] 3.1 WSAPoll/epoll wrapper (std::net::eventloop — EventLoop, Interest, Event)
- [x] 3.2 Event loop runtime (per-worker EventLoop with connection table)
- [x] 3.3 Async accept (non-blocking listener + round-robin distribution)
- [x] 3.4 Async recv (non-blocking read with header detection)
- [x] 3.5 Async send (non-blocking write with state machine buffering)
- [ ] 3.6 Timer wheel for timeout management (no per-socket setsockopt)
- [ ] 3.7 Work-stealing thread pool (multiple event loops)
- [ ] 3.8 Backpressure: pause accept when all workers busy

## Phase 4: Production Hardening

- [ ] 4.1 Graceful shutdown with connection draining (finish in-flight, reject new)
- [ ] 4.2 Signal handler (Ctrl+C triggers shutdown)
- [ ] 4.3 Access logging middleware (timestamp, method, path, status, duration)
- [ ] 4.4 Rate limiter middleware (token bucket per IP)
- [ ] 4.5 CORS middleware (preflight, allowed origins/methods/headers)
- [ ] 4.6 Static file serving (MIME types, ETag, Range)
- [ ] 4.7 JSON body parser middleware (auto-parse Content-Type: application/json)
- [ ] 4.8 Request ID middleware (X-Request-Id header)
- [ ] 4.9 Health check endpoint (/health with configurable checks)
- [ ] 4.10 Metrics endpoint (/metrics — connections, requests, latency histogram)

## Phase 5: TLS + HTTP/2

- [ ] 5.1 TLS via OpenSSL FFI (already have bindings in std::net::tls)
- [ ] 5.2 ALPN negotiation for HTTP/2
- [ ] 5.3 HTTP/2 framing (HPACK, streams, flow control)
- [ ] 5.4 Server push
- [ ] 5.5 Auto-redirect HTTP→HTTPS
