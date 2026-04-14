# 15 — Networking (std::net, std::net::tcp, std::net::async_tcp)

## TML Network Benchmarks Available

| Benchmark File | Status |
|---------------|--------|
| `tcp_bench.tml` | Not run (requires network stack) |
| `tcp_async_bench.tml` | Not run (requires async runtime) |
| `tcp_sync_async_bench.tml` | Not run |
| `udp_sync_async_bench.tml` | Not run |
| `large_scale_bench.tml` | BLOCKED — N002 link failure |

## Large-Scale Benchmark — N002 Error

```
error: failed to open object crypto_DTMLHASO.obj: FileNotFound
lld-link: error: crypto.c: unknown file type
```

The large-scale socket benchmark (`100,000 socket binds`) fails because it imports `std::net::tcp` which transitively depends on `std::net::tls` → `crypto.c` → OpenSSL. The C runtime crypto objects haven't been pre-compiled.

**Root cause**: The TML build pipeline expects pre-compiled `.obj` files for C runtime modules (`crypto/*.c`, `net/tls.c`), but these are absent in the current debug build. The linker receives `.c` source files instead of `.obj` files.

## TML Network Architecture (from source analysis)

### std::net Module Hierarchy

```
std::net
├── mod.tml          — Ipv4Addr, Ipv6Addr, SocketAddr
├── tcp.tml          — TcpListener, TcpStream (sync, blocking)
├── async_tcp.tml    — AsyncTcpListener, AsyncTcpStream (async, non-blocking)
├── udp.tml          — UdpSocket
├── dns.tml          — DNS resolution
├── url.tml          — URL parsing
├── mime.tml         — MIME type handling
├── parser.tml       — Protocol parsing
├── sys.tml          — Low-level socket operations (FFI to OS)
└── error.tml        — Network error types
```

### Expected Performance Characteristics

| Operation | Expected TML (µs) | Rust std (µs) | Notes |
|-----------|-------------------|---------------|-------|
| Socket bind | 5-20 | 3-10 | OS syscall dominant |
| TCP connect | 50-200 | 50-200 | Network latency dominant |
| TCP send (1KB) | 1-5 | 1-5 | Kernel buffer copy |
| TCP recv (1KB) | 1-5 | 1-5 | Kernel buffer copy |
| DNS resolve | 1,000-50,000 | 1,000-50,000 | Network latency dominant |

For network I/O, the OS kernel and network latency dominate. Language overhead is <1% of total time. TML and Rust should be effectively identical for network benchmarks.

### Async Runtime Comparison

| Feature | Rust (tokio) | TML (std::runtime) |
|---------|-------------|-------------------|
| Executor model | Multi-threaded work-stealing | Single-threaded cooperative |
| I/O model | epoll/kqueue/IOCP | IOCP (Windows) |
| Task overhead | ~50-100 bytes | Unknown (not measured) |
| Context switch | ~100 ns | Unknown (not measured) |
| Max concurrent tasks | Millions | Unknown |

## What Needs to Be Done

| Priority | Action | Blocker |
|----------|--------|---------|
| P0 | Pre-compile C runtime crypto objects | N002 fix |
| P1 | Run tcp_bench.tml (sync socket ops) | Depends on N002 |
| P1 | Run tcp_async_bench.tml (async I/O) | Depends on N002 |
| P2 | Create Rust TCP/UDP equivalents | After TML data |
| P2 | Compare async runtime overhead | After TML data |
