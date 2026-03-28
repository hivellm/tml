# Proposal: Async Network Stack

## Status: PROPOSED

**Consolidates**: `add-network-stdlib` + `async-http-runtime` + `multi-threaded-runtime` + `promises-reactivity`

## Why

TML currently lacks networking, async runtime, and high-level async primitives. These four capabilities form a **single dependency chain** that must be built bottom-up:

```
Layer 1: Multi-threaded async runtime (executor, scheduler, I/O reactor)
Layer 2: Async I/O primitives (TCP, UDP, timers, buffers)
Layer 3: Protocol implementations (HTTP/1.1, HTTP/2, TLS, WebSocket)
Layer 4: Application framework (decorators, middleware, DI) + high-level async (Promise, Observable)
```

Building these as separate tasks creates artificial boundaries. A developer cannot use networking without the runtime, cannot use HTTP without TCP, and cannot use Promise without an executor. They are one cohesive system.

### Current State

- **Thread primitives**: 99% done (`thread-safe-native`): atomics, mutex, rwlock, channels, Arc, threads
- **Async runtime**: 2/10 - `Poll[T]` exists but no executor/scheduler
- **Networking**: 1/10 - Stubs only in `net/pending/`
- **High-level async**: 0/10 - No Promise, Observable, or reactive types

### Design Principles

| Principle | Implementation |
|-----------|----------------|
| Zero-cost abstractions | Futures compile to efficient state machines |
| Cooperative scheduling | Tasks yield via `Poll::Pending`, no preemption |
| Work-stealing | Multi-threaded executor balances load across cores |
| Decorator-driven API | NestJS-style `@Controller`, `@Get`, `@Post` |
| Zero-copy where possible | `BufferView` for borrowed slices |
| Safe defaults | TLS by default, timeouts enforced |

## What Changes

### Phase 1: Async Runtime Foundation
OS-level event loop and basic executor.

- Event loop abstraction: epoll (Linux), kqueue (macOS), IOCP (Windows)
- `Future` behavior and `Poll[T]` type
- Single-threaded executor (`block_on`, `spawn`)
- Timer wheel for efficient timeout handling
- `Waker` and `Context` types for Future polling

### Phase 2: Multi-Threaded Executor
Work-stealing scheduler for multi-core utilization.

- Worker threads with local task queues
- Global MPMC queue for overflow
- Work-stealing between workers
- Thread pool for blocking operations (`spawn_blocking`)
- `@runtime("multi", workers: N)` configuration

### Phase 3: Async I/O Primitives
Async wrappers for I/O operations.

- `AsyncRead` / `AsyncWrite` behaviors
- Async TCP: `TcpListener`, `TcpStream`
- Async UDP: `UdpSocket`
- Buffered async I/O: `BufReader`, `BufWriter`
- Async channels: `mpsc`, `oneshot`, `broadcast`

### Phase 4: Low-Level Network API
Socket-level networking with address types.

- `std::net::socket` - Raw socket API
- `std::net::addr` - `IpAddr`, `SocketAddr` (IPv4, IPv6)
- Non-blocking socket operations
- Connection pooling with configurable limits
- Unix domain sockets (POSIX)

### Phase 5: TLS Integration
Secure transport layer.

- Platform TLS bindings (OpenSSL/BoringSSL or native)
- Async TLS handshake
- HTTPS support
- TLS 1.2/1.3 with ALPN for protocol negotiation
- Certificate verification by default

### Phase 6: HTTP Core
HTTP protocol implementation.

- HTTP types: `Request`, `Response`, `Headers`, `Method`, `StatusCode`
- HTTP/1.1 codec: persistent connections, chunked transfer, pipelining
- HTTP client with connection pooling
- HTTP server with routing
- Streaming bodies

### Phase 7: HTTP/2 + Advanced Protocols
Binary protocols and real-time communication.

