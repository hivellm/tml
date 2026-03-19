# Comparative HTTP Server Analysis: nginx vs Tokio/Hyper vs Node.js vs Go vs TML

**Date**: 2026-03-19
**Purpose**: Identify gaps and improvement opportunities for TML's HTTP server

---

## 1. I/O Model & Concurrency

| Server | Model | Concurrency Strategy | Max Connections |
|--------|-------|---------------------|-----------------|
| **nginx** | Event loop (epoll/kqueue) | Process-per-core, events | 100K+ per worker |
| **Tokio/Hyper** | Async tasks (epoll/kqueue/IOCP) | Work-stealing, Futures | 200K+ per process |
| **Node.js** | Event loop (libuv) | Single-thread + cluster | 10K per worker |
| **Go** | Goroutines (netpoller) | M:N scheduling, goroutine-per-conn | 1M+ goroutines |
| **TML** | Thread pool / IOCP / Event loop | Multiple modes, manual selection | 65K IOCP, unlimited thread pool |

### Gap Analysis

- **TML has 3 modes but none matches best-in-class**: Thread pool is simple but not optimal for C100K. IOCP is Windows-only and limited to 65K slots. Event loop mode is least developed.
- **No work-stealing**: TML uses fixed thread assignment. Tokio's work-stealing scheduler handles load imbalances better.
- **No transparent async**: Go's netpoller makes blocking code async transparently. TML requires explicit mode selection.

### Priority: HIGH
- Improve IOCP mode to handle 100K+ connections (dynamic slot array)
- Add work-stealing to thread pool mode
- Consider Go-style transparent I/O (long-term, requires language-level support)

---

## 2. HTTP Parsing

| Server | Parser | Zero-Copy | SIMD | Throughput | Alloc/Request |
|--------|--------|-----------|------|-----------|---------------|
| **nginx** | Hand-coded state machine | Yes (offsets) | No | ~1-5us/req | 0 (pool) |
| **Tokio/Hyper** | httparse (Rust) | Yes (&[u8] slices) | SSE2/AVX2 | ~2-5 GB/s | 0 (stack) |
| **Node.js** | llhttp | Yes (callbacks) | No | ~2-5 GB/s | ~5KB/parser |
| **Go** | textproto | No (copies to strings) | No | N/A | ~1-4KB/req |
| **TML** | Custom zero-copy | Yes (null-terminate in buffer) | No | Not measured | 0 |

### Gap Analysis

- **No SIMD acceleration**: httparse uses SSE2/AVX2 to scan for delimiters (CRLF, colon, space). This gives 2-5x speedup on header-heavy requests. TML's parser is byte-by-byte.
- **Null-termination is destructive**: TML's parser null-terminates in the recv buffer, which prevents re-scanning the same buffer for pipelining without extra bookkeeping. nginx/httparse use offset-based parsing.
- **No header canonicalization**: Go canonicalizes headers (Content-Type), Hyper preserves original. TML should decide on a strategy.

### Priority: MEDIUM
- Add SIMD-accelerated header scanning (use LLVM vector intrinsics from TML)
- Consider switching from null-termination to offset-based parsing
- Add header canonicalization option

---

## 3. Memory Management

| Server | Strategy | Per-Connection | Per-Request | Buffer Reuse |
|--------|----------|---------------|-------------|--------------|
| **nginx** | Pool allocator (ngx_pool_t) | 6-8 KB | 8-12 KB (pool) | Slab allocator |
| **Tokio/Hyper** | Ownership + Bytes crate | 12-16 KB | 0 (stack) | Arc-based sharing |
| **Node.js** | V8 heap + Buffer pool | 5-100 KB | 2-4 KB | Buffer pooling (<=16KB) |
| **Go** | GC + sync.Pool | 12-20 KB | 1-4 KB | sync.Pool per-P |
| **TML** | Manual (mem_alloc/mem_free) | 64 KB (recv) + 80B (slot) | Varies | No systematic reuse |

### Gap Analysis

- **No buffer pool**: TML allocates 64KB recv buffer per connection but has no buffer pooling/reuse strategy. nginx uses slab allocators, Go uses sync.Pool, Hyper uses Bytes.
- **64KB per connection is large**: nginx uses 6-8KB, Hyper uses 12-16KB. TML's 64KB recv buffer is generous but wastes memory at scale (64KB x 65K connections = 4GB just for buffers).
- **No reference-counted buffers**: Hyper's Bytes crate enables zero-copy slicing with Arc. TML must copy data when passing between components.

