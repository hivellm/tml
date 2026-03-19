# Tokio and Hyper HTTP Server Architecture: Comprehensive Technical Analysis

**Date**: 2026-03-19
**Scope**: Async runtime, HTTP parsing, buffer management, middleware, performance

## Executive Summary

Tokio and Hyper represent the state-of-the-art in asynchronous HTTP server design for compiled languages. Built on Rust's ownership model and async/await syntax, they achieve performance parity with C/C++ servers while providing memory safety guarantees. Hyper powers production systems at Cloudflare, Discord, and AWS (via the Rust SDK).

**Key Performance Targets:**
- Tokio runtime: 200,000+ concurrent connections per machine
- Hyper HTTP/1.1: 80,000-150,000 requests/second (hello world)
- Hyper HTTP/2: 40,000-80,000 requests/second
- Per-connection memory: 12-16 KB (buffer + state)
- Latency: p50=50-70us, p95=200-500us, p99=1ms (local)

---

## 1. Async Runtime (Tokio)

### Work-Stealing Scheduler

Tokio uses a multi-threaded work-stealing scheduler inspired by Go's runtime and Cilk:

- **Worker threads**: Default = number of CPU cores (configurable via `worker_threads`)
- **Task queue**: Each worker has a local LIFO queue (256 slots) + global FIFO queue (shared)
- **Work stealing**: When a worker's local queue is empty, it steals from another worker's queue (takes half the tasks)
- **Scheduling cost**: ~50ns per task poll (measured on modern x86)

### Task Model

Every async operation becomes a `Task` — a heap-allocated state machine:

```rust
// This async function:
async fn handle(req: Request) -> Response {
    let data = db.query(req.id).await;
    Response::new(data)
}

// Compiles to approximately:
enum HandleFuture {
    Start { req: Request },
    WaitingForDb { fut: DbQueryFuture },
    Done,
}
```

Key properties:
- **Zero-cost**: No heap allocation for the Future itself (inlined into parent task)
- **Pinned**: Once polled, a task is pinned in memory (cannot move)
- **Waker-based**: When I/O completes, the waker notifies the scheduler to re-poll

### Runtime Threads vs Blocking Threads

- **Worker threads** (N = CPU cores): Run async tasks. Never block.
- **Blocking thread pool** (default max 512): For `spawn_blocking()` — CPU-heavy or legacy sync code
- **I/O driver thread**: Embedded in worker threads (each worker polls the OS for events)

### Current-Thread Runtime

For single-threaded use cases:
```rust
#[tokio::main(flavor = "current_thread")]
```
All tasks run on one thread. No work stealing. Lower overhead for simple programs.

---

## 2. I/O Driver (mio)

### Cross-Platform Abstraction

Mio wraps OS-specific I/O multiplexing:

| OS | Backend | Syscall |
|----|---------|---------|
| Linux | epoll | `epoll_wait()` |
| macOS/BSD | kqueue | `kevent()` |
| Windows | IOCP + AFD | `GetQueuedCompletionStatusEx()` |

### Readiness-Based Model

Mio uses a **readiness** model (not completion):

1. Register interest: "Tell me when socket X is readable"
2. Poll: `epoll_wait()` returns ready events
3. Perform I/O: `read()` / `write()` — non-blocking, may return `WouldBlock`
4. Re-register interest if `WouldBlock`

This is natural for epoll/kqueue but requires translation for Windows IOCP.

### Windows IOCP via AFD (Critical Detail)

Traditional IOCP is completion-based (issue I/O, get notified when done). Mio converts this to readiness:

1. **AFD Handle**: Mio opens the Ancillary Function Driver (`\Device\Afd`) for each socket
2. **IOCTL_AFD_POLL**: Issues an I/O control request to check readiness (readable/writable)
3. **IOCP notification**: When AFD poll completes, IOCP delivers a completion packet
4. **Readiness translation**: Mio converts the AFD poll result into readiness flags

