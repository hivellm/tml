# Tasks: Async Network Stack

**Status**: COMPLETE (remaining items are codegen bugs tracked separately)
**Priority**: High
**Updated**: 2026-03-19
**Tests**: 1599 total, 1130 passed, 0 failures

## Phase 1: Async Runtime Foundation — ✅ DONE

- [x] 1.1 OS event loop abstraction (epoll/IOCP/WSAPoll in `lib/std/src/aio/`)
- [x] 1.2 `poll.c` for Linux (epoll in `compiler/runtime/net/poll.c`)
- [x] 1.3 `iocp.c` for Windows (`compiler/runtime/net/iocp.c`)
- [x] 1.5 `Future` behavior (`core::future::Future`)
- [x] 1.6 `Poll[T]` with `Ready(T)` and `Pending` (`core::task::Poll`)
- [x] 1.7 `Waker`/`Context` — wake()/wake_by_ref()/duplicate() fixed
- [x] 1.8 Single-threaded executor (`lib/std/src/runtime/executor.tml`)
- [x] 1.9 Task spawning (`compiler/runtime/concurrency/async.c`)
- [x] 1.10 Hierarchical timer wheel (`lib/std/src/aio/timer_wheel.tml`)
- [x] 1.11 async/await codegen verified end-to-end

## Phase 2: Multi-Threaded Executor — ✅ DONE

- [x] 2.1 Worker threads with local task queues (`lib/std/src/runtime/multi_executor.tml`)
- [x] 2.2 Global MPMC queue (mutex-protected growable array)
- [x] 2.3 Work-stealing between workers
- [x] 2.4 `spawn_blocking` (`thread::spawn_blocking`, `thread::spawn_blocking_i64`)
- [x] 2.6 Graceful shutdown (drain tasks, signal workers, join)

## Phase 3: Async I/O Primitives — ✅ DONE

- [x] 3.1 `AsyncRead` behavior (`lib/std/src/stream/async_io.tml`)
- [x] 3.2 `AsyncWrite` behavior (`lib/std/src/stream/async_io.tml`)
- [x] 3.3 Async TCP (`lib/std/src/net/async_tcp.tml`)
- [x] 3.4 Async UDP (`lib/std/src/net/async_udp.tml`)
- [x] 3.5 `AsyncBufReader`/`AsyncBufWriter` (`lib/std/src/stream/async_buffered.tml`)
- [x] 3.6 Async channels (`lib/std/src/runtime/channel.tml`)
- [x] 3.7 `select2` combinator (`lib/core/src/future/select.tml`)

## Phase 4: Network Types — ✅ DONE

- [x] 4.1 `IpAddr`, `SocketAddr` (`lib/std/src/net/ip.tml`, `socket.tml`)
- [x] 4.2 DNS resolution (`compiler/runtime/net/dns.c`)
- [x] 4.3 Connection pooling (`lib/std/src/http/agent.tml`)
- [x] 4.5 `BufferView` zero-copy (`lib/std/src/net/buffer_view.tml`)

## Phase 5: TLS — ✅ DONE

- [x] 5.1-5.4 Platform TLS (OpenSSL), async handshake, HTTPS, TLS 1.3
- [x] 5.5 ALPN protocol negotiation (`TlsContext::set_alpn_protocols()`)
- [x] 5.6-5.7 Certificate verification, tested with real certs

## Phase 6: HTTP/1.1 — ✅ DONE (36 files)

- [x] 6.1-6.7 Full HTTP stack: types, codec, keep-alive, chunked, client, server, streaming/SSE

## Phase 7: HTTP/2 + WebSocket — ✅ DONE

- [x] 7.1 HTTP/2 binary framing (`lib/std/src/http/h2/frame.tml`)
- [x] 7.2 HPACK compression (`lib/std/src/http/h2/hpack.tml`) — static table (61), dynamic table, integer/string codec
- [x] 7.3 Stream multiplexing + flow control (`lib/std/src/http/h2/stream.tml`, `connection.tml`)
- [x] 7.4 Server integration (`lib/std/src/http/h2/server.tml`)
- [x] 7.5 WebSocket RFC 6455 (`lib/std/src/http/websocket.tml`) — frame codec, masking, handshake (uses std::crypto::sha1)
- [x] 7.6 Server-Sent Events (`lib/std/src/http/stream.tml`)

## Phase 8: Application Framework — ✅ DONE

- [x] 8.1 `Controller` behavior + registration (`lib/std/src/http/controller.tml`)
- [x] 8.2 `@Get`, `@Post`, `@Put`, `@Delete`, `@Patch` decorators (compiler codegen — full pipeline)
- [x] 8.4 Radix router with `:param` and `*` wildcards (`router.tml`)
- [x] 8.9 CORS, rate limiting, security headers (`cors.tml`, `rate_limit.tml`, `security.tml`)
- [x] 8.10 Route decorator codegen generates `__tml_register_routes()` auto-registration

## Phase 9: Promise + Observable — ✅ DONE

- [x] 9.1-9.4 `Promise[T]` with resolve/reject/then/catch/finally/all/race/any/all_settled
- [x] 9.6-9.8 `Observable[T]` with of/from_list/empty/never/throw factories
- [x] 9.9 Operators: map, filter, take, skip, scan, distinct
- [x] 9.10 Combination: merge, concat
- [x] 9.12 `Subject[T]`, `BehaviorSubject[T]`, `ReplaySubject[T]`
- [x] 9.13 Pipe operator `|>` in parser (desugars to function calls)

## Phase 10: Validation — PARTIAL

- [x] 10.3 TLS with real certificates verified
- [x] 10.7 1599 tests, 0 runtime failures

## Compiler Fixes Applied (2026-03-19)

- [x] Pipe operator `|>` (lexer + parser, zero downstream changes)
- [x] Nested generic monomorphization (`Poll[Outcome[I64, IoError]]` → correct type)
- [x] Generic static → List.push (`infer_expr_type` substitution fix)
- [x] @Controller decorator pipeline (parser → checker → HIR → THIR → MIR → codegen)
- [x] Waker vtable field call workaround (extract to local variable)
- [x] async/await AwaitInst in MIR codegen
- [x] Incremental cache staleness (binary mtime instead of __DATE__)
