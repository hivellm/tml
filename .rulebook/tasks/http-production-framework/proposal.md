# Proposal: HTTP Framework — Production-Ready (Tokio-Level)

## Status: IN_PROGRESS

## Why

TML's HTTP framework (`std::http::app`) has a working Express-like API with thread pool, middleware, and timeouts. But it's not production-ready and can't compete with Go's net/http (~150K req/s) or Tokio/Axum (~400K req/s). This task tracks the path from current state (56K req/s) to production quality.

## Current State (March 2026)

### Already Implemented
- `App` type with Express-like API (`app.get`, `app.post`, `app.listen`)
- Thread pool (N workers = CPU cores) with mutex+condvar ring buffer
- Middleware pipeline (`app.use_middleware`)
- Read/write/idle timeouts via `SO_RCVTIMEO/SO_SNDTIMEO`
- Graceful shutdown flag
- Keep-alive connections
- `AppContext` with `.json()`, `.text()`, `.html()`, `.redirect()`
- Radix-tree Router (in `router.tml`) with `:param` and `*wildcard`
- 93 passing HTTP tests, 56K req/s benchmark

### Missing for Production
1. Router not wired to dispatch (still using O(n) linear scan)
2. No async I/O (blocking sockets, thread pool is CPU-bound)
3. No zero-copy HTTP parsing (allocates per method/path/body)
4. No buffer-based responses (string concat per response)
5. No connection draining on shutdown
6. No request size limits (DoS vector)
7. No error recovery (panicking handler kills worker thread)
8. No HTTP/1.1 pipelining support
9. No chunked request body support

## Performance Targets

| Milestone | Target | Key Changes |
|-----------|--------|-------------|
| Phase 1 | 80K req/s | Wire Router, buffer responses |
| Phase 2 | 150K req/s | Zero-copy parsing, arena allocator |
| Phase 3 | 400K req/s | Async I/O (IOCP/epoll) |

## Comparison

| Framework | Req/s | I/O Model |
|-----------|-------|-----------|
| TML (current) | 56K | Thread pool, blocking |
| Node.js/Express | 30K | Event loop, single thread |
| Go net/http | 150K | Goroutines, epoll |
| Tokio/Axum | 400K | Async, IOCP/epoll |
| Techempower #1 | 7M | io_uring, zero-copy |
