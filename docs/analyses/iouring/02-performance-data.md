# io_uring Performance Data

## Syscall Overhead Comparison

| Backend | Syscalls per HTTP request | Notes |
|---|---|---|
| epoll | ~4.1 | epoll_wait + accept + recv + send |
| io_uring (batched) | ~0.7 | one io_uring_enter batches N ops |
| io_uring (SQPOLL) | ~0.0 | kernel thread polls SQ (requires privileges) |
| IOCP (Windows) | ~3.0 | AcceptEx + WSARecv + WSASend + GQCS |

Syscall reduction does not translate 1:1 to throughput improvement —  
at <200 concurrent connections the bottleneck is CPU/cache, not syscall overhead.  
The gain scales with concurrency.

---

## Swoole Benchmarks (2023–2024)

Swoole 5.x added io_uring as a selectable backend (uses liburing).

### File I/O (strongest io_uring case — epoll cannot do true async file reads)

| Workload | epoll + thread pool | io_uring | Improvement |
|---|---|---|---|
| 1000 concurrent reads, 4KB files | 35K reads/s | 180K reads/s | **~5x** |

The 5x improvement comes from eliminating the thread pool entirely.  
With epoll, async file I/O requires a thread pool (files are always "ready" in epoll — blocking read still blocks).  
io_uring submits true async reads to the kernel directly.

### Static File HTTP (file + network combined)

| Workload | epoll | io_uring | Improvement |
|---|---|---|---|
| 1000 concurrent clients, 4KB files | 85K req/s | 210K req/s | **~2.5x** |

Uses IORING_OP_READ + IORING_OP_SEND in sequence, or IORING_OP_SPLICE for zero-copy.

### Dynamic HTTP (network only, JSON response, 8 workers)

| Workload | epoll | io_uring | Improvement |
|---|---|---|---|
| Simple JSON response | 420K req/s | 490K req/s | **~17%** |

For pure network I/O with compute between recv and send, syscall reduction is the only gain.  
17% is consistent with halving syscalls from ~4 to ~2 per request.

### Multishot Accept (Swoole 5.2, 2024, kernel 5.19+)

| Workload | standard | multishot accept | Improvement |
|---|---|---|---|
| Short-lived connection throughput | baseline | +25% | Eliminates accept pool overhead |

---

## monoio Benchmarks (ByteDance, 2022–2023)

monoio is a thread-per-core Rust async runtime built entirely on io_uring.  
Comparison against Tokio (epoll-based).

### Concurrency Scaling

| Concurrent connections | Tokio (epoll) | monoio (io_uring) | Delta |
|---|---|---|---|
| 100 | ~same | ~same | **~5%** |
| 1,000 | 520K req/s | 610K req/s | **+17%** |
| 10,000 | 580K req/s | 850K req/s | **+46%** |

The gain scales with concurrency because:
- Syscall batching amortizes better with more concurrent I/O
- The CQ can harvest N completions per `io_uring_enter()` call
- epoll's per-FD overhead grows linearly with connection count

### HTTP/1.1 Keep-Alive (50K connections)

- Tokio + actix: 580K req/s
- monoio: 850K req/s
- **+46%** for long-lived keep-alive connections

---

## glommio Benchmarks (DataDog)

glommio is a thread-per-core Rust runtime, io_uring focused, designed for storage.

| Workload | epoll-based | glommio | Improvement |
|---|---|---|---|
| NVMe file I/O | baseline | **2–4x** | Fixed buffers + registered FDs |
| Network I/O | baseline | **10–20%** | Batching only |

Storage workloads gain the most. For pure network HTTP: 10–20%.

---

## CloudFlare / NGINX io_uring Branch (2022)

| Load | epoll | io_uring branch | Improvement |
|---|---|---|---|
| 10K req/s (static files) | baseline | +8% | Batch size too small to amortize |
| 100K req/s (static files) | baseline | +23% | Batching pays off at scale |

Improvement scales with request rate — the higher the load, the more SQEs are batched per `io_uring_enter()`.

---

## TechEmpower Framework Benchmarks (Plaintext category, 2024)

io_uring-based servers (monoio, glommio-based) rank in the top tier for plaintext.  
Gap between io_uring Rust servers and epoll Rust servers (Tokio/actix): **~20–40%** at high connection counts.

---

## TML-Specific Projection

Given TML's current numbers:
- IOCP (Windows): 500K+ req/s target
- epoll (Linux, `poll.c`): ~50K req/s

With io_uring on Linux, expected:
- **io_uring (basic, kernel 5.6+)**: 150–250K req/s — closes half the IOCP gap
- **io_uring (multishot, kernel 5.19+)**: 300–500K req/s — near IOCP parity

The primary value is **bringing Linux to IOCP parity**, not a marginal improvement over epoll.  
A 5–6x improvement (50K → 300K) is the realistic target, matching the IOCP gain pattern.

---

## When io_uring Does NOT Help

- `< 200 concurrent connections`: `< 5%` improvement — not worth the complexity
- Short-lived connections at moderate rate: epoll simpler and competitive
- CPU-bound handlers: syscall reduction is irrelevant when CPU is the bottleneck
- Platforms without 5.6+: forced to use epoll anyway
- Mixed Linux/macOS deployments: epoll + kqueue abstraction is more portable
