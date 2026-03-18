# HTTP Production Server — Generic HTTP/1.1 Implementation

Reference: nginx, tokio, Node.js http module, find-my-way router.
Goal: production-quality HTTP server AND client, not benchmark hacks.

## Phase 1: Fix Critical Bugs — MOSTLY DONE

- [x] 1.1 Fix routing — linear scan + radix tree for root (167K req/s)
- [ ] 1.1b Fix radix tree split corruption — node prefix overwritten after split
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

- [ ] 3.1 Fix Bool/i1 struct field codegen bug
- [ ] 3.2 Re-enable onRequest, preHandler, onSend, onResponse, onError hooks
- [ ] 3.3 Re-enable middleware pipeline in app_dispatch
- [ ] 3.4 Custom error handler support

## Phase 4: Event Loop Mode

- [x] 4.1 tml_sys_would_block() for non-blocking I/O
- [ ] 4.2 Fix event loop recv/send flow on Windows (WSAPoll + blocking send incompatibility)
- [ ] 4.3 Proper body accumulation in event loop mode
- [ ] 4.4 Multi-thread event loop with N pollers (Tokio model)

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
- [ ] 6.3 Connection: upgrade / WebSocket
- [ ] 6.4 X-Request-Id generation
- [ ] 6.5 Access logging (method, path, status, latency)

## Performance (2026-03-18)

| Scenario | TML std::http | Node.js http | Ratio |
|----------|--------------|-------------|-------|
| 100c, no pipeline | 48K | 52K | 0.92x |
| 100c, pipeline 10 | 95K | 67K | 1.42x |
| 100c, pipeline 100 | 169K | 94K | 1.80x |
| 200c, pipeline 100 | 178K | ~94K | 1.89x |

Codegen bugs fixed: 5 (extern declare, mul i32→i64, Unit call, kqueue, EWOULDBLOCK)