**Why AFD?** Direct IOCP requires pre-issued I/O buffers (WSARecv/WSASend). AFD polling lets mio check readiness first, then the application issues the actual I/O — matching the epoll model. This is the same approach Tokio, libuv (Node.js), and wepoll use on Windows.

**Performance impact**: AFD polling adds ~1-2us overhead per event vs direct IOCP, but enables the unified readiness API.

---

## 3. Hyper HTTP/1.1

### Connection State Machine

Each HTTP/1.1 connection is a state machine:

```
[Idle] -> [ReadingHeaders] -> [ReadingBody] -> [Dispatching]
       -> [WritingHeaders] -> [WritingBody] -> [KeepAlive/Close]
```

### Parser (httparse)

Hyper uses the `httparse` crate for HTTP/1.x parsing:

- **Zero-copy**: Returns byte slices (&[u8]) pointing into the input buffer
- **No allocation**: Parser state is stack-allocated (~64 bytes)
- **SIMD-accelerated**: Uses SSE2/AVX2 for header scanning on x86
- **Throughput**: ~2-5 GB/s on modern CPUs
- **Max headers**: Configurable (default 100)

```rust
let mut headers = [httparse::EMPTY_HEADER; 64];
let mut req = httparse::Request::new(&mut headers);
let status = req.parse(buf)?; // Returns byte offset consumed
// req.method, req.path, req.headers — all &[u8] slices into buf
```

### Keep-Alive

- HTTP/1.1: Keep-alive by default
- HTTP/1.0: Only if `Connection: keep-alive` header present
- Idle timeout: Configurable (default 90s in Hyper)
- Max requests per connection: Unlimited (server can close at any time)

### Chunked Transfer Encoding

Hyper handles chunked encoding transparently:
- Decoder: Reads chunk-size, chunk-data, CRLF in a state machine
- Encoder: Wraps response body in chunked format when Content-Length unknown

---

## 4. Hyper HTTP/2

### h2 Crate

Hyper delegates HTTP/2 to the `h2` crate:

- **Frame parser**: Binary protocol, 9-byte frame header
- **Stream multiplexing**: Up to 100 concurrent streams per connection (configurable)
- **Flow control**: Per-stream and per-connection window sizes (default 64KB initial)
- **HPACK**: Header compression with dynamic table (4KB default)

### Connection Lifecycle (HTTP/2)

```
[TCP Accept] -> [TLS Handshake + ALPN "h2"]
-> [Connection Preface] -> [SETTINGS exchange]
-> [Multiple streams in parallel]:
    [HEADERS frame] -> [DATA frames] -> [Response HEADERS] -> [Response DATA]
-> [GOAWAY] -> [Close]
```

### Performance vs HTTP/1.1

- **Multiplexing**: Eliminates head-of-line blocking at HTTP layer
- **Header compression**: 30-50% reduction in header size for repeated requests
- **Server push**: Proactive resource delivery (rarely used in practice)
- **Overhead**: Frame parsing adds ~5-10us per request vs HTTP/1.1
- **Throughput**: 40-80K req/s (lower than HTTP/1.1 due to frame overhead for small responses)

---

## 5. Buffer Management (Bytes Crate)

### Bytes — Immutable Reference-Counted Slices

```rust
let data = Bytes::from(vec![1, 2, 3, 4, 5]);
let slice = data.slice(1..4); // [2, 3, 4] — no copy, shared Arc
drop(data); // slice still valid, data freed when last ref dropped
```

Properties:
- **Reference counted**: Arc-based sharing (no copy on slice)
- **Cheap clone**: O(1) — just increments refcount
- **Thread-safe**: Send + Sync
- **Size**: 32 bytes on stack (pointer + length + vtable for dealloc)

### BytesMut — Mutable Buffer

