# Tasks: Async Network Stack

**Status**: Not Started (0%)
**Priority**: High
**Consolidates**: `add-network-stdlib` + `async-http-runtime` + `multi-threaded-runtime` + `promises-reactivity`
**Depends on**: `thread-safe-native` (99% done - atomics, mutex, channels, Arc, threads)

## Phase 1: Async Runtime Foundation

- [ ] 1.1 Implement OS event loop abstraction (epoll/kqueue/IOCP detection and dispatch)
- [ ] 1.2 Implement `io_reactor_epoll.c` for Linux
- [ ] 1.3 Implement `io_reactor_iocp.c` for Windows
- [ ] 1.4 Implement `io_reactor_kqueue.c` for macOS/BSD
- [ ] 1.5 Define `Future` behavior with `poll(mut this, cx: mut ref Context) -> Poll[Self::Output]`
- [ ] 1.6 Define `Poll[T]` type with `Ready(T)` and `Pending` variants
- [ ] 1.7 Implement `Waker` and `Context` types
- [ ] 1.8 Implement single-threaded executor with `block_on[F: Future](future: F) -> F::Output`
- [ ] 1.9 Implement `spawn[F: Future](future: F) -> JoinHandle[F::Output]`
- [ ] 1.10 Implement hierarchical timer wheel (O(1) insert, O(1) amortized expiration)
- [ ] 1.11 Verify basic Future/Poll works end-to-end

## Phase 2: Multi-Threaded Executor

- [ ] 2.1 Implement worker threads with local task queues (LIFO)
- [ ] 2.2 Implement global MPMC queue for overflow tasks
- [ ] 2.3 Implement work-stealing between workers (FIFO steal from other workers)
- [ ] 2.4 Implement `spawn_blocking` for offloading blocking operations to thread pool
- [ ] 2.5 Implement runtime configuration: `@runtime("multi", workers: N)`
- [ ] 2.6 Implement graceful shutdown (complete all pending tasks)
- [ ] 2.7 Verify linear scaling up to available CPU cores
- [ ] 2.8 Benchmark: task scheduling latency < 1us

## Phase 3: Async I/O Primitives

- [ ] 3.1 Define `AsyncRead` behavior: `async func read(mut this, buf: mut ref [U8]) -> Outcome[USize, IoError]`
- [ ] 3.2 Define `AsyncWrite` behavior: `async func write(mut this, buf: ref [U8]) -> Outcome[USize, IoError]`
- [ ] 3.3 Implement async TCP: `TcpListener::bind()`, `TcpListener::accept()`, `TcpStream::connect()`
- [ ] 3.4 Implement async UDP: `UdpSocket::bind()`, `send_to()`, `recv_from()`
- [ ] 3.5 Implement async buffered I/O: `BufReader`, `BufWriter`
- [ ] 3.6 Implement async channels: `mpsc`, `oneshot`, `broadcast`
- [ ] 3.7 Implement `tokio::select!`-style macro for waiting on multiple futures
- [ ] 3.8 Verify TCP echo server handles concurrent connections

## Phase 4: Network Address Types & Sockets

- [ ] 4.1 Implement `IpAddr` (IPv4 + IPv6), `SocketAddr` types in `std::net::addr`
- [ ] 4.2 Implement DNS resolution: `resolve(hostname: Str) -> List[IpAddr]`
- [ ] 4.3 Implement connection pooling with configurable limits
- [ ] 4.4 Implement Unix domain sockets (POSIX)
- [ ] 4.5 Implement zero-copy buffer management (`BufferView` type)
- [ ] 4.6 Implement memory-mapped buffers for large transfers

## Phase 5: TLS Integration

- [ ] 5.1 Implement platform TLS bindings (OpenSSL/BoringSSL or native)
- [ ] 5.2 Implement async TLS handshake
- [ ] 5.3 Implement HTTPS support (TLS + HTTP)
- [ ] 5.4 Configure TLS 1.3 default, TLS 1.2 fallback, strong cipher suites
- [ ] 5.5 Implement ALPN for protocol negotiation (h2, http/1.1)
- [ ] 5.6 Implement certificate verification by default
- [ ] 5.7 Verify TLS works with real certificates

## Phase 6: HTTP Core

