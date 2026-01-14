# Tasks: Network Standard Library Implementation

## Overview

Implement a high-performance, memory-safe networking standard library for TML with TCP/UDP sockets, HTTP client/server, and a NestJS-inspired decorator-based web framework.

---

## Phase 1: Core Socket Infrastructure

### 1.1 Address Types ✅ COMPLETE

> **Implementation**: `lib/core/src/net/` (ip.tml, socket.tml, parser.tml, mod.tml)
> **Tests**: `lib/core/tests/pending/net.test.tml`

- [x] Define `IpAddr` enum (V4, V6)
- [x] Define `Ipv4Addr` struct (4 bytes with octets array)
- [x] Define `Ipv6Addr` struct (16 bytes with segments array)
- [x] Define `SocketAddr` struct (ip + port)
- [x] Define `SocketAddrV4` struct (IPv4 + port)
- [x] Define `SocketAddrV6` struct (IPv6 + port + flowinfo + scope_id)
- [x] Implement `parse()` for address strings (via parser.tml)
- [x] Implement `Display` behavior for all types
- [x] Implement additional behaviors: `Copy`, `Duplicate`, `PartialEq`, `Eq`, `PartialOrd`, `Ord`, `Hash`, `Default`, `Debug`
- [x] Add classification methods: `is_loopback()`, `is_private()`, `is_multicast()`, `is_link_local()`, etc.
- [x] Add conversion methods: `to_ipv6_mapped()`, `to_ipv6_compatible()`, `to_ipv4()`
- [x] Add tests for address parsing and formatting

### 1.2 Buffer Management

- [ ] Define `Buffer` struct with capacity and length
- [ ] Define `BufferView` for zero-copy borrowed slices
- [ ] Implement `BufferPool` for pre-allocated buffers
- [ ] Add `Arena` allocator for request-scoped memory
- [ ] Implement `read_into()` and `write_from()` methods
- [ ] Add tests for buffer operations and pool reuse

### 1.3 Platform Abstraction Layer

- [ ] Create `sys/mod.tml` with platform detection
- [ ] Implement Windows socket wrappers (Winsock2)
- [ ] Implement POSIX socket wrappers (BSD sockets)
- [ ] Define `RawSocket` handle type
- [ ] Implement `socket()`, `bind()`, `listen()`, `accept()`
- [ ] Implement `connect()`, `send()`, `recv()`, `close()`
- [ ] Add non-blocking mode support (`set_nonblocking()`)

### 1.4 Error Types (Partial)

> **Implementation**: `lib/core/src/net/parser.tml` (AddrParseError only)

- [x] Define `AddrParseError` enum with variants (Empty, InvalidIpv4, InvalidIpv6, InvalidPort, InvalidSocketAddr)
- [x] Implement `Display` for parse error messages
- [ ] Define `NetError` enum with variants:
  - `ConnectionRefused`
  - `ConnectionReset`
  - `TimedOut`
  - `AddrInUse`
  - `AddrNotAvailable`
  - `WouldBlock`
  - `InvalidInput`
  - `Other(I32)`
- [ ] Implement `Display` for network error messages
- [ ] Map platform error codes to `NetError`

---

## Phase 2: TCP Implementation

### 2.1 TCP Listener

- [ ] Define `TcpListener` struct
- [ ] Implement `bind(addr: SocketAddr) -> Outcome[TcpListener, NetError]`
- [ ] Implement `accept() -> Outcome[TcpStream, NetError]`
- [ ] Implement `local_addr() -> SocketAddr`
- [ ] Add `set_ttl()`, `ttl()` methods
- [ ] Implement `incoming() -> TcpIncoming` iterator
- [ ] Add tests for listener bind and accept

### 2.2 TCP Stream

- [ ] Define `TcpStream` struct
- [ ] Implement `connect(addr: SocketAddr) -> Outcome[TcpStream, NetError]`
- [ ] Implement `Read` behavior (`read()`, `read_exact()`)
- [ ] Implement `Write` behavior (`write()`, `write_all()`, `flush()`)
- [ ] Add `peer_addr()`, `local_addr()` methods
- [ ] Implement `set_read_timeout()`, `set_write_timeout()`
- [ ] Add `set_nodelay()`, `nodelay()` for Nagle's algorithm
- [ ] Implement `shutdown(how: Shutdown)` method
- [ ] Add tests for TCP echo client/server

### 2.3 Async TCP (requires async runtime)

- [ ] Define `AsyncTcpListener` struct
- [ ] Define `AsyncTcpStream` struct
- [ ] Implement async `accept()` with `await`
- [ ] Implement async `read()` and `write()` with `await`
- [ ] Integrate with io_uring (Linux) / IOCP (Windows)
- [ ] Add tests for async TCP operations

---

## Phase 3: UDP Implementation

### 3.1 UDP Socket

