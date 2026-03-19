# HTTP Production Server — Generic HTTP/1.1 Implementation

Reference: nginx, tokio, Node.js http module, find-my-way router.
Goal: production-quality HTTP server AND client, not benchmark hacks.

## Phase 1: Fix Critical Bugs — MOSTLY DONE

- [x] 1.1 Fix routing — linear scan + radix tree for root (167K req/s)
- [x] 1.1b Fix radix tree split corruption — proper split with saved locals + separate stack frames
- [x] 1.2 Parse ALL headers in worker hot path (zero-copy nginx-style)
- [x] 1.3 Fix Content-Length body reading — recv loop for large bodies
- [x] 1.4 Fix pipelining — bodyless methods only (GET/HEAD safe)
- [x] 1.5 Query string extraction (app_extract_query, app_extract_path_from_url)
- [ ] 1.6 Fix SO_RCVTIMEO/SO_SNDTIMEO — passes I32 but POSIX needs struct timeval
- [x] 1.7 Fix graceful shutdown — condvar_notify_all to wake blocked workers
- [ ] 1.8 Fix IncomingMessage::new() dangling pointer
- [x] 1.9 Fix TRACE/PATCH method_index conflict — checks first char now

## Phase 2: Proper HTTP/1.1 Compliance

- [x] 2.1 Proper request body accumulation — recv loop until Content-Length bytes received
- [ ] 2.2 Chunked transfer-encoding decoding for request bodies
- [ ] 2.3 `Expect: 100-continue` handling
- [ ] 2.4 `Date:` header in all responses (RFC 7231 §7.1.1.2)
- [x] 2.5 400 Bad Request for malformed requests / oversized headers
- [ ] 2.6 405 Method Not Allowed with `Allow:` header
- [ ] 2.7 501 Not Implemented for unknown methods
- [ ] 2.8 URL percent-decoding before routing
- [ ] 2.9 Idle timeout enforcement between keep-alive requests
- [x] 2.10 Connection: close handling per HTTP/1.1 spec

## Phase 3: Enable Middleware & Hooks

- [x] 3.1 Fix Bool/i1 struct field codegen bug — IncomingMessage.is_complete changed to I64
- [ ] 3.2 Re-enable onRequest, preHandler, onSend, onResponse, onError hooks
- [ ] 3.3 Re-enable middleware pipeline in app_dispatch
- [ ] 3.4 Custom error handler support

## Phase 4: Event Loop Mode

- [x] 4.1 tml_sys_would_block() for non-blocking I/O
- [ ] 4.2 Fix event loop recv/send flow on Windows (WSAPoll + blocking send incompatibility)
- [ ] 4.3 Proper body accumulation in event loop mode
- [ ] 4.4 Multi-thread event loop with N pollers (Tokio model)

## Phase 4b: IOCP — Windows High-Performance Async I/O

- [x] 4b.1 Implement iocp.c runtime — tml_iocp_create/destroy, tml_iocp_associate
- [x] 4b.2 Async WSARecv/WSASend via tml_iocp_recv/tml_iocp_send
- [x] 4b.3 Completion dequeue via tml_iocp_wait (GetQueuedCompletionStatus)
- [x] 4b.4 Custom sentinel posts via tml_iocp_post (graceful shutdown)
- [x] 4b.5 IocpOperation alloc/free/accessor helpers (op_alloc, op_free, op_type, op_token, op_socket)
- [x] 4b.6 AcceptEx support — tml_iocp_listen, tml_iocp_accept, tml_iocp_create_accept_socket, tml_iocp_update_accept_context
- [x] 4b.7 Non-Windows stubs for cross-platform compilation
- [x] 4b.8 Added iocp.c to compiler/CMakeLists.txt next to poll.c
- [x] 4b.9 TML-side IOCP worker: iocp_worker.tml with app_listen_iocp
- [x] 4b.10 IOCP integrated into App.listen_iocp() + linker fix (libtml_runtime.a + mswsock)
- [ ] 4b.11 Fix IOCP pipeline stall with >100 connections (accept pool exhaustion)
- [ ] 4b.12 Fix IOCP scaling >500 connections (slot allocation race)