- [ ] 6.1 Implement HTTP types: `Request`, `Response`, `Headers`, `Method`, `StatusCode`
- [ ] 6.2 Implement HTTP/1.1 codec: request/response parsing and serialization
- [ ] 6.3 Implement persistent connections (keep-alive)
- [ ] 6.4 Implement chunked transfer encoding
- [ ] 6.5 Implement HTTP client with connection pooling
- [ ] 6.6 Implement HTTP server with basic routing
- [ ] 6.7 Implement streaming request/response bodies
- [ ] 6.8 Benchmark: target > 500K req/s for simple responses

## Phase 7: HTTP/2 + Advanced Protocols

- [ ] 7.1 Implement HTTP/2 binary framing layer
- [ ] 7.2 Implement HPACK header compression
- [ ] 7.3 Implement stream multiplexing and flow control
- [ ] 7.4 Implement server push
- [ ] 7.5 Implement WebSocket protocol (upgrade handshake, frames, ping/pong)
- [ ] 7.6 Implement Server-Sent Events (SSE)
- [ ] 7.7 Benchmark: target > 400K req/s for HTTP/2

## Phase 8: Application Framework (NestJS-style)

- [ ] 8.1 Implement `@Controller("/path")` decorator for route grouping
- [ ] 8.2 Implement `@Get`, `@Post`, `@Put`, `@Delete`, `@Patch` route decorators
- [ ] 8.3 Implement `@Param`, `@Query`, `@Body`, `@Header` for parameter injection
- [ ] 8.4 Implement router with path matching, parameter extraction, wildcard routes
- [ ] 8.5 Implement `@Middleware` decorator and middleware pipeline (request → ... → handler → ... → response)
- [ ] 8.6 Implement `@Guard` for authentication/authorization checks
- [ ] 8.7 Implement `@Interceptor` for request/response transformation
- [ ] 8.8 Implement `@Injectable` for dependency injection container
- [ ] 8.9 Implement CORS, rate limiting, request size limits
- [ ] 8.10 Verify decorator routing works end-to-end with middleware chain

## Phase 9: High-Level Async Primitives (Promise + Observable)

- [ ] 9.1 Implement `Promise[T]` type with Pending/Fulfilled/Rejected states
- [ ] 9.2 Implement `.then()`, `.catch()`, `.finally()`, `.map()` on Promise
- [ ] 9.3 Implement `Promise.resolve()`, `Promise.reject()`, `Promise.new()`
- [ ] 9.4 Implement `Promise.all()`, `Promise.race()`, `Promise.any()`, `Promise.allSettled()`
- [ ] 9.5 Implement microtask queue for Promise resolution ordering
- [ ] 9.6 Implement `Observable[T]` push-based value stream
- [ ] 9.7 Implement `Observer[T]` behavior: `on_next`, `on_error`, `on_complete`
- [ ] 9.8 Implement creation: `Observable.of()`, `.from()`, `.interval()`, `.timer()`
- [ ] 9.9 Implement transformation: `.map()`, `.flat_map()`, `.switch_map()`, `.filter()`, `.take()`, `.skip()`
- [ ] 9.10 Implement combination: `.merge()`, `.concat()`, `.zip()`, `.combine_latest()`
- [ ] 9.11 Implement utilities: `.debounce()`, `.throttle()`, `.retry()`, `.catch_error()`
- [ ] 9.12 Implement `Subject[T]`, `BehaviorSubject[T]`, `ReplaySubject[T]`
- [ ] 9.13 Implement pipe operator `|>` in parser for fluent chaining
- [ ] 9.14 Verify Promise and Observable operators produce correct results

## Phase 10: Validation & Benchmarks

- [ ] 10.1 Verify TCP echo server handles 10K concurrent connections
- [ ] 10.2 Verify HTTP server with decorator routing serves requests correctly
- [ ] 10.3 Verify TLS with real certificates
- [ ] 10.4 Verify HTTP/2 passes basic compliance
- [ ] 10.5 Verify memory usage stays constant under sustained load (no leaks)
- [ ] 10.6 Benchmark HTTP throughput vs Go/Rust equivalents
- [ ] 10.7 Verify all networking operations are memory-safe
- [ ] 10.8 Documentation with examples for all features
