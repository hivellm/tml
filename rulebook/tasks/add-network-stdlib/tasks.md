# Tasks: Network Standard Library

**Status**: In Progress (~45% - Phases 1-3 Complete, Phase 4 In Progress)

**Priority**: Medium - Standard library feature

## Phase 1: Core Socket Infrastructure

### 1.1 Address Types
- [x] 1.1.1 Define `IpAddr` enum (V4, V6)
- [x] 1.1.2 Define `Ipv4Addr` struct (4 bytes with octets array)
- [x] 1.1.3 Define `Ipv6Addr` struct (16 bytes with segments array)
- [x] 1.1.4 Define `SocketAddr` struct (ip + port)
- [x] 1.1.5 Define `SocketAddrV4` struct (IPv4 + port)
- [x] 1.1.6 Define `SocketAddrV6` struct (IPv6 + port + flowinfo + scope_id)
- [x] 1.1.7 Implement `parse()` for address strings
- [x] 1.1.8 Implement `Display` behavior for all types
- [x] 1.1.9 Implement `Copy`, `Duplicate`, `PartialEq`, `Eq`, `PartialOrd`, `Ord`, `Hash`, `Default`, `Debug`
- [x] 1.1.10 Add classification methods: `is_loopback()`, `is_private()`, `is_multicast()`, `is_link_local()`
- [x] 1.1.11 Add conversion methods: `to_ipv6_mapped()`, `to_ipv6_compatible()`, `to_ipv4()`
- [x] 1.1.12 Add tests for address parsing and formatting

### 1.2 Error Types
- [x] 1.2.1 Define `AddrParseError` enum (Empty, InvalidIpv4, InvalidIpv6, InvalidPort, InvalidSocketAddr)
- [x] 1.2.2 Implement `Display` for `AddrParseError`
- [x] 1.2.3 Define `NetError` enum with all variants
- [x] 1.2.4 Implement `Display` for `NetError`
- [x] 1.2.5 Map platform error codes to `NetError` (POSIX + Windows)

### 1.3 Buffer Management
- [x] 1.3.1 Define `Buffer` struct with capacity and length
- [x] 1.3.2 Define `BufferView` for zero-copy borrowed slices
- [x] 1.3.3 Implement `BufferPool` for pre-allocated buffers
- [x] 1.3.4 Add `Arena` allocator for request-scoped memory
- [x] 1.3.5 Implement `read_into()` and `write_from()` methods
- [x] 1.3.6 Add tests for buffer operations and pool reuse

### 1.4 Platform Abstraction Layer
- [x] 1.4.1 Create `sys/mod.tml` with platform detection
- [x] 1.4.2 Implement Windows socket wrappers (Winsock2)
- [x] 1.4.3 Implement POSIX socket wrappers (BSD sockets)
- [x] 1.4.4 Define `RawSocket` handle type
- [x] 1.4.5 Implement `socket()`, `bind()`, `listen()`, `accept()`
- [x] 1.4.6 Implement `connect()`, `send()`, `recv()`, `close()`
- [x] 1.4.7 Add non-blocking mode support (`set_nonblocking()`)

## Phase 2: TCP Implementation

### 2.1 TCP Listener
- [x] 2.1.1 Define `TcpListener` struct
- [x] 2.1.2 Implement `bind(addr: SocketAddr) -> Outcome[TcpListener, NetError]`
- [x] 2.1.3 Implement `accept() -> Outcome[TcpStream, NetError]`
- [x] 2.1.4 Implement `local_addr() -> SocketAddr`
- [x] 2.1.5 Add `set_ttl()`, `ttl()` methods
- [x] 2.1.6 Implement `incoming() -> TcpIncoming` iterator
- [x] 2.1.7 Add tests for listener bind and accept

