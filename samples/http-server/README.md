# TML HTTP Server Sample

A concurrent HTTP/1.1 server with JSON support, keep-alive, and threading.

## Benchmark Results

```
41,329 req/s | 1.92ms avg latency | 4ms p99 | 0 errors
100 concurrent connections, 10 seconds, 413K total requests
```

## Architecture

- **Thread-per-connection** via OS threads (`tml_thread_spawn` FFI)
- **HTTP/1.1 keep-alive** — multiple requests per TCP connection
- **TCP_NODELAY** — Nagle disabled for low-latency responses
- **Static route dispatch** — method+path string matching

## Routes

| Method | Path | Response |
|--------|------|----------|
| GET | `/` | `{"status":"ok","message":"TML HTTP Server"}` |
| GET | `/health` | `{"status":"ok"}` |
| POST | `/api/echo` | Echoes the request body |

## Build and Run

```bash
tml build samples/http-server/server.tml
build/debug/server.exe
```

## Benchmark

```bash
npx autocannon -c 100 -d 10 http://localhost:3000/
```

## API Pattern

Routes are registered with lambdas and dispatched via string matching:

```tml
// Route registration (lambdas compile and work!)
count = route_add(table, count, "GET", "/", do(ctx: HandlerContext) -> Str {
    json_response(200, "{\"status\":\"ok\"}")
})

// Dispatch is static (string comparison)
// Dynamic fn-ptr dispatch is blocked by a codegen bug (fn ptrs in locals)
```

## Known Limitations

| Feature | Status | Blocker |
|---------|--------|---------|
| Lambda route handlers | Registration works | Dispatch via stored fn ptrs fails (MIR codegen treats local fn vars as global names) |
| Event loop (epoll/WSAPoll) | Runtime exists | Not integrated with HTTP server yet |
| `std::http::HttpServer` keep-alive | Not implemented | `send_response()` always closes socket |
| Async I/O | Poller + EventLoop exist | No async/await syntax in closures |

## Comparison

| Framework | req/s | Model |
|-----------|-------|-------|
| **TML (this)** | **41K** | Thread-per-connection, blocking |
| Node.js http | ~35K | Event loop, non-blocking |
| Go net/http | ~80K | Goroutines, multiplexed |
| Rust/Hyper | ~150K | Tokio, epoll/IOCP async |

TML is competitive with Node.js. To reach Go/Rust levels, needs epoll/IOCP integration.
