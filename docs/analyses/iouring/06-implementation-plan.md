# io_uring Implementation Plan for TML

## Prerequisites (Do These First)

Before implementing io_uring, complete the existing HTTP performance work:

1. **Fix pipelining bug** (`dispatch_single` null-termination) — `00-strategic-plan.md` item #1  
   This is a correctness bug that affects all backends. io_uring does not fix it.

2. **SIMD HTTP parser** — cross-platform, benefits all three backends (epoll, IOCP, io_uring)

3. **Vectored I/O with writev** — reduces syscalls on the epoll path, buy-in for all platforms

io_uring is a Linux-specific optimization that should not be the first priority.  
Fix correctness, then add cross-platform performance, then add Linux-specific acceleration.

---

## Priority

**P3 — after pipelining and SIMD parser**

Expected overall impact: bring Linux HTTP throughput from ~50K req/s to ~300–500K req/s  
(parity with IOCP on Windows). But the pipelining fix alone takes epoll from 6.5K to 50K+.

---

## Architecture Overview

```
app.tml
  ├── app_listen()           → thread pool (cross-platform, current)
  ├── app_listen_evloop()    → epoll/kqueue event loop (cross-platform, current)
  ├── app_listen_iocp()      → IOCP (Windows only, current)
  └── app_listen_iouring()   → io_uring (Linux only, NEW)
        └── iouring_worker.tml
              └── compiler/runtime/net/iouring.c
```

Auto-detection path (optional):
```
app_listen_auto()
  ├── Linux + kernel 5.19+ + io_uring available → app_listen_iouring()
  ├── Windows → app_listen_iocp()
  └── other → app_listen_evloop()
```

---

## Phase 1 — C Runtime Layer (`iouring.c`)

**File**: `compiler/runtime/net/iouring.c`  
**Dependency**: liburing 2.11+ (or raw syscalls — see note below)  
**Platform guard**: `#ifdef __linux__`

### liburing vs raw syscalls

**Option A — liburing dependency**:
- Simpler code, well-documented, no manual ring management
- Adds a build dependency
- Use `pkg-config liburing` to detect; fall back to epoll if not found
- Minimum: liburing 2.11

**Option B — raw io_uring syscalls**:
- Zero external dependencies (like how poll.c uses raw epoll syscalls)
- ~300 lines of ring management code
- Harder to maintain; must handle memory barriers manually
- Preferred for the TML philosophy of minimal dependencies

Recommendation: **Option B with the kernel API directly**, matching how `poll.c` uses raw epoll.  
Use the kernel `io_uring.h` header (available in `linux/io_uring.h`).

### API to export (mirrors iocp.c)

```c
// Context management
void* tml_iouring_create(int port, int backlog, int ring_size);
void  tml_iouring_destroy(void* ctx);

// Event loop
// Returns: { event_type, conn_index, bytes, error }
IouringEvent tml_iouring_wait(void* ctx, int timeout_ms);

// I/O operations (queue SQEs — do NOT block)
int tml_iouring_recv(void* ctx, int conn, char* buf, int len);
int tml_iouring_send(void* ctx, int conn, const char* data, int len);
void tml_iouring_close(void* ctx, int conn);

// Runtime detection
int tml_iouring_available(void); // returns 1 if io_uring works, 0 if blocked/unavailable
```

### Internal structure

```c
typedef struct {
    int ring_fd;
    struct io_uring_sq sq;
    struct io_uring_cq cq;
    void* sq_ring_ptr;
    void* cq_ring_ptr;
    struct io_uring_sqe* sqes;

    int listen_fd;
    int conn_fds[MAX_CONNS];
    int conn_state[MAX_CONNS]; // ACCEPTING, RECVING, SENDING, CLOSED

    // For multishot accept (kernel 5.19+)
    int multishot_accept;      // 1 if available, 0 if not

    // Token encoding: (conn_index << 8) | op_type
} IouringCtx;
```

### Runtime kernel version detection

```c
int tml_iouring_available(void) {
    struct io_uring_params params = {0};
    int fd = syscall(SYS_io_uring_setup, 16, &params);
    if (fd < 0) return 0;
    close(fd);
    return 1;
}

int tml_iouring_has_multishot(void) {
    // probe IORING_OP_ACCEPT with IORING_ACCEPT_MULTISHOT
    // available in kernel 5.19+
    struct io_uring_probe* probe = io_uring_get_probe();
    int has = io_uring_opcode_supported(probe, IORING_OP_ACCEPT);
    // additionally check kernel version >= 5.19
    free(probe);
    return has;
}
```