### 2.2 TCP Stream
- [x] 2.2.1 Define `TcpStream` struct
- [x] 2.2.2 Implement `connect(addr: SocketAddr) -> Outcome[TcpStream, NetError]`
- [x] 2.2.3 Implement `Read` behavior (`read()`, `read_exact()`)
- [x] 2.2.4 Implement `Write` behavior (`write()`, `write_all()`, `flush()`)
- [x] 2.2.5 Add `peer_addr()`, `local_addr()` methods
- [x] 2.2.6 Implement `set_read_timeout()`, `set_write_timeout()`
- [x] 2.2.7 Add `set_nodelay()`, `nodelay()` for Nagle's algorithm
- [x] 2.2.8 Implement `shutdown(how: Shutdown)` method
- [x] 2.2.9 Add tests for TCP echo client/server

### 2.3 Async TCP
- [x] 2.3.1 Define `AsyncTcpListener` struct
- [x] 2.3.2 Define `AsyncTcpStream` struct
- [x] 2.3.3 Implement async `accept()` with `await`
- [x] 2.3.4 Implement async `read()` and `write()` with `await`
- [ ] 2.3.5 Integrate with io_uring (Linux) / IOCP (Windows)
- [x] 2.3.6 Add tests for async TCP operations

## Phase 3: UDP Implementation

### 3.1 UDP Socket
- [x] 3.1.1 Define `UdpSocket` struct
- [x] 3.1.2 Implement `bind(addr: SocketAddr) -> Outcome[UdpSocket, NetError]`
- [x] 3.1.3 Implement `send_to(buf: ref [U8], addr: SocketAddr) -> Outcome[U64, NetError]`
- [x] 3.1.4 Implement `recv_from(buf: mut ref [U8]) -> Outcome[(U64, SocketAddr), NetError]`
- [x] 3.1.5 Add `connect()` for connected UDP mode
- [x] 3.1.6 Implement `send()` and `recv()` for connected sockets
- [x] 3.1.7 Add multicast support: `join_multicast_v4()`, `leave_multicast_v4()`
- [x] 3.1.8 Add broadcast support: `set_broadcast()`, `broadcast()`
- [x] 3.1.9 Add tests for UDP send/receive

### 3.2 Async UDP
- [x] 3.2.1 Define `AsyncUdpSocket` struct
- [x] 3.2.2 Implement async `send_to()` and `recv_from()`
- [x] 3.2.3 Add tests for async UDP operations

## Phase 4: HTTP Implementation

### 4.1 HTTP Types
- [x] 4.1.1 Define `Method` enum (GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS)
- [x] 4.1.2 Define `StatusCode` struct with common constants
- [x] 4.1.3 Define `Headers` type (case-insensitive map)
- [x] 4.1.4 Define `Version` enum (HTTP10, HTTP11, HTTP2)
- [x] 4.1.5 Implement header parsing and serialization
- [x] 4.1.6 Add tests for HTTP type operations

### 4.2 HTTP Request
- [x] 4.2.1 Define `Request[B]` generic struct
- [x] 4.2.2 Implement `builder()` pattern for construction
- [ ] 4.2.3 Implement body reading with streaming support
- [x] 4.2.4 Add `Request::get(uri)`, `Request::post(uri)` shortcuts
- [x] 4.2.5 Add tests for request building and parsing

### 4.3 HTTP Response
- [x] 4.3.1 Define `Response[B]` generic struct
- [x] 4.3.2 Implement `builder()` pattern for construction
- [x] 4.3.3 Add `Response::ok()`, `Response::not_found()` shortcuts
- [ ] 4.3.4 Implement streaming body support
- [x] 4.3.5 Add tests for response building and serialization

### 4.4 HTTP Client
- [ ] 4.4.1 Define `HttpClient` struct with connection pool
- [ ] 4.4.2 Implement `send(req: Request) -> Outcome[Response, HttpError]`
- [ ] 4.4.3 Add connection pooling with configurable limits
- [ ] 4.4.4 Implement keep-alive connection reuse
- [ ] 4.4.5 Add timeout configuration
- [ ] 4.4.6 Implement redirect following (configurable)
- [ ] 4.4.7 Add TLS support for HTTPS
- [ ] 4.4.8 Add tests for HTTP client operations