### Priority: HIGH
- Implement per-worker buffer pool (arena or slab allocator)
- Reduce default recv buffer to 8-16KB with dynamic growth
- Implement Bytes-like reference-counted buffer type for zero-copy sharing

---

## 4. Router/Dispatch Performance

| Server | Router Type | Lookup Complexity | Param Extraction | Performance |
|--------|------------|-------------------|-----------------|-------------|
| **nginx** | Prefix tree + regex | O(n) for regex | Regex capture groups | 1-5us |
| **Tokio/Axum** | matchit (radix tree) | O(path_len) | During traversal | <1us |
| **Node.js/Fastify** | find-my-way (radix tree) | O(path_len) | During traversal | <1us |
| **Go/Gin** | httprouter (radix tree) | O(path_len) | During traversal | <1us |
| **TML** | Custom radix tree | O(path_len) | During traversal | Not measured |

### Gap Analysis

- **TML's router is already competitive**: Radix tree per method with static/param/wildcard support matches industry standard.
- **Linear child scan**: TML uses linear scan for node children. At typical branching factors (2-5 children) this is fine, but could use sorted array + binary search for deeply branched APIs.
- **No route priority**: When both `/users/:id` and `/users/admin` match, need clear priority rules (static > param > wildcard, which TML already handles).

### Priority: LOW
- Router is solid. Consider benchmarking against find-my-way and httprouter.
- Add route compilation/optimization at startup (flatten common prefixes).

---

## 5. Protocol Support

| Feature | nginx | Tokio/Hyper | Node.js | Go | TML |
|---------|-------|------------|---------|-----|-----|
| HTTP/1.0 | Yes | Yes | Yes | Yes | **Yes** |
| HTTP/1.1 | Yes | Yes | Yes | Yes | **Yes** |
| HTTP/2 | Yes | Yes | Yes (via http2) | Yes (auto) | **No** |
| HTTP/3 (QUIC) | Experimental | Via quinn | Via http3 | Via quic-go | **No** |
| WebSocket | Via module | Via tungstenite | Built-in | Via gorilla | **No** |
| TLS | Yes | Via rustls/openssl | Built-in | Built-in | **No** |
| Keep-Alive | Yes | Yes | Yes | Yes | **Yes** |
| Chunked TE | Yes | Yes | Yes | Yes | **Yes** |
| Compression | Yes | Via tower-http | Via zlib | Via middleware | **Yes** |
| HTTP Pipelining | Yes | Yes | Limited | Limited | **Yes** |

### Gap Analysis

- **No HTTP/2**: All reference servers support HTTP/2. It's essential for modern web (multiplexing, header compression, server push). This is the largest protocol gap.
- **No TLS**: Required for HTTPS and HTTP/2 (ALPN negotiation). Can use @extern("c") to OpenSSL/BCrypt.
- **No WebSocket**: Important for real-time applications (chat, live updates, notifications).
- **Pipelining is TML's advantage**: TML's pipelining detection (100 requests per 64KB buffer) is more aggressive than most servers.

### Priority: HIGH (HTTP/2), MEDIUM (TLS, WebSocket)
- HTTP/2 implementation: frame parser, stream multiplexer, HPACK, flow control
- TLS via @extern("c") to OpenSSL or Schannel (Windows)
- WebSocket upgrade mechanism

---

## 6. Framework API & Middleware

| Server | API Style | Middleware | Type Safety | Error Handling |
|--------|-----------|-----------|-------------|----------------|
| **nginx** | Config-driven | 11-phase pipeline | N/A | Error pages |
| **Tokio/Axum** | Handler functions + extractors | Tower layers (composable) | Full (compile-time) | Result types |
| **Node.js/Express** | Handler functions + middleware | Chain (next()) | None (dynamic) | Error middleware |
| **Go** | Handler interface | HandlerFunc wrapping | Partial | Error returns |
| **TML** | Handler functions | Hooks (disabled) | Partial | Pre-built error responses |

### Gap Analysis

- **Middleware/hooks disabled**: Due to codegen bug (Bool/i1 in ServerResponse through function pointers). This is the #1 blocker for production use.
- **No extractors**: Axum's `FromRequest` pattern is powerful — type-safe decomposition of request into handler parameters. TML handlers receive raw `IncomingMessage` and must manually extract params/body/headers.
- **No error handler customization**: Pre-built 404/500 responses. Need custom error handler support.

### Priority: HIGH
- Fix Bool/i1 codegen bug to enable hooks/middleware
- Add extractor pattern for type-safe request decomposition
- Add custom error handler support

