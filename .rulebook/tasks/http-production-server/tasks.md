# HTTP Production Server — Generic HTTP/1.1 Implementation

Reference: nginx, tokio, Node.js http module.
Goal: production-quality HTTP server AND client, not benchmark hacks.

## Phase 1: Fix Critical Bugs (server doesn't work)

- [x] 1.1 Fix routing — all routes work via linear scan (radix tree deferred)
- [ ] 1.1b Fix radix tree split corruption — node prefix overwritten after split
- [ ] 1.2 Parse ALL headers in worker hot path — handlers need Content-Type, Authorization, etc.
- [ ] 1.3 Fix Content-Length body reading — read exact bytes, don't just take whatever's in buffer
- [ ] 1.4 Fix pipelining body boundary — `\r\n\r\n` inside POST body miscounted as request boundary
- [ ] 1.5 Fix query string — preserve `?foo=bar` in IncomingMessage for handlers
- [ ] 1.6 Fix SO_RCVTIMEO/SO_SNDTIMEO — passes I32 but POSIX needs struct timeval
- [ ] 1.7 Fix graceful shutdown — condvar_notify_all to wake blocked workers
- [ ] 1.8 Fix IncomingMessage::new() dangling pointer — Headers freed when stack var goes out of scope
- [ ] 1.9 Fix TRACE/PATCH method_index conflict — both length 5, TRACE misidentified as PATCH

## Phase 2: Proper HTTP/1.1 Compliance

- [ ] 2.1 Proper request body accumulation — recv loop until Content-Length bytes received
- [ ] 2.2 Chunked transfer-encoding decoding for request bodies
- [ ] 2.3 `Expect: 100-continue` handling — send 100 before reading large bodies
- [ ] 2.4 `Date:` header in all responses (RFC 7231 §7.1.1.2)
- [ ] 2.5 400 Bad Request for malformed requests / oversized headers
- [ ] 2.6 405 Method Not Allowed with `Allow:` header (RFC 7231)
- [ ] 2.7 501 Not Implemented for unknown methods (CONNECT, LOCK, etc.)
- [ ] 2.8 URL percent-decoding before routing (`%20` → space)
- [ ] 2.9 Idle timeout enforcement between keep-alive requests
- [ ] 2.10 Connection: close handling per HTTP/1.1 spec (1.1 default keep-alive, 1.0 default close)

## Phase 3: Enable Middleware & Hooks

- [ ] 3.1 Fix Bool/i1 struct field codegen bug — ServerResponse can't pass through fn ptrs
- [ ] 3.2 Re-enable onRequest, preHandler, onSend, onResponse, onError hooks
- [ ] 3.3 Re-enable middleware pipeline in app_dispatch
- [ ] 3.4 Custom error handler support

## Phase 4: Event Loop Mode

- [ ] 4.1 Fix EWOULDBLOCK handling in non-blocking recv
- [ ] 4.2 Fix event loop pipelining — don't discard buffered bytes on state reset
- [ ] 4.3 Proper body accumulation in event loop mode
- [ ] 4.4 Multi-thread event loop with N pollers (Tokio model)

## Phase 5: HTTP Client

- [ ] 5.1 HTTP client request building (method, headers, body)
- [ ] 5.2 HTTP client response parsing (status, headers, body)
- [ ] 5.3 Connection pooling (keep-alive reuse)
- [ ] 5.4 Chunked transfer-encoding for client responses
- [ ] 5.5 Redirect following (301, 302, 307, 308)
- [ ] 5.6 Timeout support (connect, read, total)

## Phase 6: Production Hardening

- [ ] 6.1 SO_REUSEPORT for zero-downtime restarts
- [ ] 6.2 Multi-value header support (Set-Cookie, Vary)
- [ ] 6.3 Trailer headers in chunked responses
- [ ] 6.4 Connection: upgrade / WebSocket handshake
- [ ] 6.5 X-Request-Id generation for tracing
- [ ] 6.6 Proper error pages (HTML 404/500 for browsers)
- [ ] 6.7 Access logging (method, path, status, latency)