```rust
let mut buf = BytesMut::with_capacity(4096);
buf.extend_from_slice(b"HTTP/1.1 200 OK\r\n");
let frozen: Bytes = buf.freeze(); // Convert to immutable, zero-cost
```

Operations:
- **Reserve**: Grows underlying allocation if needed (amortized O(1))
- **Split**: `buf.split_to(n)` — returns first n bytes as BytesMut, advances cursor
- **Freeze**: Converts to immutable Bytes (no copy, no allocation)

### BufList — Scatter-Gather

For HTTP/2 frame assembly, Hyper uses BufList (linked list of Bytes):
- Avoids copying frames into a contiguous buffer
- Enables vectored I/O (writev) to send multiple buffers in one syscall

### Memory Efficiency

| Operation | Bytes | Traditional Vec |
|-----------|-------|-----------------|
| Slice | O(1), no copy | O(n), copies |
| Clone | O(1), refcount | O(n), copies |
| Send across threads | Zero-copy | O(n), copies |
| Freeze mut to immut | O(1) | N/A |

---

## 6. Connection Lifecycle

### Accept Pipeline

```
Listener (TcpListener::accept())
  -> Spawn task per connection (tokio::spawn)
    -> Optional TLS handshake (tokio-rustls)
      -> HTTP version detection (HTTP/1.1 or HTTP/2 via ALPN)
        -> Protocol-specific connection handler
          -> Keep-alive loop or close
```

### Connection Reuse

- HTTP/1.1: Sequential request-response on same TCP connection
- HTTP/2: Multiplexed streams on same connection
- Connection pool (client-side): Hyper's `Client` maintains pool per host
  - Max idle connections per host: 32 (default)
  - Idle timeout: 90s (default)

---

## 7. Axum Framework

### Architecture

Axum is built on three pillars:

1. **Tower Service trait**: `async fn call(&self, Request) -> Response`
2. **matchit router**: Radix-tree based route matching
3. **Extractors**: Type-safe request decomposition via `FromRequest`

### Router (matchit)

```rust
let app = Router::new()
    .route("/users/:id", get(get_user))
    .route("/users/:id/posts", get(list_posts));
```

matchit uses a radix tree (compressed trie):
- **Lookup**: O(path_length), not O(num_routes)
- **Static routes**: Direct trie traversal
- **Parameters**: `:id` matches single segment
- **Wildcards**: `*path` matches remaining path
- **Performance**: <1us for typical routes

### Extractors (FromRequest)

```rust
async fn create_user(
    State(db): State<DbPool>,        // App state
    Json(body): Json<CreateUser>,     // Parsed JSON body
    headers: HeaderMap,               // Raw headers
) -> impl IntoResponse { ... }
```

Each extractor is independently extracted from the request — can be parallelized.

### Tower Middleware (Layers)

```rust
let app = Router::new()
    .route("/", get(root))
    .layer(TraceLayer::new_for_http())
    .layer(CorsLayer::permissive())
    .layer(CompressionLayer::new())
    .layer(TimeoutLayer::new(Duration::from_secs(30)));
```

Layers compose as nested Service wrappers — zero runtime dispatch overhead (monomorphized at compile time).

---

## 8. Performance Characteristics

### Throughput Benchmarks (TechEmpower)

| Scenario | Hyper (direct) | Axum | Go net/http | Node.js (Fastify) |
|----------|---------------|------|------------|-------------------|
| Plaintext | 7M+ req/s | 5M+ req/s | 500K req/s | 300K req/s |
| JSON | 900K req/s | 700K req/s | 400K req/s | 200K req/s |
| DB query | 150K req/s | 120K req/s | 80K req/s | 50K req/s |
| Fortunes | 120K req/s | 100K req/s | 60K req/s | 40K req/s |

*Note: TechEmpower numbers are with pipelining and multiple connections.*

### Memory Usage