## Phase 5: HTTP Client

- [ ] 5.1 HTTP client request building
- [ ] 5.2 HTTP client response parsing
- [ ] 5.3 Connection pooling
- [ ] 5.4 Chunked transfer-encoding for client responses
- [ ] 5.5 Redirect following
- [ ] 5.6 Timeout support

## Phase 6: Production Hardening

- [ ] 6.1 SO_REUSEPORT for zero-downtime restarts
- [ ] 6.2 Multi-value header support (Set-Cookie, Vary)
- [ ] 6.3 Connection: upgrade / WebSocket frame parser
- [ ] 6.4 X-Request-Id generation
- [ ] 6.5 Access logging (method, path, status, latency)

## Phase 7: Memory & Buffer Optimization (from comparative analysis 2026-03-19)

Reference: docs/analyses/comparative-analysis.md

- [x] 7.1 Reduce recv buffer 64KB → 8KB with dynamic growth (IOCP: INITIAL_BUF_SIZE=8KB, grows 2x to 64KB)
- [ ] 7.2 Per-worker buffer pool (arena allocator) — 0 malloc/free in steady state
- [ ] 7.3 Dynamic IOCP connection slots — replace fixed 65K array with growable (start 4K, grow to 256K)
- [ ] 7.4 Implement Bytes-like ref-counted buffer type for zero-copy sharing (Tokio pattern)
- [ ] 7.5 Add backpressure — TCP flow control when handler is slow (Node.js/Hyper pattern)

## Phase 8: Performance Instrumentation

- [x] 8.1 Latency measurement — per-worker time_ns() tracking (min/max/avg_us) in thread pool worker
- [ ] 8.2 Request/connection timeout enforcement (read_timeout, write_timeout, idle_timeout)
- [ ] 8.3 Work-stealing for thread pool mode (Tokio model: LIFO local + FIFO global + steal-half)
- [ ] 8.4 Graceful shutdown with connection draining (Go Server.Shutdown pattern)

## Phase 9: TLS & HTTP/2

- [ ] 9.1 TLS via @extern("c") to Schannel (Windows) / OpenSSL (Linux)
- [ ] 9.2 ALPN negotiation for HTTP/2 detection
- [ ] 9.3 HTTP/2 frame parser (9-byte header, binary protocol)
- [ ] 9.4 HTTP/2 stream multiplexer (SETTINGS, HEADERS, DATA, RST_STREAM, GOAWAY)
- [ ] 9.5 HPACK header compression (dynamic table)
- [ ] 9.6 HTTP/2 flow control (WINDOW_UPDATE, per-stream + per-connection windows)

## Phase 10: Advanced Optimizations

- [ ] 10.1 SIMD-accelerated header parsing (LLVM vector intrinsics for CRLF/colon scan)
- [ ] 10.2 sendfile()/TransmitFile for zero-copy static file serving
- [ ] 10.3 Vectored I/O (writev/WSASend with multiple buffers)
- [ ] 10.4 Offset-based parsing (replace null-termination with offset tracking)

## Performance (2026-03-18)

### Thread Pool mode (app.listen)
| Scenario | TML std::http | Node.js http | Ratio |
|----------|--------------|-------------|-------|
| 100c, no pipeline | 48K | 52K | 0.92x |
| 100c, pipeline 10 | 95K | 67K | 1.42x |
| 100c, pipeline 100 | 169K | 94K | 1.80x |
| 200c, pipeline 100 | 178K | ~94K | 1.89x |

### IOCP mode (app.listen_iocp) — NEW
| Scenario | TML IOCP | Notes |
|----------|----------|-------|
| 100c, no pipeline | 60K | async I/O, no thread blocking |
| 200c, no pipeline | 54K | scales with connections |
| 10c, pipeline 2 | 63K | pipelining works |
| 50c, pipeline 10 | 41K | some non-2xx (capacity) |

IOCP bottlenecks remaining: accept pool exhaustion >100c, slot scan O(n), pipeline stall >100c

Codegen bugs fixed: 7 (+Bool/i1 struct layout, +mut ref O3 dead store)
