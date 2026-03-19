# Go net/http Architecture: Comprehensive Technical Analysis

**Date**: 2026-03-19
**Scope**: Goroutine model, netpoller, HTTP server, buffer management, performance

## Executive Summary

Go's net/http server represents a fundamentally different approach to concurrent network programming. By combining lightweight goroutines (M:N scheduled green threads), transparent I/O multiplexing via the netpoller, and a simple handler interface, Go achieves high throughput with minimal code complexity. Go powers production HTTP infrastructure at Google, Cloudflare, Uber, and Twitch.

**Key Performance Targets:**
- net/http hello world: 80,000-120,000 req/s (single process)
- Memory per goroutine: 2KB initial stack + ~200 bytes scheduler metadata
- 10K concurrent connections: ~80 MB memory
- Latency: p50=80-100us, p99=1-5ms

---

## 1. Goroutine Model: M:N Scheduling

### The Three Components: G, M, P

Go's runtime uses an M:N scheduler to multiplex millions of goroutines onto OS threads:

- **G (Goroutine)**: Logical execution context. Stack starts at **2KB**, grows dynamically up to 1GB. Creating a goroutine costs ~200 bytes in scheduler metadata + 2KB stack = ~2.2KB total. Can create millions.

- **M (Machine/OS Thread)**: Actual OS thread. Go creates as many as needed (default max: 10,000). Each M runs one G at a time. When a G blocks on a syscall, the M is parked and a new M is created.

- **P (Processor)**: Logical CPU context. Default = `GOMAXPROCS` (number of CPU cores). Each P has a local run queue of Gs (max 256). A G cannot execute without being assigned to a P.

### Scheduling Algorithm

```
Per-P local run queue (LIFO, 256 slots)
  -> If empty: steal from another P's queue (take half)
    -> If empty: check global run queue (FIFO)
      -> If empty: poll network (netpoll)
        -> If empty: steal from another P again
```

**Key properties:**
- **Work stealing**: Idle Ps steal from busy Ps — load balances automatically
- **Preemptive** (since Go 1.14): Goroutines preempted at function call points AND via async signals (SIGURG on Linux)
- **Context switch cost**: ~100-200ns (vs ~1-5us for OS thread switch)

### Stack Growth

Goroutine stacks are contiguous (since Go 1.3), copied on growth:

1. Start at 2KB
2. When stack overflows, allocate new stack 2x the size
3. Copy entire old stack to new stack (update all pointers)
4. Free old stack

This enables millions of goroutines with minimal memory waste.

---

## 2. Netpoller: Transparent Async I/O

### How It Works

The netpoller is Go's runtime-level I/O multiplexer. It makes blocking I/O calls appear synchronous to the programmer while being async under the hood:

```go
// Programmer writes this (blocking-style):
conn, err := listener.Accept()  // Blocks until connection arrives
data := make([]byte, 4096)
n, err := conn.Read(data)       // Blocks until data available

// Runtime actually does this:
// 1. Register socket with epoll/kqueue/IOCP
// 2. Park the goroutine (remove from P's run queue)
// 3. When I/O ready, netpoll callback unparks the goroutine
// 4. Goroutine resumes on any available P
```

### Per-OS Implementation

| OS | Backend | Registration | Polling |
|----|---------|-------------|---------|
| Linux | epoll | `epoll_ctl(EPOLL_CTL_ADD)` | `epoll_wait()` |
| macOS/BSD | kqueue | `kevent(EV_ADD)` | `kevent()` |
| Windows | IOCP | `CreateIoCompletionPort()` | `GetQueuedCompletionStatusEx()` |

### Integration with Scheduler

The netpoller runs as part of the scheduler loop:

1. Every scheduling decision checks the netpoll queue
2. If timer-based polling: check every ~10ms
3. When a P has no work: block on `epoll_wait()` with timeout
4. Ready goroutines are added to the P's local run queue

