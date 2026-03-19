# Node.js HTTP Server Architecture: Comprehensive Technical Analysis

**Date**: 2026-03-19
**Scope**: Event loop, HTTP parsing, streams, connections, clustering, performance, and comparative analysis with alternatives.

## 1. Event Loop (libuv)

### Architecture Overview

Node.js uses **libuv**, a cross-platform async I/O library written in C, as the foundation of its event loop. The event loop is **single-threaded** for user JavaScript code, but libuv manages multiple OS-level threads for I/O operations behind the scenes.

### Event Loop Phases

The event loop executes in distinct phases, each with its own queue of callbacks:

1. **TIMERS**: Execute setImmediate(), setTimeout() callbacks
2. **PENDING CALLBACKS**: I/O errors from previous cycle (rarely used)
3. **IDLE/PREPARE**: Prepare for poll phase (mostly internal)
4. **POLL**: Block on I/O events (the heavy lifting)
5. **CHECK**: setImmediate() callbacks
6. **CLOSE**: Cleanup (socket.on('close'))

Then repeat.

### The Poll Phase (Critical for HTTP Servers)

The **poll phase** is where I/O multiplexing happens. This phase:

- **Blocks** (with a calculated timeout) on OS-level I/O multiplexing primitives
- **Processes incoming events**: new TCP connections, readable data on sockets, writable buffer space
- **Executes the corresponding callbacks** (typically in node's net.Server)
- **Will not move to next phase** until the poll queue is empty or maximum callbacks processed

For HTTP servers, this is where `server.on('connection')` callbacks are invoked for each incoming TCP connection, or where `socket.on('data')` fires for readable data.

### I/O Polling per Operating System

#### Linux/Unix

- **epoll** (event poll): O(1) lookup, supports unlimited file descriptors
- System call: `epoll_wait()` blocks until events available
- Memory overhead: minimal (red-black tree in kernel)
- Typical latency: sub-millisecond wake-up on event
- Used by: Linux kernel 2.5.44+ (universally)

#### Windows

- **IOCP** (I/O Completion Ports): Kernel-assisted I/O queuing
- Syscalls: `GetQueuedCompletionStatus()` blocks until completion packets available
- Throughput: Optimized for high concurrency (tens of thousands of connections)
- Typical latency: microsecond-scale wake-up
- Thread pool: libuv uses a dedicated thread pool for I/O (default 4 threads, configurable via UV_THREADPOOL_SIZE)

Key advantage: Scalability with thousands of concurrent connections without per-connection overhead.

#### macOS

- **kqueue**: BSD-style keyed event notification
- System call: `kevent()` blocks until events available
- Similar performance to epoll
- Supports file events, timers, signals, and more

### Event Loop Latency Characteristics

- **Typical blocking time in poll phase**: 10-100ms (depends on timeout parameter)
- **Microtask queue (Promises)**: Drained **between phases**, not within phases
- **I/O event latency**: Sub-millisecond from OS event to JavaScript callback invocation
- **Maximum uninterrupted blocking**: Limited by poll timeout + callback execution time

## 2. HTTP Parser (llhttp)

### Evolution and Motivation

Node.js originally used **http_parser** (developed by Ryan Dahl for Node.js). In 2019, it was replaced with **llhttp** (Low-Level HTTP), which maintains API compatibility while improving performance and correctness.

### llhttp Design Principles

- **State machine-based**: Rigorous HTTP/1.1 RFC 7230 compliance
- **Zero-copy**: Never copies parsed headers into temporary buffers; points directly into the input buffer
- **Callback-based**: Minimal memory allocation; client code handles each parsed field
- **Reusable**: Single parser instance can parse multiple requests (pipelined or keep-alive)

### Parsing Performance

- **Throughput**: ~2-5 GB/s on modern CPUs (single-threaded)
- **Per-header latency**: Microseconds (simple state transitions, minimal branching)
- **Memory overhead**: ~5 KB per parser instance

### Handling Pipelining and Keep-Alive

For HTTP/1.1 keep-alive connections, after one request is fully parsed and handled:

parser.reinitialize();  // Reset state machine

Next request's bytes are already in the buffer, parsed immediately. This allows multiple requests on a single TCP connection with zero inter-request latency.

## 3. Buffer Management

### Buffer Class Design

Node.js `Buffer` is a typed array backed by allocated C++ memory. Unlike JavaScript strings (UTF-16, immutable), buffers are:

- **Mutable**: Can modify bytes in-place
- **Efficient**: Direct memory access, no overhead
- **Pooling**: Small allocations (<= 16 KB) use a shared pool to reduce GC pressure

### String Encoding and HTTP Headers

HTTP headers are transmitted as **UTF-8 bytes** in the network stream. Node.js handles encoding/decoding with O(n) complexity for string length. For HTTP headers, this is negligible (typically < 10 KB per request).

### Backpressure and Flow Control

When a server is slow to process data, the TCP receive buffer fills up. Node.js implements backpressure:

- **Without backpressure**: Memory grows unboundedly (server processes slower than data arrives)
- **With backpressure**: TCP flow control (TCP window size reduced by receiver) causes sender to slow down

## 4. Stream Architecture

### Readable, Writable, Duplex, Transform Streams

Node.js streams abstract I/O into a unified interface with internal buffering:

- **Readable Stream**: Produces data (e.g., socket, file)
- **Writable Stream**: Consumes data
- **Duplex Stream**: Both directions
- **Transform Stream**: Modifies data in transit

Default highWaterMark: 16 KB for sockets.

### HTTP Request and Response as Streams

In Node.js HTTP:

```javascript
const server = http.createServer((req, res) => {
  // 'req' is a Readable stream (IncomingMessage)
  // 'res' is a Writable stream (ServerResponse)
});
```

Each stream maintains internal buffering with automatic backpressure handling.

## 5. Connection Handling

### net.Server Architecture

1. `net.Server` creates a listening socket on port
2. In the event loop's **poll phase**, libuv detects incoming connections
3. `accept()` syscall retrieves a new socket file descriptor
4. Node.js wraps it in a `net.Socket` object
5. User callback is invoked with the socket

### Socket Pooling and Keep-Alive

HTTP/1.1 **keep-alive** allows multiple requests on one connection.

Client-side pooling via `http.Agent`:
- **keepAlive**: true
- **maxSockets**: 50 (concurrent connections per host)
- **maxFreeSockets**: 10 (idle sockets kept alive)
- **keepAliveMsecs**: 30000 (TCP keep-alive probe interval)
- **timeout**: 60000 (socket timeout)

Server side: Keep-alive is automatic (no per-connection pooling needed).

### Connection Lifecycle

```
[IDLE] → [ESTABLISHED] → [READING] → [WRITING] → [KEEP-ALIVE] or [CLOSED]
```

Keep-alive timeout: If no new request received within 5-30 seconds, server closes connection.

## 6. Clustering and Load Balancing

### cluster Module Design

Master process forks worker processes (one per CPU core). All workers listen on same port:

1. Master creates a listening socket on port in each worker (each has its own file descriptor)
2. Kernel maintains a single TCP listen queue for port
3. Load balancing happens at the OS level

### How Multiple Workers on Same Port Works

**Linux 4.5+ (SO_REUSEPORT)**:
- Multiple processes can listen; kernel distributes connections fairly
- Latency: No IPC overhead; each worker processes its own connections
- Fairness: Kernel distributes connections to available workers
- Scalability: Excellent up to 8-16 workers (CPU cores)

**Older systems (Thundering herd)**:
- All workers wake up, but only one accepts each connection
- Less efficient than SO_REUSEPORT

### Worker Communication (IPC)

- **Throughput**: ~100K messages/sec per IPC channel
- **Latency**: 100-500 microseconds
- **Use case**: Statistics, monitoring (not per-request overhead)

## 7. Performance Characteristics

### Typical Throughput

#### Single Process

- Requests/sec: 15,000 - 30,000 req/s
- Response time: 0.5-2 ms
- Memory: 30-50 MB base

#### 8-Process Cluster

- Total throughput: 100,000 - 150,000 req/s
- Per-worker: 12,500 - 18,750 req/s (lower due to IPC overhead)

### Memory per Connection

- Idle socket: 160 bytes
- Active socket (64 KB buffer): 64 KB + 160 bytes
- Request + headers: 2-4 KB

Total: 5-100 KB per concurrent connection.
For 10,000 concurrent keep-alive: 500 MB - 1 GB memory.

### Event Loop Latency

Under load:
- Busy event loop: 50-100 ms between poll cycles
- Idle event loop: sub-1 ms between idle iterations

Blocked by:
1. Synchronous user code (most severe)
2. Garbage collection pauses (10-50 ms)
3. Large I/O callbacks (parsing, serialization)

## 8. Express vs Fastify

### Express (Mature)

- Route matching: Linear O(n)
- Router latency: 1-2 ms for 100 routes
- Middleware overhead: High
- Throughput: 5,000-10,000 req/s
- Strengths: Large ecosystem, easy
- Weaknesses: O(n) routing, overhead

### Fastify (High-Performance)

- Router: Radix tree O(1)
- Router latency: <100 microseconds
- Validation: Built-in schema
- Serialization: Pre-compiled
- Throughput: 50,000-100,000 req/s
- Overhead: 5-10x less than Express

Radix tree advantage: 1000 routes <1 microsecond vs Express 50+ microseconds.

| Metric | Express | Fastify |
|--------|---------|---------|
| Route lookup | O(n) | O(1) |
| Latency (100 routes) | 1-2 ms | <100 µs |
| Middleware | High | Low |
| Validation | Manual | Built-in |
| Throughput | 10K req/s | 50-80K req/s |

## 9. Worker Threads

Node.js worker_threads enable true parallelism for CPU-bound work:

- True parallelism: Separate CPU cores
- Overhead: 10-50 MB per worker, 1-5 ms startup
- Latency: 100-500 microsecond round-trip
- No shared mutable state: Message passing only

When to use:
- Crypto (bcrypt, argon2)
- Image processing
- Data compression
- Complex math

When NOT to use:
- Fast work (<100 µs) - overhead dominates
- I/O operations - event loop handles
- Large messages - serialization overhead

## 10. Key Design Decisions

### Why Single-Threaded Event Loop Works

Node.js succeeds because most server workloads are I/O-bound:

Typical HTTP request timeline:
- 0 ms: Request arrives
- 0.5 ms: Parsed by llhttp
- 1 ms: DB query initiated (async)
- 5 ms: Other requests parsed
- 10 ms: DB response, callback fires, response sent

Multi-threaded (Java):
- Thread 1: Request, DB query, wait (sleeps)
- Thread 2: Request, DB query, wait (sleeps)
- CPU: Idle

Event loop benefits:
1. Simplicity (no locks)
2. Memory efficiency
3. Zero context switching
4. Simpler GC
5. Sequential async/await code

## 11. Weaknesses and Limitations

### 1. CPU-Bound Blocking

Synchronous code blocks entire event loop:

```javascript
bcrypt.hashSync(password, 10);  // Blocks 100-500 ms
```

Other requests queue up. Solution: async version or worker.

### 2. Callback/Promise Overhead

Every async operation allocates:
- Function closure
- Promise object
- Microtask entry
- Call stack frame

10,000 req/s × 2 ops = 20,000 promises/sec (GC pressure).

### 3. Garbage Collection Pauses

V8 Mark-Compact causes 10-100 ms spikes for 100 MB heaps.

Low-latency (<10 ms p99) requires GC tuning.

### 4. No Main Thread Parallelism

All user code single-threaded. Promise.all() sequential:
- 0-50 ms: func1
- 50-100 ms: func2
- Total: 100 ms

With workers: Both run in parallel (50 ms).

### 5. Heap Fragmentation

Long-lived objects cause fragmentation.
Mitigation: Regular process restarts.

### 6. Microtask Queue Processing

All microtasks drain before next event loop phase:

Promise.resolve().then(() => {})...  
// All must complete before poll continues

## 12. Comparative Performance

### Node.js vs Go

| Aspect | Node.js | Go |
|--------|---------|-----|
| Throughput | 25K req/s | 100K req/s |
| Memory/conn | 5 KB | 1 KB |
| Startup | 100-200 ms | 1-5 ms |
| GC pauses | 10-100 ms | 1-10 ms |

Go advantages: Parallelism, static binary, lower footprint
Node.js: Ecosystem, async/await, rapid prototyping

### Node.js vs Rust

| Aspect | Node.js | Rust |
|--------|---------|------|
| Throughput | 25K req/s | 150K+ req/s |
| Memory/conn | 5 KB | <1 KB |
| GC pauses | 10-100 ms | 0 (no GC) |
| Dev time | Fast | Slow |

Rust advantages: No GC, zero-cost, memory safety
Node.js: Easier, ecosystem, simpler

## Conclusion

Node.js HTTP servers excel at **I/O-bound workloads** with **simple concurrency** (async/await). The single-threaded event loop, libuv multiplexing, and stream architecture make it ideal for APIs and web servers.

For **low-latency** (< 5 ms p99) or **CPU-bound** work, Go, Rust, or compiled languages are superior. Node.js shines when developer velocity and ecosystem matter more than absolute performance.

The evolution from Express (simple, flexible) to Fastify (optimized, schema-driven) reflects maturing production use cases, where framework overhead and router efficiency become critical at scale.