- **Per-connection**: ~12-16 KB (recv buffer + send buffer + state machine)
- **Per-task overhead**: ~256 bytes (scheduler metadata)
- **10K connections**: ~160 MB
- **100K connections**: ~1.6 GB
- **No GC**: Memory freed deterministically when connection closes

### Latency Distribution

| Percentile | Latency |
|-----------|---------|
| p50 | 50-70 us |
| p95 | 200-500 us |
| p99 | 1-2 ms |
| p99.9 | 5-10 ms |

No GC pause spikes — latency distribution is tight and predictable.

---

## 9. Key Design Decisions

### Zero-Cost Async

Rust's async/await compiles to state machines with no heap allocation for simple futures:
- No Future trait object boxing needed
- No vtable dispatch for simple async calls
- Cost: same as hand-written state machine

### Ownership Model Benefits

- **No data races**: Compiler prevents concurrent mutable access at compile time
- **No use-after-free**: Borrow checker ensures references outlive their data
- **No GC pauses**: Deterministic drop — memory freed when scope ends
- **Zero-cost Send/Sync**: Thread safety checked at compile time, no runtime locks needed

### Tower Service Trait

```rust
pub trait Service<Request> {
    type Response;
    type Error;
    type Future: Future<Output = Result<Self::Response, Self::Error>>;
    fn poll_ready(&mut self, cx: &mut Context) -> Poll<Result<(), Self::Error>>;
    fn call(&mut self, req: Request) -> Self::Future;
}
```

Enables:
- **Backpressure**: `poll_ready` allows services to signal overload
- **Composability**: Middleware wraps inner services (like Unix pipes)
- **Type safety**: Compiler verifies middleware chain compatibility
- **Monomorphization**: No dynamic dispatch in optimized builds

---

## 10. Weaknesses

### Complexity

- **Learning curve**: Pin, lifetimes, async traits, Send bounds — significant barrier
- **Error messages**: Async Rust errors are notoriously complex
- **Debug difficulty**: Async stack traces are fragmented across poll points

### Compile Times

- **Monomorphization**: Every generic instantiation generates new code
- **Procedural macros**: Axum's `#[derive]` and routing macros add compile time
- **Typical build**: 30-120 seconds for a medium Axum project (debug mode)
- **Incremental**: 5-15 seconds for changes (after initial build)

### Async Trait Limitations

Before Rust 1.75, `async fn` in traits required boxing:
```rust
#[async_trait]
trait Handler {
    async fn handle(&self, req: Request) -> Response;
    // Compiles to: fn handle(...) -> Pin<Box<dyn Future<...>>>
    // Box allocation per call!
}
```
Rust 1.75+ supports native async fn in traits, eliminating boxing.

### Runtime Overhead for Simple Programs

- Tokio runtime initialization: ~1-2ms
- Worker thread spawn: ~100us each
- For CLI tools or scripts, this overhead is noticeable vs synchronous code

---

## 11. Lessons for TML

### What TML Can Adopt

1. **Readiness-based I/O model**: Even on Windows, convert IOCP completions to readiness checks
2. **Zero-copy buffer management**: Bytes/BytesMut pattern (reference-counted slices)
3. **httparse-style SIMD parsing**: Use SIMD for header scanning
4. **Service trait for middleware**: Composable, type-safe middleware chain
5. **Work-stealing scheduler**: Better CPU utilization than fixed thread assignment

### Where TML Can Improve

1. **Simpler async model**: TML can avoid Pin/Unpin complexity with compiler support
2. **Faster compilation**: TML already compiles faster than Rust
3. **Built-in HTTP/2**: No need for separate crate ecosystem
4. **Unified I/O model**: Single API for all platforms

---

## References

- https://tokio.rs/tokio/tutorial
- https://hyper.rs/
- https://docs.rs/mio/latest/mio/
- https://docs.rs/bytes/latest/bytes/
- https://docs.rs/axum/latest/axum/
- https://www.techempower.com/benchmarks/