**Key insight**: The programmer writes simple, blocking-style code. The runtime transparently converts all socket I/O to non-blocking + epoll/kqueue/IOCP. This is Go's killer feature for network servers.

---

## 3. HTTP Server Architecture

### Server Structure

```go
type Server struct {
    Addr         string        // ":8080"
    Handler      Handler       // Usually a ServeMux
    ReadTimeout  time.Duration
    WriteTimeout time.Duration
    IdleTimeout  time.Duration
    MaxHeaderBytes int         // Default: 1MB
}
```

### Connection Handling

```
ListenAndServe()
  -> net.Listen("tcp", addr)
    -> for { conn, _ := listener.Accept() }
      -> go c.serve(ctx)   // One goroutine per connection
        -> Read request (bufio.Reader, 4KB buffer)
        -> Parse headers (textproto)
        -> Match handler (ServeMux)
        -> Call handler
        -> Write response (bufio.Writer, 4KB buffer)
        -> Keep-alive? -> loop back to Read
        -> Close connection
```

**Goroutine-per-connection**: Each accepted connection spawns a new goroutine. Simple, efficient, and scalable to 100K+ connections because goroutines are cheap.

### Request Parsing

```go
// textproto.Reader reads line-by-line:
line, err := tp.ReadLine()     // "GET /users/42 HTTP/1.1"
// Split: method, requestURI, proto

// Headers read via textproto.ReadMIMEHeader():
// Canonicalizes keys: "content-type" -> "Content-Type"
// Stored in map[string][]string (Header type)

// Body: wrapped in LimitedReader (Content-Length) or chunkedReader
```

### Default ServeMux

```go
mux.HandleFunc("/", rootHandler)           // Catch-all
mux.HandleFunc("/users/", usersHandler)    // Prefix match
mux.HandleFunc("/api/v1/", apiHandler)     // Longer prefix wins
```

Matching algorithm: O(n) linear scan of registered patterns. Longest match wins.

### Go 1.22+ Enhanced ServeMux

```go
mux.HandleFunc("GET /users/{id}", getUser)
mux.HandleFunc("POST /users", createUser)
mux.HandleFunc("DELETE /users/{id}/posts/{postID}", deletePost)
```

Now supports:
- **Method matching**: `GET /path` only matches GET
- **Path parameters**: `{name}` captures path segments
- **Wildcards**: `{path...}` captures remaining path
- **Priority**: More specific patterns win

---

## 4. HTTP/2 Support

### Automatic Upgrade

```go
// HTTP/2 is automatic for HTTPS:
server.ListenAndServeTLS("cert.pem", "key.pem")
// ALPN negotiation selects "h2" during TLS handshake
```

### Implementation (x/net/http2)

- **Stream multiplexing**: Multiple request/response pairs on one TCP connection
- **Flow control**: Per-stream and per-connection window sizes
- **HPACK**: Header compression with dynamic table
- **Server push**: Via `http.Pusher` interface (rarely used)
- **Transparent**: Same `Handler` interface for HTTP/1.1 and HTTP/2

---

## 5. Buffer Management

### bufio.Reader and bufio.Writer

Every HTTP connection uses buffered I/O:

```go
// Default buffer sizes:
br := bufio.NewReaderSize(conn, 4096)  // 4KB read buffer
bw := bufio.NewWriterSize(conn, 4096)  // 4KB write buffer
```

### sync.Pool for Buffer Reuse

Go's standard library extensively uses `sync.Pool` to recycle buffers:

```go
var bufPool = sync.Pool{
    New: func() interface{} {
        return make([]byte, 32*1024) // 32KB buffers
    },
}

buf := bufPool.Get().([]byte)
defer bufPool.Put(buf)
```

Properties:
- **Per-P cache**: Each P has its own pool shard — no contention
- **GC-aware**: Pool entries are cleared on GC (prevents unbounded growth)
- **Used by**: net/http (request bodies, response writers), fmt (print buffers)

### io.Reader/io.Writer Interfaces

