# Proposal: Network Standard Library (TCP, UDP, HTTP)

## Status: PROPOSED

## Why

TML currently lacks networking capabilities, which limits its use for building modern applications such as:
- Web servers and APIs
- Microservices
- Real-time communication systems
- Network utilities and tools

A high-performance, memory-safe networking library is essential for TML to compete with languages like Rust and Go in the systems programming space.

## What Changes

### New Module: `std::net`

Add a comprehensive networking standard library with:

1. **Low-Level Socket API** (`std::net::socket`)
   - TCP client/server sockets
   - UDP datagram sockets
   - Unix domain sockets (POSIX)
   - Non-blocking I/O with async/await integration
   - Zero-copy buffer management

2. **HTTP Module** (`std::net::http`)
   - HTTP/1.1 and HTTP/2 support
   - Request/Response types with streaming bodies
   - Connection pooling and keep-alive
   - TLS/SSL integration

3. **Decorator-Based Server Framework** (`std::net::server`)
   - NestJS-inspired decorators for routing
   - `@Controller`, `@Get`, `@Post`, `@Put`, `@Delete`, `@Patch`
   - `@Param`, `@Query`, `@Body`, `@Header` for parameter injection
   - `@Middleware`, `@Guard`, `@Interceptor` for request pipeline
   - Dependency injection support

4. **Performance Features**
   - io_uring on Linux for maximum throughput
   - IOCP on Windows
   - kqueue on macOS/BSD
   - Memory-mapped buffers for large transfers
   - Connection pooling with configurable limits

### Design Principles

| Principle | Implementation |
|-----------|----------------|
| Zero-copy where possible | `BufferView` type for borrowed slices |
| Explicit memory control | `Arena` allocators for request handling |
| Predictable latency | Pre-allocated connection pools |
| Safe defaults | TLS by default, timeouts configured |
| Composable | Middleware pattern, decorator composition |

## Impact

### New Files
- `lib/std/src/net/mod.tml` - Module root
- `lib/std/src/net/socket.tml` - Low-level socket API
- `lib/std/src/net/tcp.tml` - TCP client/server
- `lib/std/src/net/udp.tml` - UDP datagram sockets
- `lib/std/src/net/http/mod.tml` - HTTP module root
- `lib/std/src/net/http/request.tml` - HTTP request type
- `lib/std/src/net/http/response.tml` - HTTP response type
- `lib/std/src/net/http/client.tml` - HTTP client
- `lib/std/src/net/http/server.tml` - HTTP server
- `lib/std/src/net/server/mod.tml` - Decorator-based server
- `lib/std/src/net/server/decorators.tml` - Route decorators
- `lib/std/src/net/server/router.tml` - Request router
- `lib/std/src/net/server/middleware.tml` - Middleware system
- `lib/std/src/net/buffer.tml` - Buffer management
- `lib/std/src/net/addr.tml` - Address types (IPv4, IPv6)

### Modified Files
- `lib/std/src/mod.tml` - Export `net` module

### Runtime Requirements
- Platform-specific syscall wrappers in `compiler/runtime/`
- TLS integration (OpenSSL/BoringSSL or native)

## Success Criteria

1. **Functionality**
   - [ ] TCP echo server handles 10k concurrent connections
   - [ ] UDP server can send/receive datagrams
   - [ ] HTTP server responds to GET/POST requests
   - [ ] Decorators correctly route requests to handlers
   - [ ] Middleware chain executes in correct order

2. **Performance**
   - [ ] HTTP throughput >= 100k req/s on modern hardware
   - [ ] Memory usage stays constant under load (no leaks)
   - [ ] Latency p99 < 10ms for simple requests
   - [ ] Zero-copy path for static file serving

3. **Safety**
   - [ ] All network operations are memory-safe
   - [ ] Timeouts prevent resource exhaustion
   - [ ] Connection limits enforced
   - [ ] TLS configured securely by default

4. **Ergonomics**
   - [ ] Simple echo server in < 10 lines
   - [ ] HTTP API feels familiar to NestJS developers
   - [ ] Error messages are actionable
   - [ ] Documentation with examples for all features

## References

- [NestJS Documentation](https://docs.nestjs.com/)
- [Rust tokio](https://tokio.rs/)
- [Go net/http](https://pkg.go.dev/net/http)
- [io_uring](https://kernel.dk/io_uring.pdf)
