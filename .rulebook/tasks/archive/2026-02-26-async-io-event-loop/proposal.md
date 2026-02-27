# Proposal: Async I/O Event Loop

## Why

The current `AsyncTcpStream`/`AsyncUdpSocket` use busy-wait loops (spin until ready) which burn CPU and can't handle concurrent connections. A real event loop with OS-level I/O multiplexing is required before the HTTP server can handle load. This follows tokio (Rust) and Node.js (libuv) architecture patterns.

## What Changes

New `std::async` module providing:
- **Platform Poller** — C FFI layer (~200 lines) wrapping epoll (Linux) + WSAPoll (Windows)
- **Poller TML wrapper** — Token-based readiness dispatch (like mio)
- **Timer Wheel** — O(1) hashed timer wheel (like tokio), pure TML
- **Event Loop** — Single-threaded callback-based event loop (like Node.js/libuv)
- **Async TCP** — TcpServer (accept on event loop) + TcpClient (connect/read/write on event loop)
- **Async UDP** — UdpHandle with event-loop-driven send/recv

Architecture: 3-layer design (Platform FFI → Poller+Timers → EventLoop+API). Single-threaded MVP that becomes the building block for future multi-reactor (Phase 2: N threads with SO_REUSEPORT, like Envoy/Pingora/NGINX).

## Impact
- Affected specs: none (new module)
- Affected code: new files in `compiler/runtime/net/poll.c`, `lib/std/src/async/`, `lib/std/tests/async/`
- Breaking change: NO
- User benefit: Efficient async TCP/UDP I/O for servers handling many concurrent connections
