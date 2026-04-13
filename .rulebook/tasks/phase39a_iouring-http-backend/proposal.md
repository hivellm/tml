# Proposal: phase28a_iouring-http-backend

## Why

The TML HTTP server on Linux uses an epoll readiness-based backend (`poll.c` + `worker.tml`)
that achieves ~50K req/s. On Windows, the IOCP completion-based backend targets 500K+ req/s —
a 10x gap that is architectural, not implementation quality.

io_uring (Linux 5.1+) is a completion-based async I/O interface that is architecturally
equivalent to IOCP. Benchmarks show:
- Dynamic HTTP (10K concurrent): +46% over epoll (monoio/ByteDance)
- Static file serving: +2.5x over epoll (Swoole 5.x)
- At scale: brings Linux to IOCP parity (~300–500K req/s target)

The TML IOCP implementation (`iocp_worker.tml`) already follows the exact pattern
that an io_uring worker requires — the port is ~mechanical. liburing 2.11+ (June 2025)
fixed Alpine/musl compatibility, making Docker deployments viable without custom images.

Source: `docs/analyses/iouring/`

## What Changes

1. **`compiler/runtime/net/iouring.c`** (new, ~400 lines, Linux only)
   - Raw io_uring syscalls (no liburing dependency, matching `poll.c` philosophy)
   - Ring setup, SQE submission, CQE harvesting
   - Runtime probe: `tml_iouring_available()` — returns 0 if seccomp blocks or kernel < 5.6
   - Multishot accept detection for kernel 5.19+
   - Exports same API shape as `iocp.c`

2. **`lib/std/src/http/server/iouring_worker.tml`** (new, ~200 lines, Linux only)
   - Mirror of `iocp_worker.tml` with io_uring FFI
   - Identical connection state machine: ACCEPT → RECV → DISPATCH → SEND → keep-alive/close
   - Token encoding: `user_data = (conn_index << 8) | op_type` (same as IOCP token)
   - Multishot accept path (no pool management) when kernel 5.19+
   - Standard single-shot accept fallback for kernel 5.6–5.18

3. **`lib/std/src/http/app/app.tml`** (modified)
   - Add `app_listen_iouring(app, port, workers)` function
   - Add `app_listen_auto(app, port, workers)` — probes backend at runtime:
     Windows → IOCP, Linux + io_uring available → io_uring, else → evloop

## Impact

- Affected specs: `docs/analyses/iouring/06-implementation-plan.md`
- Affected code: `compiler/runtime/net/`, `lib/std/src/http/server/`, `lib/std/src/http/app/`
- Breaking change: NO — additive only; existing `app_listen`, `app_listen_evloop`, `app_listen_iocp` unchanged
- User benefit: Linux HTTP servers reach IOCP-class throughput without code changes (via `app_listen_auto`)

## Prerequisites

Complete before this task:
- `phase26a_http-performance` — pipelining fix and SIMD parser (cross-platform gains first)