```go
type Reader interface { Read(p []byte) (n int, err error) }
type Writer interface { Write(p []byte) (n int, err error) }
```

Enable zero-copy pipelines:
```go
io.Copy(responseWriter, file)
// On Linux: uses sendfile() syscall — zero user-space copies
```

### Memory per Connection

| Component | Size |
|-----------|------|
| Goroutine stack | 2-8 KB |
| Scheduler metadata | ~200 bytes |
| bufio.Reader | 4 KB |
| bufio.Writer | 4 KB |
| Request headers (parsed) | 1-4 KB |
| **Total** | **~12-20 KB** |

For 10,000 connections: ~120-200 MB
For 100,000 connections: ~1.2-2 GB

---

## 6. Third-Party Routers

### Default ServeMux Limitations

- O(n) pattern matching (linear scan)
- No path parameters until Go 1.22
- No method-based routing until Go 1.22
- No middleware support
- No route groups

### Chi (Trie-Based Router)

```go
r := chi.NewRouter()
r.Use(middleware.Logger)
r.Route("/users/{id}", func(r chi.Router) {
    r.Get("/", getUser)
    r.Put("/", updateUser)
})
```

- **Router**: Radix trie with method trees
- **Performance**: ~95K req/s (hello world)
- **Middleware**: Compatible with net/http middleware

### Gin (httprouter-Based)

```go
r := gin.Default()
r.GET("/users/:id", func(c *gin.Context) {
    id := c.Param("id")
    c.JSON(200, gin.H{"id": id})
})
```

- **Router**: httprouter — radix tree, ~40K routes/sec insertion
- **Performance**: ~100K req/s (hello world)
- **Allocation**: Pre-allocated context pool (sync.Pool)

### Router Performance Comparison

| Router | Lookup Time | Memory/Route | req/s |
|--------|------------|-------------|-------|
| Default ServeMux | O(n) | ~200 bytes | 80K |
| Chi | O(path_len) | ~300 bytes | 95K |
| Gin/httprouter | O(path_len) | ~150 bytes | 100K |

---

## 7. Context and Cancellation

### Request-Scoped Context

```go
func handler(w http.ResponseWriter, r *http.Request) {
    ctx := r.Context() // Cancelled when client disconnects

    ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
    defer cancel()

    result, err := db.QueryContext(ctx, "SELECT ...")
    // If client disconnects or timeout: ctx.Done() closes
}
```

### Graceful Shutdown

```go
server.Shutdown(ctx)
// 1. Stop accepting new connections
// 2. Wait for active requests to complete
// 3. Close idle connections immediately
// 4. Return when all connections closed
```

---

## 8. Performance Characteristics

### Throughput Benchmarks

| Scenario | req/s (single process) | Notes |
|----------|----------------------|-------|
| Hello world (plaintext) | 80,000-120,000 | Default net/http |
| JSON response | 60,000-90,000 | encoding/json |
| DB query (PostgreSQL) | 15,000-30,000 | pgx driver |
| Static file | 40,000-60,000 | http.FileServer |

### TechEmpower Benchmarks (Go vs Others)

| Framework | Plaintext | JSON | DB |
|-----------|-----------|------|-----|
| Go (net/http) | 500K req/s | 300K req/s | 80K req/s |
| Go (fasthttp) | 1.5M req/s | 600K req/s | 100K req/s |
| Rust (Hyper) | 7M+ req/s | 900K req/s | 150K req/s |
| Node.js | 300K req/s | 200K req/s | 50K req/s |

*Note: TechEmpower uses pipelining, multiple connections, optimized configs.*

### Goroutine Overhead

| Metric | Value |
|--------|-------|
| Creation cost | ~300ns + 2KB stack |
| Context switch | 100-200ns |
| Scheduler overhead | <1% CPU at 100K goroutines |
| Stack growth | ~1us (copy + rewrite pointers) |
| Max goroutines | Millions (limited by memory) |

### Latency Distribution

