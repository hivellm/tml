# io_uring Internals

## Core Architecture

io_uring uses two shared-memory ring buffers between kernel and userspace:

```
Userspace                          Kernel
---------                          ------
Write SQEs to SQ ring   →   io_uring_enter() syscall
                         ←   Kernel processes SQEs
Read CQEs from CQ ring   ←   Kernel writes completions
```

**Submission Queue Entry (SQE)** — fields relevant to HTTP:
- `opcode` — operation (IORING_OP_ACCEPT, IORING_OP_RECV, IORING_OP_SEND, …)
- `fd` — file descriptor
- `addr` / `len` — buffer pointer and length
- `user_data` — opaque u64 echoed verbatim in the CQE (correlation ID)
- `buf_index` / `buf_group` — for registered/provided buffers (zero-copy path)
- `flags` — IOSQE_FIXED_FILE, IOSQE_IO_LINK, IOSQE_BUFFER_SELECT, …

**Completion Queue Entry (CQE)**:
- `user_data` — echoed from the SQE
- `res` — result (bytes transferred, or negative errno)
- `flags` — IORING_CQE_F_MORE (multishot has more completions pending), IORING_CQE_F_BUFFER (which provided buffer was used)

## Syscall Modes

| Mode | Syscalls/req | Requirement |
|---|---|---|
| Standard | ~0.7 (batched `io_uring_enter`) | kernel 5.1+ |
| SQPOLL | ~0.0 (kernel thread polls SQ) | CAP_SYS_NICE or root |
| epoll (comparison) | ~4.1 | any |

**SQPOLL note**: Not suitable for production HTTP servers without privileges. Standard mode is the target.

## Kernel Version Requirements

| Feature | Min Kernel | Notes |
|---|---|---|
| io_uring base | 5.1 | read/write/fsync only |
| IORING_OP_ACCEPT | 5.5 | TCP accept |
| IORING_OP_CONNECT | 5.5 | TCP connect |
| IORING_OP_RECV / SEND | **5.6** | Practical HTTP minimum |
| IORING_OP_SPLICE | 5.7 | file→socket zero-copy |
| IORING_ACCEPT_MULTISHOT | **5.19** | One SQE → unlimited accepts |
| IORING_RECV_MULTISHOT | **5.20 / 6.0** | One SQE → unlimited recvs |
| IORING_OP_SENDMSG_ZC | 6.0 | Zero-copy send |
| Fixed buffers + buffer rings | 6.2 | Full zero-copy recv path |

**Practical TML targets**:
- `5.6+`: basic io_uring backend (accept + recv + send)
- `5.19+`: multishot accept (eliminate accept pool management)
- `6.0+`: multishot recv + zero-copy send (maximum throughput)

## Supported Opcodes for HTTP

```
IORING_OP_ACCEPT         accept4(2) — new connections
IORING_OP_CONNECT        connect(2) — client connections
IORING_OP_RECV           recv(2) — read request data
IORING_OP_SEND           send(2) — write response data
IORING_OP_CLOSE          close(2) — connection teardown
IORING_OP_SHUTDOWN       shutdown(2) — half-close
IORING_OP_TIMEOUT        timer completion — keepalive timeouts
IORING_OP_CANCEL         cancel pending SQE — connection abort
IORING_OP_SPLICE         file→socket for static file serving
IORING_OP_STATX          async file metadata
IORING_OP_OPENAT         async file open (for static serving)
IORING_OP_POLL_ADD       epoll compatibility mode (readiness fallback)
```

## Multishot Operations (Critical for HTTP Servers)

**IORING_ACCEPT_MULTISHOT** (kernel 5.19+):  
One SQE issues unlimited accept completions until cancelled.  
Eliminates the need for an accept pool (TML IOCP uses 256 pending AcceptEx calls as a workaround).

**IORING_RECV_MULTISHOT** (kernel 5.20+):  
One SQE issues unlimited recv completions on a connection.  
Requires provided buffers — the kernel picks a buffer from a pre-registered pool for each recv.

## Provided Buffers (Zero-Copy Recv Path)

Instead of specifying a buffer per SQE:
1. Register a buffer ring with `io_uring_register_buf_ring()`
2. Set `IOSQE_BUFFER_SELECT` in SQE flags
3. Kernel picks an available buffer from the ring for each recv
4. CQE `flags` contains `IORING_CQE_F_BUFFER | (buf_id << 16)` to identify which buffer was used
5. Application recycles the buffer after processing

This eliminates per-recv allocation and reduces page-pinning overhead by 15–30%.

## Registered File Descriptors

Register open sockets with `io_uring_register_files()`.  
Use `IOSQE_FIXED_FILE` in SQEs — skips atomic refcount on the kernel file table.  
~20ns savings per operation, measurable at >500K req/s.

## Memory Ordering

CQ ring polling uses `smp_load_acquire` on the tail pointer (fixed in liburing 2.12 after a race condition was found in `__io_uring_peek_cqe`). Always use liburing helpers rather than raw ring access to avoid memory ordering bugs.
