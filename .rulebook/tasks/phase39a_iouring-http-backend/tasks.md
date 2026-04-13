## 1. C Runtime Layer (iouring.c)
- [ ] 1.1 Create `compiler/runtime/net/iouring.c` with Linux platform guard (`#ifdef __linux__`)
- [ ] 1.2 Implement io_uring ring setup: `io_uring_setup()` + 3 `mmap()` calls (SQ ring, CQ ring, SQE array)
- [ ] 1.3 Implement `tml_iouring_available()` — probe syscall, return 0 on EPERM/ENOSYS
- [ ] 1.4 Implement `tml_iouring_create(port, backlog, ring_size)` — bind, listen, setup ring
- [ ] 1.5 Implement `tml_iouring_wait(ctx, timeout_ms)` — drain CQ ring, return first event
- [ ] 1.6 Implement `tml_iouring_recv(ctx, conn, buf, len)` — submit IORING_OP_RECV SQE
- [ ] 1.7 Implement `tml_iouring_send(ctx, conn, data, len)` — submit IORING_OP_SEND SQE
- [ ] 1.8 Implement `tml_iouring_close(ctx, conn)` — submit IORING_OP_CLOSE SQE
- [ ] 1.9 Implement single-shot accept (kernel 5.6+) — IORING_OP_ACCEPT, re-issue after each completion
- [ ] 1.10 Implement multishot accept (kernel 5.19+) — IORING_ACCEPT_MULTISHOT, one SQE handles all accepts
- [ ] 1.11 Add runtime multishot detection: probe kernel version or use `io_uring_probe` opcode check
- [ ] 1.12 Add `tml_iouring_destroy(ctx)` — `munmap` rings, close ring fd, free connection slots

## 2. TML Worker (iouring_worker.tml)
- [ ] 2.1 Create `lib/std/src/http/server/iouring_worker.tml` with `@extern("c")` FFI declarations
- [ ] 2.2 Implement connection slot array (`conn_slots: [ConnSlot; MAX_CONNS]`) — same as IOCP worker
- [ ] 2.3 Implement per-worker recv buffer pool (pre-allocated, reused across requests)
- [ ] 2.4 Implement `iouring_worker_run(ctx, shared)` event loop — handle ACCEPT/RECV/SEND/TIMEOUT events
- [ ] 2.5 Implement connection state machine: ACCEPT → RECV → DISPATCH → SEND → keepalive/close
- [ ] 2.6 Implement token encoding: `user_data = (conn_index as I64 << 8) or op_type`
- [ ] 2.7 Wire dispatch: call `shared.dispatch(conn_slots[conn])` and queue IORING_OP_SEND with response

## 3. App Integration (app.tml)
- [ ] 3.1 Add `@extern("c") func tml_iouring_available() -> I32` to app.tml FFI section
- [ ] 3.2 Add `app_listen_iouring(app: App, port: I32, workers: I32)` — spawn N worker threads
- [ ] 3.3 Add `app_listen_auto(app: App, port: I32, workers: I32)` — runtime backend selection

## 4. Build System
- [ ] 4.1 Add `iouring.c` to `CMakeLists.txt` runtime sources with `if(LINUX)` guard
- [ ] 4.2 Verify `scripts/build.bat` picks up the new source without changes

## 5. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 5.1 Update `docs/analyses/iouring/06-implementation-plan.md` with actual implementation notes
- [ ] 5.2 Write tests: basic request round-trip, keep-alive, high-concurrency (1K conns), fallback when unavailable
- [ ] 5.3 Run tests and confirm they pass (io_uring tests are guarded by `tml_iouring_available() == 0` check at runtime)