### 4.5 HTTP Server
- [ ] 4.5.1 Define `HttpServer` struct
- [ ] 4.5.2 Implement `bind(addr: SocketAddr) -> Outcome[HttpServer, NetError]`
- [ ] 4.5.3 Define `Handler` type: `func(Request) -> Response`
- [ ] 4.5.4 Implement request parsing from TCP stream
- [ ] 4.5.5 Implement response serialization to TCP stream
- [ ] 4.5.6 Add connection handling with keep-alive
- [ ] 4.5.7 Implement graceful shutdown
- [ ] 4.5.8 Add tests for HTTP server

## Phase 5: Web Framework (NestJS-inspired)

### 5.1 Core Decorators
- [ ] 5.1.1 Implement `@Controller(path: Str)` decorator
- [ ] 5.1.2 Implement `@Get(path: Str)` route decorator
- [ ] 5.1.3 Implement `@Post(path: Str)` route decorator
- [ ] 5.1.4 Implement `@Put(path: Str)` route decorator
- [ ] 5.1.5 Implement `@Delete(path: Str)` route decorator
- [ ] 5.1.6 Implement `@Patch(path: Str)` route decorator
- [ ] 5.1.7 Store decorator metadata in compile-time reflection data
- [ ] 5.1.8 Add tests for decorator parsing

### 5.2 Parameter Decorators
- [ ] 5.2.1 Implement `@Param(name: Str)` for URL parameters
- [ ] 5.2.2 Implement `@Query(name: Str)` for query string
- [ ] 5.2.3 Implement `@Body` for request body deserialization
- [ ] 5.2.4 Implement `@Header(name: Str)` for header values
- [ ] 5.2.5 Implement `@Req` for raw request access
- [ ] 5.2.6 Implement `@Res` for raw response access
- [ ] 5.2.7 Add automatic type coercion for parameters
- [ ] 5.2.8 Add tests for parameter injection

### 5.3 Router
- [ ] 5.3.1 Define `Router` struct with route tree
- [ ] 5.3.2 Implement path pattern matching with parameters (`:id`, `*wildcard`)
- [ ] 5.3.3 Implement route registration from decorated classes
- [ ] 5.3.4 Add route conflict detection
- [ ] 5.3.5 Implement efficient route lookup (radix tree)
- [ ] 5.3.6 Add tests for routing

### 5.4 Middleware System
- [ ] 5.4.1 Define `Middleware` behavior
- [ ] 5.4.2 Implement `@Middleware` decorator for classes
- [ ] 5.4.3 Implement `@UseMiddleware(M)` decorator
- [ ] 5.4.4 Define middleware execution order (global -> controller -> route)
- [ ] 5.4.5 Implement `next()` pattern for middleware chaining
- [ ] 5.4.6 Add built-in middleware: `Logger`, `Cors`, `Compression`
- [ ] 5.4.7 Add tests for middleware chain

### 5.5 Guards and Interceptors
- [ ] 5.5.1 Define `Guard` behavior with `canActivate()` method
- [ ] 5.5.2 Implement `@Guard` decorator
- [ ] 5.5.3 Implement `@UseGuard(G)` decorator
- [ ] 5.5.4 Define `Interceptor` behavior with `intercept()` method
- [ ] 5.5.5 Implement `@Interceptor` decorator
- [ ] 5.5.6 Implement `@UseInterceptor(I)` decorator
- [ ] 5.5.7 Add tests for guards and interceptors

### 5.6 Application Bootstrap
- [ ] 5.6.1 Define `Application` struct
- [ ] 5.6.2 Implement `Application::create(modules: [...])` factory
- [ ] 5.6.3 Implement module scanning for decorated classes
- [ ] 5.6.4 Implement dependency injection container
- [ ] 5.6.5 Add `listen(port: U16)` to start server
- [ ] 5.6.6 Add graceful shutdown support
- [ ] 5.6.7 Add tests for application lifecycle

## Phase 6: Performance Optimization

### 6.1 I/O Backend Selection
- [ ] 6.1.1 Implement io_uring backend for Linux 5.1+
- [ ] 6.1.2 Implement IOCP backend for Windows
- [ ] 6.1.3 Implement kqueue backend for macOS/BSD
- [ ] 6.1.4 Implement epoll fallback for older Linux
- [ ] 6.1.5 Add runtime backend detection and selection
- [ ] 6.1.6 Add tests for each backend

