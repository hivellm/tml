# IOCP → io_uring Architectural Mapping

## Conceptual Equivalence

Both IOCP and io_uring are **completion-based** systems.  
The application submits async I/O operations and later receives completion notifications,  
rather than waiting for readiness and then making blocking calls (epoll model).

This is the key reason the TML IOCP implementation translates directly:  
`iocp_worker.tml` already follows the exact pattern an io_uring HTTP server uses.

---

## Concept Mapping Table

| Concept | IOCP (TML, Windows) | io_uring (Linux) |
|---|---|---|
| Submit async op | `AcceptEx`, `WSARecv`, `WSASend` | Write SQE to submission ring |
| Wait for result | `GetQueuedCompletionStatus` | `io_uring_wait_cqe` / read CQ ring |
| Correlation token | `OVERLAPPED*` + completion key | `user_data` (u64 in SQE/CQE) |
| Accept (one shot) | `AcceptEx(listen, accept_sock, ...)` | SQE with `IORING_OP_ACCEPT` |
| Accept (continuous) | Pool of 256 `AcceptEx` calls | **`IORING_ACCEPT_MULTISHOT`** (1 SQE) |
| Recv data | `WSARecv(sock, buf, ...)` | SQE with `IORING_OP_RECV` |
| Send data | `WSASend(sock, buf, ...)` | SQE with `IORING_OP_SEND` |
| Buffer ownership | Pinned in kernel until completion | Pinned until CQE arrives |
| Pre-allocated ops | `IocpOperation` pool | SQE pool (ring size) |
| Connection slots | Array indexed by token (`I64`) | Array indexed by `user_data` |
| Shutdown signal | `PostQueuedCompletionStatus(iocp, 0)` | `io_uring_prep_nop()` + cancel |
| Buffer per op | `WSABUF` struct | `addr` + `len` in SQE |
| Registered buffers | N/A (IOCP pins on demand) | `IORING_REGISTER_BUFFERS` (pre-pinned) |

---

## TML File Mapping

### Existing (Windows IOCP)

```
compiler/runtime/net/iocp.c          → C layer: CreateIoCompletionPort, AcceptEx, WSARecv, WSASend
lib/std/src/http/server/iocp_worker.tml → TML layer: event loop, connection state machine
```

### Proposed (Linux io_uring)

```
compiler/runtime/net/iouring.c       → C layer: io_uring_setup, io_uring_enter, SQE/CQE helpers
lib/std/src/http/server/iouring_worker.tml → TML layer: mirrors iocp_worker.tml exactly
```

The C layer is the only net-new code. The TML worker is a near-mechanical port of the IOCP worker.

---

## Key Simplifications vs IOCP

### 1. Accept Pool Elimination

IOCP requires a pool of 256 pending `AcceptEx` calls to keep the accept pipeline full.  
Managing this pool (refilling, tracking which `OVERLAPPED*` is which) is 40+ lines in `iocp.c`.

```c
// IOCP: maintain pool of pending accepts
for (int i = 0; i < ACCEPT_POOL_SIZE; i++) {
    issue_accept(&pool[i]);
}
// On each accept completion: refill the slot
issue_accept(&pool[slot]);
```

With io_uring multishot accept (kernel 5.19+):
```c
// io_uring: one SQE handles all accepts forever
io_uring_prep_multishot_accept(sqe, listen_fd, NULL, NULL, SOCK_NONBLOCK);
sqe->user_data = TOKEN_ACCEPT;
// Never re-issue — the kernel keeps posting CQEs
```

### 2. Buffer Registration

IOCP pins pages on every `WSARecv` call. With io_uring registered buffers, pages are pinned once at startup:

```c
// Register recv buffers once at startup
struct iovec bufs[MAX_CONNS];
// fill bufs[i].iov_base and iov_len
io_uring_register_buffers(ring, bufs, MAX_CONNS);

// Each recv SQE references a registered buffer by index — no page pinning
io_uring_prep_recv(sqe, fd, NULL, 0, 0);
sqe->buf_group = RECV_BUF_GROUP;
sqe->flags |= IOSQE_BUFFER_SELECT;
```

### 3. Unified Token (user_data)

IOCP uses `OVERLAPPED*` (pointer to op struct) as the completion key.  
The token mapping in `iocp_worker.tml` does `conn_index = (token >> 8); op_type = (token & 0xFF)`.

io_uring `user_data` is already a u64 — encode both in one field directly:
```
user_data = (conn_index << 8) | op_type
```
Identical encoding, no pointer arithmetic needed.

---

## Connection State Machine (Both Paths Identical)

```
ACCEPT_PENDING
    ↓ (connection arrives)
RECV_PENDING  ←──────────────────────┐
    ↓ (recv completes, >0 bytes)     │
DISPATCH                             │
    ↓ (handler returns response)     │
SEND_PENDING                         │
    ↓ (send completes)               │
    ├── keep-alive? ─── yes ─────────┘
    └── no → CLOSE
```

This state machine is identical in `iocp_worker.tml` and the proposed `iouring_worker.tml`.  
The only difference is the syscall used to transition states.

---

## FFI Surface

### iocp.c exports (current)

```c
void* tml_iocp_create(int port, int backlog);
IoUringEvent tml_iocp_wait(void* ctx, int timeout_ms);
int tml_iocp_recv(void* ctx, int conn, char* buf, int len);
int tml_iocp_send(void* ctx, int conn, const char* data, int len);
void tml_iocp_close(void* ctx, int conn);
```

### iouring.c exports (proposed — same API shape)

```c
void* tml_iouring_create(int port, int backlog);
IoUringEvent tml_iouring_wait(void* ctx, int timeout_ms);
int tml_iouring_recv(void* ctx, int conn, char* buf, int len);
int tml_iouring_send(void* ctx, int conn, const char* data, int len);
void tml_iouring_close(void* ctx, int conn);
```

The TML-side FFI declarations are identical modulo the function name prefix.  
This means `iouring_worker.tml` can share almost all logic with `iocp_worker.tml`.