| Percentile | Latency |
|-----------|---------|
| p50 | 80-100 us |
| p95 | 500 us - 1 ms |
| p99 | 1-5 ms |
| p99.9 | 5-20 ms (includes GC pauses) |

---

## 9. Key Design Decisions

### Why Goroutine-Per-Connection Works

| Property | OS Thread | Goroutine |
|----------|-----------|-----------|
| Stack size | 1-8 MB | 2 KB (grows to 1 GB) |
| Creation cost | ~10us | ~300ns |
| Context switch | 1-5 us | 100-200 ns |
| Max concurrent | ~10K | ~1M+ |
| Memory (10K) | 10-80 GB | ~20-80 MB |

### Blocking-Style Code with Async I/O

Go's greatest insight: programmers write blocking code, runtime provides async I/O.

```go
data, err := ioutil.ReadAll(r.Body)  // Goroutine parked while waiting
result := processData(data)           // CPU work — goroutine runs on P
conn.Write(result)                    // Goroutine parked while writing
```

No callbacks, no promises, no async/await keywords needed.

### Static Binary

`go build` produces a single binary:
- No shared libraries needed
- Cross-compilation built-in (`GOOS=linux GOARCH=amd64`)
- Deployment: copy one file

### Fast Compilation

- Full rebuild: 5-30 seconds (even large projects)
- Incremental: 1-5 seconds

---

## 10. Weaknesses

### GC Pauses

Go's garbage collector has improved dramatically:
- **Go 1.5**: 10-100ms pauses
- **Go 1.8+**: Sub-millisecond pauses (usually <500us)
- **Go 1.19+**: Soft memory limit, better GC tuning

Still: under high allocation rates, GC can consume 5-10% CPU. For latency-sensitive (p99.9 < 1ms), GC pauses remain a concern.

### Limited Memory Layout Control

```go
type MyStruct struct {
    A bool   // 1 byte + 7 padding
    B int64  // 8 bytes
    C bool   // 1 byte + 7 padding
}
// Size: 24 bytes (vs optimal 10 bytes)
```

No packed structs, no union types, no manual memory management.

### No Zero-Copy I/O Primitives

```go
buf := make([]byte, 4096)
n, err := conn.Read(buf) // Copies from kernel buffer to userspace

// No equivalent of Rust's Bytes (reference-counted zero-copy slicing)
// No writev/readv exposure in standard library
```

### Interface Boxing

```go
var w io.Writer = conn // Allocates 16-byte interface value
w.Write(data)          // Virtual call — no inlining possible
```

### Error Handling Verbosity

```go
result, err := doSomething()
if err != nil {
    return nil, err
}
// Repeated for every fallible operation
```

---

## 11. Lessons for TML

### What TML Can Adopt

1. **Goroutine-per-connection model**: Simple programming model, no callback hell
2. **Transparent netpoller**: User writes blocking code, runtime handles async I/O
3. **sync.Pool pattern**: Per-worker buffer pools to avoid allocation
4. **Graceful shutdown**: `Server.Shutdown(ctx)` pattern
5. **Context propagation**: Request-scoped cancellation and timeouts

### Where TML Can Improve

1. **No GC**: TML's ownership model avoids GC pauses — guaranteed sub-microsecond latency
2. **Memory layout control**: TML can pack structs optimally (like Rust)
3. **Zero-copy buffers**: Reference-counted Bytes-like type in standard library
4. **Inlined behaviors**: TML's behavior system can monomorphize, avoiding interface boxing
5. **SIMD parsing**: TML can use LLVM's SIMD intrinsics for HTTP header scanning

---

## References

- https://go.dev/doc/
- https://go.dev/blog/io2013-talk-concurrency
- https://go.dev/src/net/http/
- https://go.dev/src/runtime/proc.go (scheduler)
- https://go.dev/src/runtime/netpoll_epoll.go
- https://www.techempower.com/benchmarks/
- https://github.com/go-chi/chi
- https://github.com/gin-gonic/gin
