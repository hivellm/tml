# io_uring Analysis — TML HTTP Server

**Date**: 2026-04-10  
**Trigger**: Swoole 5.x benchmark results showing significant gains with liburing  
**Scope**: Feasibility, architecture mapping, performance data, risks, liburing 2.14 state

---

## Files in This Analysis

| File | Contents |
|---|---|
| `00-overview.md` | Summary, verdict, and file index |
| `01-iouring-mechanics.md` | io_uring internals, kernel versions, opcodes |
| `02-performance-data.md` | Benchmarks: Swoole, monoio, epoll comparison |
| `03-iocp-mapping.md` | IOCP → io_uring architectural mapping for TML |
| `04-risks.md` | Security CVEs, Docker/K8s restrictions, kernel fragmentation |
| `05-liburing-changelog.md` | liburing 2.9–2.14 changes and what they fix |
| `06-implementation-plan.md` | Proposed TML implementation strategy |

---

## Executive Summary

io_uring is the Linux kernel's completion-based async I/O interface (introduced in Linux 5.1).  
It reduces syscall overhead by submitting I/O operations in batches via shared-memory ring buffers,  
replacing epoll's readiness model with a true async completion model — architecturally equivalent  
to Windows IOCP.

### TML Current State

| Platform | Backend | Model | Target perf |
|---|---|---|---|
| Windows | IOCP (`iocp.c` + `iocp_worker.tml`) | Completion-based | 500K+ req/s |
| Linux | epoll (`poll.c` + `worker.tml`) | Readiness-based | ~50K req/s |
| macOS | kqueue (`poll.c`) | Readiness-based | ~50K req/s |

The 10x gap between IOCP and epoll on TML is not a code quality gap —  
it is an architectural gap. io_uring closes it on Linux.

### Verdict

**io_uring is a viable optional backend for TML on Linux**, with these conditions:

1. Must fall back to epoll gracefully when io_uring is unavailable (containers, old kernels)
2. Minimum target: Linux 5.19+ for multishot accept/recv (the primary performance driver)
3. Implementation follows `iocp_worker.tml` pattern — the architectures are 1:1
4. liburing 2.11+ required for Alpine/musl compatibility (now fixed)
5. Priority: after pipelining fix + SIMD parser (cross-platform gains first)

### Expected Gain

- Dynamic HTTP (JSON responses, 10K concurrent): **~40–50% over epoll**
- Static file serving (file+network mixed): **~2.5x over epoll**
- Low concurrency (<200 conns): **<5% difference** — not worth it at that scale

The primary win is **parity with IOCP on Linux**, not marginal improvement over epoll.
