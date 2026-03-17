# TML HTTP Server Sample

A concurrent HTTP/1.1 server with JSON support, designed for benchmarking with [autocannon](https://github.com/mcollina/autocannon).

## Benchmark Results

```
55,020 req/s | 1.18ms avg latency | 3ms p99 | 0 errors
100 concurrent connections, 10 seconds, 550K total requests
```

## Architecture

- **Thread-per-connection** with OS threads (via `tml_thread_spawn` FFI)
- **HTTP/1.1 keep-alive** — multiple requests per TCP connection
- **TCP_NODELAY** — no Nagle buffering, minimal latency
- **Direct socket I/O** — `recv` / `send` with no intermediate layers
- **Handler dispatch** — route matching via simple string comparison

## Routes

| Method | Path | Response |
|--------|------|----------|
| GET | `/` | `{"status":"ok","message":"TML HTTP Server"}` |
| GET | `/health` | `{"status":"ok"}` |
| POST | `/api/echo` | Echoes the request body as JSON |

## Build and Run

```bash
tml build samples/http-server/server.tml
build/debug/server.exe
```

## Benchmark

```bash
# Basic (100 connections, 10 seconds)
npx autocannon -c 100 -d 10 http://localhost:3000/

# High concurrency
npx autocannon -c 500 -d 30 http://localhost:3000/

# POST with JSON body
npx autocannon -c 100 -d 10 -m POST \
  -H "Content-Type: application/json" \
  -b '{"name":"test","value":42}' \
  http://localhost:3000/api/echo
```

## Current Limitations

This sample uses raw FFI because the `std::http::HttpServer` API does not yet support:

1. **Keep-alive** — `HttpServer.send_response()` always closes the connection
2. **Concurrent connections** — no built-in threading in the accept loop
3. **Closure-based handlers** — `thread::spawn` cannot take closures yet (lambda blocker in `call.cpp`)

Once these are resolved, the server will simplify to:

```tml
use std::http::{HttpServer, Request, Response}

pub func main() -> I32 {
    let server = HttpServer.new()
    server.get("/", do(req, resp) { resp.json("{\"status\":\"ok\"}") })
    server.listen(3000)
    return 0
}
```

## What's Missing vs Tokio/Hyper

| Feature | TML (current) | Tokio/Hyper |
|---------|---------------|-------------|
| I/O model | Thread-per-connection (blocking) | epoll/IOCP async (non-blocking) |
| Thread management | OS thread per connection | Thread pool + task scheduler |
| Connection limit | OS thread limit (~2K-10K) | 100K+ connections |
| HTTP parsing | Manual byte scanning | Optimized state machine |
| Memory | Allocates per-request strings | Zero-copy with bytes crate |
| Backpressure | None | Built-in flow control |

The thread-per-connection model works well up to ~1K concurrent connections. For higher concurrency, TML needs async I/O primitives (`epoll`/`IOCP` bindings).