- [ ] Define `UdpSocket` struct
- [ ] Implement `bind(addr: SocketAddr) -> Outcome[UdpSocket, NetError]`
- [ ] Implement `send_to(buf: ref [U8], addr: SocketAddr) -> Outcome[U64, NetError]`
- [ ] Implement `recv_from(buf: mut ref [U8]) -> Outcome[(U64, SocketAddr), NetError]`
- [ ] Add `connect()` for connected UDP mode
- [ ] Implement `send()` and `recv()` for connected sockets
- [ ] Add multicast support: `join_multicast_v4()`, `leave_multicast_v4()`
- [ ] Add broadcast support: `set_broadcast()`, `broadcast()`
- [ ] Add tests for UDP send/receive

### 3.2 Async UDP

- [ ] Define `AsyncUdpSocket` struct
- [ ] Implement async `send_to()` and `recv_from()`
- [ ] Add tests for async UDP operations

---

## Phase 4: HTTP Implementation

### 4.1 HTTP Types

- [ ] Define `Method` enum (GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS)
- [ ] Define `StatusCode` struct with common constants
- [ ] Define `Headers` type (case-insensitive map)
- [ ] Define `Version` enum (HTTP10, HTTP11, HTTP2)
- [ ] Implement header parsing and serialization
- [ ] Add tests for HTTP type operations

### 4.2 HTTP Request

- [ ] Define `Request[B]` generic struct
- [ ] Fields: method, uri, version, headers, body
- [ ] Implement `builder()` pattern for construction
- [ ] Implement body reading with streaming support
- [ ] Add `Request::get(uri)`, `Request::post(uri)` shortcuts
- [ ] Add tests for request building and parsing

### 4.3 HTTP Response

- [ ] Define `Response[B]` generic struct
- [ ] Fields: status, version, headers, body
- [ ] Implement `builder()` pattern for construction
- [ ] Add `Response::ok()`, `Response::not_found()` shortcuts
- [ ] Implement streaming body support
- [ ] Add tests for response building and serialization

### 4.4 HTTP Client

- [ ] Define `HttpClient` struct with connection pool
- [ ] Implement `send(req: Request) -> Outcome[Response, HttpError]`
- [ ] Add connection pooling with configurable limits
- [ ] Implement keep-alive connection reuse
- [ ] Add timeout configuration
- [ ] Implement redirect following (configurable)
- [ ] Add TLS support for HTTPS
- [ ] Add tests for HTTP client operations

### 4.5 HTTP Server (Low-Level)

- [ ] Define `HttpServer` struct
- [ ] Implement `bind(addr: SocketAddr) -> Outcome[HttpServer, NetError]`
- [ ] Define `Handler` type: `func(Request) -> Response`
- [ ] Implement request parsing from TCP stream
- [ ] Implement response serialization to TCP stream
- [ ] Add connection handling with keep-alive
- [ ] Implement graceful shutdown
- [ ] Add tests for HTTP server

---

## Phase 5: Decorator-Based Server Framework

### 5.1 Core Decorators

- [ ] Implement `@Controller(path: Str)` decorator
- [ ] Implement `@Get(path: Str)` route decorator
- [ ] Implement `@Post(path: Str)` route decorator
- [ ] Implement `@Put(path: Str)` route decorator
- [ ] Implement `@Delete(path: Str)` route decorator
- [ ] Implement `@Patch(path: Str)` route decorator
- [ ] Store decorator metadata in compile-time reflection data
- [ ] Add tests for decorator parsing

### 5.2 Parameter Decorators

- [ ] Implement `@Param(name: Str)` for URL parameters
- [ ] Implement `@Query(name: Str)` for query string
- [ ] Implement `@Body` for request body deserialization
- [ ] Implement `@Header(name: Str)` for header values
- [ ] Implement `@Req` for raw request access
- [ ] Implement `@Res` for raw response access
- [ ] Add automatic type coercion for parameters
- [ ] Add tests for parameter injection

### 5.3 Router

- [ ] Define `Router` struct with route tree
- [ ] Implement path pattern matching with parameters (`:id`, `*wildcard`)
- [ ] Implement route registration from decorated classes
- [ ] Add route conflict detection
- [ ] Implement efficient route lookup (radix tree)
- [ ] Add tests for routing

### 5.4 Middleware System

- [ ] Define `Middleware` behavior
- [ ] Implement `@Middleware` decorator for classes
- [ ] Implement `@UseMiddleware(M)` decorator for controllers/routes
- [ ] Define middleware execution order (global -> controller -> route)
- [ ] Implement `next()` pattern for middleware chaining
- [ ] Add built-in middleware: `Logger`, `Cors`, `Compression`
- [ ] Add tests for middleware chain

### 5.5 Guards and Interceptors