---

## 7. Performance Summary

### Throughput Comparison (Hello World, Plaintext)

| Server | Single Thread/Worker | Multi-Worker | With Pipelining |
|--------|---------------------|-------------|-----------------|
| **nginx** | 20-30K | 150-300K (16 workers) | Yes |
| **Tokio/Hyper** | 150K+ | 5-7M (TechEmpower) | Yes |
| **Node.js** | 15-30K | 100-150K (8 cluster) | Limited |
| **Go** | 80-120K | 500K (TechEmpower) | Limited |
| **TML (thread pool)** | N/A | 169K (with pipelining) | **Yes (aggressive)** |
| **TML (IOCP)** | N/A | 60K | N/A |

### Latency Comparison

| Server | p50 | p99 | p99.9 |
|--------|-----|-----|-------|
| **nginx** | 0.1-0.5 ms | 5-20 ms | Variable |
| **Tokio/Hyper** | 50-70 us | 1-2 ms | 5-10 ms |
| **Node.js** | 0.5-2 ms | 10-50 ms | 100+ ms (GC) |
| **Go** | 80-100 us | 1-5 ms | 5-20 ms (GC) |
| **TML** | Not measured | Not measured | Not measured |

### Memory Efficiency

| Server | Per-Connection | 10K Connections | GC |
|--------|---------------|-----------------|-----|
| **nginx** | 6-8 KB | 60-80 MB | No |
| **Tokio/Hyper** | 12-16 KB | 160 MB | No |
| **Node.js** | 5-100 KB | 500 MB-1 GB | Yes |
| **Go** | 12-20 KB | 120-200 MB | Yes |
| **TML** | 64 KB + 80 B | ~640 MB | No |

---

## 8. Consolidated Gap Analysis

### Critical (Blocks Production Use)

| # | Gap | Impact | Reference | Effort |
|---|-----|--------|-----------|--------|
| 1 | **Middleware/hooks disabled** (codegen bug) | No logging, auth, rate limiting in pipeline | All servers have middleware | Compiler fix |
| 2 | **No HTTP/2** | Cannot serve modern browsers efficiently | All reference servers | Large (frame parser, multiplexer, HPACK) |
| 3 | **No TLS** | No HTTPS, blocks HTTP/2 ALPN | All reference servers | Medium (@extern to OpenSSL) |

### High Impact (Significant Performance/Capability Gain)

| # | Gap | Impact | Reference | Effort |
|---|-----|--------|-----------|--------|
| 4 | **64KB per-connection buffer** | 4GB at 65K connections | nginx 6-8KB, Hyper 12-16KB | Small (reduce + dynamic growth) |
| 5 | **No buffer pool** | malloc/free per connection | nginx slab, Go sync.Pool | Medium |
| 6 | **No work-stealing** | Unbalanced load across workers | Tokio, Go | Medium |
| 7 | **IOCP 65K connection limit** | Hard cap on concurrent connections | No limit in reference servers | Medium (dynamic array) |
| 8 | **No latency measurement** | Cannot benchmark improvements | All measure p50/p99/p99.9 | Small |

### Medium Impact (Production Readiness)

| # | Gap | Impact | Reference | Effort |
|---|-----|--------|-----------|--------|
| 9 | **No WebSocket** | No real-time support | All reference servers | Medium |
| 10 | **No SIMD parsing** | 2-5x slower header parsing | httparse (Hyper) | Medium |
| 11 | **No graceful shutdown** | Connections dropped on stop | Go, Hyper | Small |
| 12 | **No request timeout** | Slow clients hold connections | All reference servers | Small |
| 13 | **No backpressure** | Memory grows with slow clients | Node.js, Hyper | Medium |

### Low Impact (Polish)

| # | Gap | Impact | Reference | Effort |
|---|-----|--------|-----------|--------|
| 14 | **No sendfile()** | Extra copy for static files | nginx, Go | Small |
| 15 | **No vectored I/O (writev)** | Multiple syscalls for fragmented response | Hyper BufList | Small |
| 16 | **No connection draining** | No graceful connection close | nginx, Go | Small |

---

## 9. Action Plan: Prioritized Improvements

### Phase 1: Unblock Production (Weeks 1-2)

1. **Fix Bool/i1 codegen bug** — Enable ServerResponse through function pointers
   - Unblocks: Middleware, hooks, custom error handlers
   - Files: compiler codegen (struct field layout for Bool/i1)

