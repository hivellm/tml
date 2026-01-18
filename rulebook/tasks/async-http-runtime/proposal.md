# Async HTTP Runtime - Design Proposal

## Motivation

TML needs a modern, high-performance HTTP stack to be competitive with established languages. The Tokio ecosystem in Rust has proven that async/await with cooperative scheduling provides excellent performance while maintaining ergonomic code.

This proposal outlines an HTTP implementation that:
1. Follows Tokio's proven architecture patterns
2. Uses cooperative polling (Futures) instead of callbacks
3. Provides NestJS-style decorators for developer experience
4. Supports the full HTTP spectrum (HTTP/1.1, HTTP/2, HTTP/3, WebSocket, SSE)

## Design Principles

### 1. Zero-cost abstractions
The async runtime should compile to efficient code with minimal overhead. Futures that resolve immediately should be as fast as synchronous code.

### 2. Cooperative scheduling
Tasks yield control explicitly by returning `Poll::Pending`. This allows fine-grained control and avoids the complexity of preemptive scheduling.

### 3. Work-stealing for scalability
Multi-threaded execution uses work-stealing to balance load across cores without excessive synchronization.

### 4. Decorator-driven API
Following NestJS patterns, HTTP routes are defined declaratively with decorators like `@Controller`, `@Get`, `@Post`, making the code self-documenting.

## Architecture Overview

```
Application Code
     │
     ▼
┌─────────────────┐
│  HTTP Framework │  @Controller, @Get, Middleware, Guards
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  HTTP Protocol  │  HTTP/1.1, HTTP/2, HTTP/3 codecs
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   TLS Layer     │  TLS 1.2/1.3, ALPN
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  Async Runtime  │  Executor, Scheduler, Timers
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  I/O Reactor    │  mio-like event loop
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   OS Layer      │  epoll/kqueue/IOCP
└─────────────────┘
```

## Key Components

### Future Trait
```tml
pub type Poll[T] {
    Ready(T),
    Pending,
}

pub behavior Future {
    type Output
    func poll(mut this, cx: mut ref Context) -> Poll[Self::Output]
}
```

The `Future` trait is the core abstraction. A future represents a value that may not be available yet. When polled, it either returns `Ready(value)` or `Pending` (and arranges for the waker to be called when progress can be made).

### Executor
The executor drives futures to completion by repeatedly polling them when they're ready to make progress.

```tml
impl Runtime {
    pub func block_on[F: Future](this, future: F) -> F::Output {
        // Repeatedly poll until Ready
    }

    pub func spawn[F: Future](this, future: F) -> JoinHandle[F::Output] {
        // Schedule for concurrent execution
    }
}
```

### Event Loop (Reactor)
The reactor monitors I/O resources and wakes tasks when they can make progress. It uses platform-specific APIs:
- **Linux**: `epoll` with edge-triggered mode for efficiency
- **macOS/BSD**: `kqueue`
- **Windows**: IOCP (I/O Completion Ports)

### Timer Wheel
Efficient timeout handling using a hierarchical timer wheel. This allows O(1) insertion and amortized O(1) expiration processing.

## HTTP Framework Design

### Controller-based Routing
```tml
@Controller("/api/users")
pub class UserController {
    @Get("/")
    pub async func list(this) -> Response { ... }

    @Get("/:id")
    pub async func get(this, @Param("id") id: U64) -> Response { ... }

    @Post("/")
    pub async func create(this, @Body dto: CreateUserDto) -> Response { ... }
}
```

### Middleware Pipeline
Middleware wraps handlers to add cross-cutting concerns:
```
Request → Logger → Auth → RateLimit → Handler → Response
                                          ↓
Request ← Logger ← Auth ← RateLimit ← Handler ← Response
```

### Dependency Injection
Services are injected automatically based on type:
```tml
@Injectable
pub class UserService {
    repository: UserRepository,  // Auto-injected
    cache: CacheService,         // Auto-injected
}
```

## Protocol Support

### HTTP/1.1
- Persistent connections (keep-alive)
- Chunked transfer encoding
- Pipeline support (optional)

### HTTP/2
- Binary framing
- Header compression (HPACK)
- Stream multiplexing
- Server push
- Flow control

### HTTP/3
- QUIC transport (UDP-based)
- 0-RTT connection establishment
- Header compression (QPACK)
- Independent stream loss recovery

## Security Considerations

### TLS Configuration
- Default to TLS 1.3, fall back to TLS 1.2
- Strong cipher suites only
- Certificate verification by default
- ALPN for protocol negotiation (h2, http/1.1)

### HTTP Security
- Content-Security-Policy headers
- CORS configuration
- Rate limiting
- Request size limits
- Timeout enforcement

## Performance Targets

Based on Tokio/Hyper benchmarks, targets for a well-optimized TML HTTP server:

| Metric | Target |
|--------|--------|
| Requests/sec (HTTP/1.1) | > 500,000 |
| Requests/sec (HTTP/2) | > 400,000 |
| p99 latency | < 10ms |
| Memory per connection | < 4KB |
| Connection setup time | < 1ms |

## Implementation Phases

### Phase 1: Foundation
- OS abstraction (epoll/kqueue/IOCP)
- Event loop and selector
- Future/Poll types
- Basic executor

### Phase 2: I/O Primitives
- AsyncRead/AsyncWrite traits
- Async TCP/UDP
- Timer implementation
- Buffered I/O

### Phase 3: Multi-threaded Runtime
- Work-stealing scheduler
- Thread pool for blocking ops
- Channels (mpsc, oneshot)

### Phase 4: HTTP Core
- HTTP types and headers
- HTTP/1.1 codec
- Basic server/client

### Phase 5: TLS
- Platform TLS bindings
- Async TLS handshake
- HTTPS support

### Phase 6: HTTP/2
- Frame codec
- HPACK
- Stream management

### Phase 7: Framework
- Router and controllers
- Decorators
- Middleware/Guards/Pipes
- DI container

### Phase 8: Advanced
- WebSocket
- SSE
- HTTP/3 (QUIC)

## Compiler Requirements

The following compiler features are needed:

1. **async/await syntax** - Sugar for Future-returning functions
2. **Associated types** - For `Future::Output`
3. **Pin type** - For self-referential futures
4. **Decorator macros** - For `@Controller`, `@Get`, etc.
5. **Trait objects (dyn)** - For type erasure in handlers

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Compiler bugs block progress | Start with parts that work, parallel compiler fixes |
| Complex async debugging | Good error messages, task tracing |
| Platform differences | Abstract early, test on all platforms |
| Performance issues | Benchmark early and often |

## Success Criteria

1. All standard library tests pass
2. HTTP benchmarks competitive with Rust/Go
3. Framework decorators work intuitively
4. TLS works with real certificates
5. HTTP/2 passes h2spec compliance
6. Documentation and examples complete

## References

- [Tokio internals](https://tokio.rs/tokio/tutorial)
- [Rust async book](https://rust-lang.github.io/async-book/)
- [mio documentation](https://docs.rs/mio)
- [Hyper HTTP library](https://hyper.rs/)
- [NestJS documentation](https://docs.nestjs.com/)
- [HTTP/2 RFC 7540](https://httpwg.org/specs/rfc7540.html)
- [HTTP/3 RFC 9114](https://httpwg.org/specs/rfc9114.html)