### 6.2 Memory Optimization
- [ ] 6.2.1 Implement slab allocator for fixed-size objects
- [ ] 6.2.2 Add object pooling for Request/Response
- [ ] 6.2.3 Implement zero-copy buffer chains
- [ ] 6.2.4 Add memory-mapped file serving
- [ ] 6.2.5 Profile and eliminate allocations in hot paths
- [ ] 6.2.6 Add memory usage tests/benchmarks

### 6.3 Connection Management
- [ ] 6.3.1 Implement connection pool with health checking
- [ ] 6.3.2 Add connection limiting per client IP
- [ ] 6.3.3 Implement backpressure for slow clients
- [ ] 6.3.4 Add graceful connection draining on shutdown
- [ ] 6.3.5 Add connection metrics (active, idle, total)
- [ ] 6.3.6 Add tests for connection management

## Phase 7: Documentation and Examples

### 7.1 API Documentation
- [ ] 7.1.1 Document all public types with doc comments
- [ ] 7.1.2 Add examples in doc comments
- [ ] 7.1.3 Generate API reference documentation
- [ ] 7.1.4 Add architecture overview document

### 7.2 Examples
- [ ] 7.2.1 TCP echo server example
- [ ] 7.2.2 UDP ping/pong example
- [ ] 7.2.3 HTTP file server example
- [ ] 7.2.4 REST API with decorators example
- [ ] 7.2.5 WebSocket chat example
- [ ] 7.2.6 Middleware composition example

### 7.3 Guides
- [ ] 7.3.1 Getting started with networking
- [ ] 7.3.2 Building a REST API
- [ ] 7.3.3 Performance tuning guide
- [ ] 7.3.4 Memory management best practices

## Summary

| Phase | Description | Status | Progress |
|-------|-------------|--------|----------|
| 1 | Core Infrastructure | **Complete** | 24/24 |
| 2 | TCP Implementation | **Complete** | 21/22 |
| 3 | UDP Implementation | **Complete** | 12/12 |
| 4 | HTTP Implementation | **In Progress** | 14/30 |
| 5 | Web Framework | Not Started | 0/44 |
| 6 | Performance | Not Started | 0/18 |
| 7 | Documentation | Not Started | 0/14 |
| **Total** | | **~43%** | **71/164** |

## Files Added/Modified

### New Files
- `lib/std/src/net/mod.tml` - Network module entry point
- `lib/std/src/net/ip.tml` - IP address types (Ipv4Addr, Ipv6Addr, IpAddr)
- `lib/std/src/net/socket.tml` - Socket address types
- `lib/std/src/net/parser.tml` - Address parsing utilities
- `lib/std/src/net/error.tml` - Network error types (NetError, NetErrorKind)
- `lib/std/src/net/buffer.tml` - Buffer management (Buffer, BufferView, BufferPool)
- `lib/std/src/net/sys/mod.tml` - Platform abstraction layer (RawSocket, syscalls)
- `lib/std/src/net/tcp.tml` - TCP networking (TcpListener, TcpStream, TcpBuilder)
- `lib/std/src/net/udp.tml` - UDP networking (UdpSocket, UdpBuilder)
- `lib/std/src/net/async_tcp.tml` - Async TCP (AsyncTcpListener, AsyncTcpStream, Poll, Waker, Context)
- `lib/std/src/net/async_udp.tml` - Async UDP (AsyncUdpSocket)
- `lib/std/src/net/http.tml` - HTTP protocol types (Method, StatusCode, Headers, Request, Response, HttpClient)
- `lib/core/src/arena.tml` - Arena allocator for request-scoped memory

### Modified Files
- `lib/std/src/mod.tml` - Export net module

## Dependencies

### External
- OpenSSL or BoringSSL for TLS (optional)
- Platform headers for syscalls

### Internal
- `core::alloc` - Memory allocation
- `core::iter` - Iterators
- `core::slice` - Slice operations
- `core::hash` - Hash and Hasher behaviors
- `core::option` - Maybe type
- `core::result` - Outcome type
- `std::collections` - HashMap for headers
- `std::io` - Read/Write behaviors
- `std::sync` - Synchronization primitives
