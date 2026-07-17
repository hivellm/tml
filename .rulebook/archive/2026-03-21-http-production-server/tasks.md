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
- [x] 1.6 SO_RCVTIMEO/SO_SNDTIMEO — already fixed in C runtime (struct timeval on POSIX, DWORD on Windows)
- [x] 1.7 Fix graceful shutdown — condvar_notify_all to wake blocked workers
- [x] 1.8 IncomingMessage::new() — hot path uses direct struct construction, no dangling pointer
- [x] 1.9 Fix TRACE/PATCH method_index conflict — checks first char now

## Phase 2: Proper HTTP/1.1 Compliance

- [x] 2.1 Proper request body accumulation — recv loop until Content-Length bytes received
- [x] 2.2 Chunked transfer-encoding decoding — decode_chunked + recv_chunked_body + worker integration
- [x] 2.3 `Expect: 100-continue` — sends 100 Continue before body reading
- [x] 2.4 `Date:` header — app_build_response includes Date, 404/501 now use it in app_dispatch
- [x] 2.5 400 Bad Request for malformed requests / oversized headers
- [x] 2.6 405 Method Not Allowed with `Allow:` header — probes all 7 method trees
- [x] 2.7 501 Not Implemented for unknown methods
- [x] 2.8 URL percent-decoding — already implemented in body_parser.tml + parse.tml, 14 tests added
- [x] 2.9 Idle timeout enforcement — SO_RCVTIMEO set on accepted connections from shared state
- [x] 2.10 Connection: close handling per HTTP/1.1 spec

## Phase 3: Enable Middleware & Hooks

- [x] 3.1 Fix Bool/i1 struct field codegen bug — IncomingMessage + ServerResponse Bool→I64
- [x] 3.2 Re-enable onRequest, preHandler, onResponse hooks — wired into app_dispatch
- [x] 3.3 Re-enable middleware pipeline — hooks called at correct lifecycle points
- [x] 3.4 Custom error handler + onError hooks — wired into 404 path in app_dispatch

## Phase 4: Event Loop Mode

- [x] 4.1 tml_sys_would_block() for non-blocking I/O
- [x] 4.2 Event loop send — already non-blocking with partial write tracking (CONN_RESP_SENT)
- [x] 4.3 Body accumulation — already implemented with chunked detection in event loop worker
- [x] 4.4 Multi-thread event loop — N workers with round-robin accept distribution

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
- [x] 4b.11 IOCP accept pool 64→256 — prevents exhaustion under high connection rate
- [x] 4b.12 IOCP slot scan O(1) amortized — g_slot_hint skips occupied slots

## Phase 5: HTTP Client — ALREADY IMPLEMENTED

- [x] 5.1 HTTP client request building — HttpClient with get/post/put/delete/head
- [x] 5.2 HTTP client response parsing — Response::parse, read_all with Buffer
- [x] 5.3 Connection pool — ConnectionPool data structure with take/put (conn_pool.tml)
- [x] 5.4 Chunked transfer-encoding for client — Response::parse decodes chunked bodies
- [x] 5.5 Redirect following — recursive follow_redirects, 301/302/303/307/308, max_redirects config
- [x] 5.6 Timeout support — timeout_ms field, builder API

## Phase 6: Production Hardening

- [x] 6.1 SO_REUSEPORT — enabled on Linux via conditional compilation
- [x] 6.2 Multi-value headers — Headers::append() joins with ", " per RFC 7230
- [x] 6.3 WebSocket frame parser — 821 lines in websocket.tml (already implemented)
- [x] 6.4 X-Request-Id — generate_request_id() with hex(timestamp)-hex(counter)
- [x] 6.5 Access logging — app_log_request(method, path, status, latency_us)

## Phase 7: Memory & Buffer Optimization (from comparative analysis 2026-03-19)

Reference: docs/analyses/comparative-analysis.md

- [x] 7.1 Reduce recv buffer 64KB → 8KB with dynamic growth (IOCP: INITIAL_BUF_SIZE=8KB, grows 2x to 64KB)
- [x] 7.2 Per-worker arena allocator — Arena type with bump alloc, O(1) reset (arena.tml)
- [x] 7.3 Dynamic IOCP slots — start at 4K (352KB) instead of 64K (5.6MB)
- [x] 7.4 Bytes type — ref-counted immutable byte buffer with slice/clone/release (bytes.tml)
- [x] 7.5 Backpressure — implicit: recv paused during send (state machine already enforces this)

## Phase 8: Performance Instrumentation

- [x] 8.1 Latency measurement — per-worker time_ns() tracking (min/max/avg_us) in thread pool worker
- [x] 8.2 Timeout enforcement — read/write/idle via SO_RCVTIMEO/SO_SNDTIMEO in worker accept loop
- [x] 8.3 Work-stealing — WorkStealingScheduler with per-worker LIFO queues + try_steal (work_stealing.tml)
- [x] 8.4 Graceful shutdown with draining — 3-phase: stop accepting → drain queue → wake workers

## Phase 9: TLS & HTTP/2

- [x] 9.1 TLS — already implemented in std::net::tls (497 lines, used by HttpClient)
- [x] 9.2 ALPN — TlsContext::set_alpn_protocols() already exists
- [x] 9.3 HTTP/2 frame parser (9-byte header, binary protocol) — lib/std/src/http/h2/frame.tml
- [x] 9.4 HTTP/2 stream multiplexer (SETTINGS, HEADERS, DATA, RST_STREAM, GOAWAY) — lib/std/src/http/h2/stream.tml + connection.tml
- [x] 9.5 HPACK header compression (dynamic table) — lib/std/src/http/h2/hpack.tml
- [x] 9.6 HTTP/2 flow control (WINDOW_UPDATE, per-stream + per-connection windows) — tested in h2_flow_control.test.tml
- [x] 9.7 HTTP/2 server integration (preface validation, request extraction, response building) — lib/std/src/http/h2/server.tml

## Phase 10: Advanced Optimizations

- [x] 10.1 SIMD-ready header parsing — scalar find_crlf/colon/space/header_end + SIMD API ready (simd_parse.tml)
- [x] 10.2 sendfile — send_static_response for pre-loaded content, send_file_to_socket API ready
- [x] 10.3 Vectored I/O — send_two/send_response_parts (writev FFI fallback, vectored_io.tml)
- [x] 10.4 Offset-based parsing — current zero-copy approach already avoids data copies (null-terminate in-place)

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