- [ ] Define `Guard` behavior with `canActivate()` method
- [ ] Implement `@Guard` decorator
- [ ] Implement `@UseGuard(G)` decorator
- [ ] Define `Interceptor` behavior with `intercept()` method
- [ ] Implement `@Interceptor` decorator
- [ ] Implement `@UseInterceptor(I)` decorator
- [ ] Add tests for guards and interceptors

### 5.6 Application Bootstrap

- [ ] Define `Application` struct
- [ ] Implement `Application::create(modules: [...])` factory
- [ ] Implement module scanning for decorated classes
- [ ] Implement dependency injection container
- [ ] Add `listen(port: U16)` to start server
- [ ] Add graceful shutdown support
- [ ] Add tests for application lifecycle

---

## Phase 6: Performance Optimization

### 6.1 I/O Backend Selection

- [ ] Implement io_uring backend for Linux 5.1+
- [ ] Implement IOCP backend for Windows
- [ ] Implement kqueue backend for macOS/BSD
- [ ] Implement epoll fallback for older Linux
- [ ] Add runtime backend detection and selection
- [ ] Add tests for each backend

### 6.2 Memory Optimization

- [ ] Implement slab allocator for fixed-size objects
- [ ] Add object pooling for Request/Response
- [ ] Implement zero-copy buffer chains
- [ ] Add memory-mapped file serving
- [ ] Profile and eliminate allocations in hot paths
- [ ] Add memory usage tests/benchmarks

### 6.3 Connection Management

- [ ] Implement connection pool with health checking
- [ ] Add connection limiting per client IP
- [ ] Implement backpressure for slow clients
- [ ] Add graceful connection draining on shutdown
- [ ] Add connection metrics (active, idle, total)
- [ ] Add tests for connection management

---

## Phase 7: Documentation and Examples

### 7.1 API Documentation

- [ ] Document all public types with doc comments
- [ ] Add examples in doc comments
- [ ] Generate API reference documentation
- [ ] Add architecture overview document

### 7.2 Examples

- [ ] TCP echo server example
- [ ] UDP ping/pong example
- [ ] HTTP file server example
- [ ] REST API with decorators example
- [ ] WebSocket chat example (if WebSocket added)
- [ ] Middleware composition example

### 7.3 Guides

- [ ] Getting started with networking
- [ ] Building a REST API
- [ ] Performance tuning guide
- [ ] Memory management best practices

---

## Progress Tracking

| Phase                        | Status      | Completion |
| ---------------------------- | ----------- | ---------- |
| Phase 1: Core Infrastructure | In Progress | 40%        |
| Phase 2: TCP                 | Not Started | 0%         |
| Phase 3: UDP                 | Not Started | 0%         |
| Phase 4: HTTP                | Not Started | 0%         |
| Phase 5: Decorator Framework | Not Started | 0%         |
| Phase 6: Performance         | Not Started | 0%         |
| Phase 7: Documentation       | Not Started | 0%         |

### Phase 1 Breakdown:
- 1.1 Address Types: ✅ Complete (100%)
- 1.2 Buffer Management: ⏳ Not Started (0%)
- 1.3 Platform Abstraction: ⏳ Not Started (0%)
- 1.4 Error Types: 🔶 Partial (30% - AddrParseError done, NetError pending)

---

## Validation

### Unit Tests

```bash
tml test lib/std/tests/net/
```

### Integration Tests

```bash
tml test lib/std/tests/net/integration/
```

### Benchmarks

```bash
tml bench benchmarks/net/
```

### Performance Validation

- HTTP throughput: `wrk -t12 -c400 -d30s http://localhost:8080/`
- Memory profiling: `valgrind --tool=massif ./server`
- Latency: `wrk -t1 -c1 -d10s --latency http://localhost:8080/`

---

## Dependencies

### External

- OpenSSL or BoringSSL for TLS (optional, can use system TLS)
- Platform headers for syscalls

### Internal

- `core::alloc` - Memory allocation
- `core::iter` - Iterators
- `core::slice` - Slice operations
- `core::net` - ✅ **Available** - IP addresses and socket addresses (Ipv4Addr, Ipv6Addr, IpAddr, SocketAddr, SocketAddrV4, SocketAddrV6, parsing)
- `core::hash` - Hash and Hasher behaviors
- `core::option` - Maybe type
- `core::result` - Outcome type
- `std::collections` - HashMap for headers
- `std::io` - Read/Write behaviors
- `std::sync` - Synchronization primitives

---

## Risk Mitigation

| Risk                     | Mitigation                                            |
| ------------------------ | ----------------------------------------------------- |
| Platform differences     | Comprehensive abstraction layer, CI on all platforms  |
| Memory leaks             | Arena allocators, RAII patterns, memory tests         |
| Performance regression   | Continuous benchmarking, performance tests in CI      |
| Security vulnerabilities | TLS by default, timeout enforcement, input validation |
| API instability          | Design review before implementation, RFC process      |