2. **Reduce recv buffer to 8KB with dynamic growth**
   - Currently: 64KB per connection (wastes memory)
   - Target: 8KB initial, grow to 64KB on demand
   - Files: `iocp_worker.tml`, `worker.tml`

3. **Add latency measurement (p50/p95/p99)**
   - Instrument request processing pipeline
   - Output in server startup/shutdown stats
   - Files: `dispatch.tml`, `worker.tml`

4. **Add graceful shutdown**
   - Stop accepting, drain active connections, timeout
   - Files: `server.tml`, `worker.tml`, `iocp_worker.tml`

### Phase 2: Performance (Weeks 3-4)

5. **Implement buffer pool (arena allocator)**
   - Per-worker buffer pool for recv/send buffers
   - Recycle on connection close
   - Target: 0 malloc/free during steady-state operation
   - Files: New `lib/std/src/http/buffer_pool.tml`

6. **Add work-stealing to thread pool mode**
   - Each worker has local queue, steal from others when idle
   - Model: Tokio's LIFO local + FIFO global + steal-half
   - Files: `worker.tml`

7. **Dynamic IOCP connection slots**
   - Replace fixed 65K array with growable array (realloc on demand)
   - Start at 4K, grow to 256K
   - Files: `iocp_worker.tml`

8. **Add request/connection timeouts**
   - Read timeout, write timeout, idle timeout (configurable)
   - Files: `worker.tml`, `iocp_worker.tml`, `app.tml`

### Phase 3: Protocol (Weeks 5-8)

9. **TLS via @extern("c") to Schannel/OpenSSL**
   - Schannel on Windows (native), OpenSSL on Linux
   - ALPN negotiation for HTTP/2
   - Files: New `lib/std/src/http/tls.tml`

10. **HTTP/2 implementation**
    - Frame parser (binary protocol, 9-byte header)
    - Stream multiplexer (SETTINGS, HEADERS, DATA, RST_STREAM, GOAWAY)
    - HPACK header compression
    - Flow control (WINDOW_UPDATE)
    - Files: New `lib/std/src/http/h2/` module

11. **WebSocket upgrade**
    - HTTP upgrade mechanism
    - Frame parser (text, binary, ping, pong, close)
    - Files: New `lib/std/src/http/ws.tml`

### Phase 4: Optimization (Weeks 9-10)

12. **SIMD-accelerated header parsing**
    - Use LLVM vector intrinsics for scanning CRLF, colon, space
    - Target: 2-5x speedup on header-heavy requests
    - Files: `parse.tml` (use lowlevel block for SIMD)

13. **sendfile() for static files**
    - Zero-copy file serving via @extern("c") to sendfile/TransmitFile
    - Files: `static.tml`

14. **Vectored I/O (writev/WSASend with multiple buffers)**
    - Send headers + body in single syscall
    - Files: `worker.tml`, `iocp_worker.tml`

---

## 10. Key Architectural Insights from Reference Servers

### From nginx:
- **Per-request memory pools** — allocate from pool, free entire pool at once. Eliminates per-object free() calls. TML should implement this for request processing.
- **11-phase pipeline** — clean separation of concerns. TML's hook system is similar but needs to be enabled.
- **sendfile()** — essential for static file performance.

### From Tokio/Hyper:
- **Bytes crate** — reference-counted zero-copy slicing is the gold standard for buffer management. TML should implement this.
- **Tower Service trait** — composable middleware with backpressure. TML's behavior system could support this pattern.
- **SIMD parsing** — httparse's SIMD acceleration is a significant advantage.
- **AFD polling on Windows** — alternative to raw IOCP that provides readiness model.

### From Node.js:
- **Stream backpressure** — critical for handling slow clients. TML has no backpressure mechanism.
- **Fastify's schema validation** — pre-compiled validation and serialization eliminates runtime overhead.
- **Cluster model** — simple multi-process scaling. TML could add process-level scaling alongside thread-level.

### From Go:
- **Transparent async I/O** — blocking-style code with async runtime is the ideal developer experience.
- **sync.Pool** — per-CPU buffer pools with GC awareness. TML should implement per-worker pools.
- **Graceful shutdown** — `Server.Shutdown(ctx)` is the standard pattern. TML needs this.
- **Context propagation** — request-scoped cancellation and timeouts are essential for production.

---

## Appendix: File Inventory

Individual analysis reports:
- [nginx-architecture.md](nginx-architecture.md)
- [tokio-hyper-architecture.md](tokio-hyper-architecture.md)
- [nodejs-http-architecture.md](nodejs-http-architecture.md)
- [go-net-http-architecture.md](go-net-http-architecture.md)