- HTTP/2: binary framing, HPACK compression, stream multiplexing, server push
- WebSocket protocol support
- Server-Sent Events (SSE)
- HTTP/3 (QUIC) - best-effort, depends on QUIC library

### Phase 8: Application Framework
NestJS-style decorator-driven framework.

- `@Controller("/path")` for route grouping
- `@Get`, `@Post`, `@Put`, `@Delete`, `@Patch` for HTTP methods
- `@Param`, `@Query`, `@Body`, `@Header` for parameter injection
- `@Middleware`, `@Guard`, `@Interceptor` for request pipeline
- `@Injectable` for dependency injection
- Router with path matching and parameter extraction

### Phase 9: High-Level Async Primitives
Promise and Observable types for ergonomic async programming.

- `Promise[T]` with `.then()`, `.catch()`, `.finally()`, `.map()`
- `Promise.all()`, `Promise.race()`, `Promise.any()`, `Promise.allSettled()`
- `Observable[T]` with `.map()`, `.filter()`, `.flat_map()`, `.take()`, `.skip()`
- `Observable.merge()`, `.concat()`, `.zip()`, `.combine_latest()`
- `Subject[T]`, `BehaviorSubject[T]`, `ReplaySubject[T]`
- `.debounce()`, `.throttle()`, `.retry()`, `.catch_error()`
- Pipe operator `|>` for fluent chaining
- Microtask queue integration for Promises

## Architecture

```
Application Code
     │
     ▼
┌─────────────────┐
│  HTTP Framework  │  @Controller, @Get, Middleware, Guards, DI
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Promise/Observable│  .then(), .map(), .filter(), |>
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  HTTP Protocol   │  HTTP/1.1, HTTP/2, HTTP/3 codecs
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   TLS Layer      │  TLS 1.2/1.3, ALPN
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Async Runtime   │  Work-stealing executor, timers
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  I/O Reactor     │  epoll/kqueue/IOCP event loop
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   OS Layer       │  Sockets, threads, atomics (thread-safe-native)
└─────────────────┘
```

## Impact

- Affected specs: `04-TYPES.md`, `05-SEMANTICS.md`, `09-CLI.md`, `13-BUILTINS.md`
- Affected code: `lib/std/src/net/`, `lib/core/src/` (Promise, Observable), `compiler/runtime/`, `compiler/src/parser/` (pipe operator), `compiler/src/codegen/`
- Breaking change: NO (all additive)
- User benefit: Full networking stack, modern async programming, NestJS-like framework

## Compiler Requirements

- `async/await` syntax (sugar for Future-returning functions)
- Associated types (for `Future::Output`)
- Pin type (for self-referential futures)
- Decorator macros (`@Controller`, `@Get`, etc.)
- Trait objects (`dyn`) for type erasure in handlers
- Pipe operator `|>` for fluent chaining

## Performance Targets

| Metric | Target |
|--------|--------|
| HTTP/1.1 req/s | > 500,000 |
| HTTP/2 req/s | > 400,000 |
| p99 latency | < 10ms |
| Memory per connection | < 4KB |
| Concurrent connections | 10K+ |
| Task scheduling latency | < 1us |

## Success Criteria

1. TCP echo server handles 10K concurrent connections
2. HTTP server responds to GET/POST with decorator routing
3. Middleware chain executes correctly
4. TLS works with real certificates
5. Promise.all/race work correctly
6. Observable operators produce correct results
7. Work-stealing scales linearly with cores
8. HTTP benchmarks competitive with Rust/Go
9. All networking operations are memory-safe
10. Zero-copy path for static file serving

## References

- [Tokio internals](https://tokio.rs/tokio/tutorial)
- [Rust async book](https://rust-lang.github.io/async-book/)
- [NestJS documentation](https://docs.nestjs.com/)
- [Go net/http](https://pkg.go.dev/net/http)
- [io_uring](https://kernel.dk/io_uring.pdf)
- [HTTP/2 RFC 7540](https://httpwg.org/specs/rfc7540.html)