---

## Phase 2 — TML Worker (`iouring_worker.tml`)

**File**: `lib/std/src/http/server/iouring_worker.tml`  
**Pattern**: Mirror of `iocp_worker.tml` with iouring FFI

### Key differences from iocp_worker.tml

1. **Accept**: On kernels 5.19+, use multishot — no pool management needed  
   On older kernels: re-issue accept SQE after each completion (pool size = 1)

2. **Token encoding**: `user_data = (conn_index as I64 << 8) or op_type`  
   Same as IOCP token but without the `OVERLAPPED` pointer indirection

3. **Buffer ring** (optimization, not baseline):  
   If kernel supports provided buffers, register a buffer ring at startup.  
   Each recv SQE sets `IOSQE_BUFFER_SELECT` — kernel picks a buffer from the ring.

4. **Fallback**: If `tml_iouring_available()` returns 0, the caller falls back to evloop mode.

### State machine (identical to IOCP)

```tml
func iouring_worker_run(ctx: RawPtr, shared: SharedState) {
    loop {
        let event = tml_iouring_wait(ctx, KEEPALIVE_TIMEOUT_MS)
        when event.kind {
            EVENT_ACCEPT => {
                let conn = event.conn
                conn_slots[conn].reset()
                tml_iouring_recv(ctx, conn, recv_bufs[conn].ptr, RECV_BUF_SIZE)
            }
            EVENT_RECV => {
                let conn = event.conn
                if event.bytes <= 0 {
                    tml_iouring_close(ctx, conn)
                } else {
                    let resp = dispatch(shared, conn_slots[conn], event.bytes)
                    tml_iouring_send(ctx, conn, resp.ptr, resp.len)
                }
            }
            EVENT_SEND => {
                let conn = event.conn
                if conn_slots[conn].keepalive {
                    tml_iouring_recv(ctx, conn, recv_bufs[conn].ptr, RECV_BUF_SIZE)
                } else {
                    tml_iouring_close(ctx, conn)
                }
            }
            EVENT_TIMEOUT => {
                // close idle connections
            }
        }
    }
}
```

---

## Phase 3 — Integration (`app.tml`)

Add `app_listen_iouring()` function:

```tml
@extern("c") func tml_iouring_available() -> I32

func app_listen_iouring(app: App, port: I32, workers: I32) {
    let n = if workers <= 0 { cpu_count() } else { workers }
    loop i in 0..n {
        thread_spawn(do() {
            let ctx = tml_iouring_create(port, 1024, 4096)
            iouring_worker_run(ctx, app.shared)
        })
    }
}

// Optional: auto-detect best backend
func app_listen_auto(app: App, port: I32, workers: I32) {
    when platform() {
        "windows" => app_listen_iocp(app, port, workers)
        "linux" => {
            if tml_iouring_available() == 1 {
                app_listen_iouring(app, port, workers)
            } else {
                app_listen_evloop(app, port, workers)
            }
        }
        _ => app_listen_evloop(app, port, workers)
    }
}
```

---

## Phase 4 — Tests

Add to existing HTTP test suite:

1. `app_listen_iouring` starts and accepts connections (skip if `tml_iouring_available() == 0`)
2. Single request round-trip (GET → 200 OK)
3. Keep-alive (multiple requests on one connection)
4. High concurrency (1000 connections, 100K requests)
5. Comparison test: same workload via iouring vs evloop, assert iouring >= evloop throughput

All tests guarded with `#if linux` + `tml_iouring_available()` skip.

---

## Estimated Effort

| Phase | File | Complexity | Lines |
|---|---|---|---|
| 1 | `iouring.c` | Medium | ~400 |
| 2 | `iouring_worker.tml` | Low (mirror IOCP) | ~200 |
| 3 | `app.tml` additions | Low | ~30 |
| 4 | Tests | Medium | ~150 |

Total: ~780 lines of new code. The IOCP implementation (`iocp.c` = 800 lines) serves as the template.

---

## Success Criteria

- [ ] `tml_iouring_available()` correctly returns 0 in Docker (default seccomp) → graceful fallback
- [ ] `tml_iouring_available()` returns 1 in Docker with custom seccomp or bare Linux
- [ ] Single-request round-trip passes on kernel 5.6+
- [ ] Multishot accept activates on kernel 5.19+
- [ ] Throughput ≥ 300K req/s at 10K concurrent connections on bare Linux
- [ ] No regression on the epoll path when io_uring unavailable
- [ ] All existing HTTP tests pass (both backends)
