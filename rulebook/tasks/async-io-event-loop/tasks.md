# Tasks: Async I/O Event Loop (std::aio)

**Status**: Phase 5 Complete — High-Level Async UDP (100%)

## Phase 1: Platform Poller (C FFI) [DONE]

- [x] 1.1 Create `compiler/runtime/net/poll.c` (epoll + WSAPoll) — 200 lines, cross-platform
- [x] 1.2 Add poll.c to CMakeLists.txt build system
- [x] 1.3 Build compiler with new poll.c (commit f2090a5)
- [x] 1.4 Create `lib/std/src/aio/poller.tml` (TML FFI wrapper) — 162 lines
- [x] 1.5 Sandbox test: register TCP listener, poll for accept readiness

## Phase 2: Timer Wheel (pure TML) [DONE]

- [x] 2.1 Create `lib/std/src/aio/timer_wheel.tml` (2-level hashed wheel) — 439 lines
- [x] 2.2 Implement 3 core operations: schedule (O1), cancel (O1), advance (O1)
- [x] 2.3 Implement automatic cascading from L0→L1 and overflow list
- [x] 2.4 Test suite: 8 tests covering basic/cancel/growth/L1/overflow/deadline
- [x] 2.5 All timer tests passing (100%)

## Phase 3: Event Loop [DONE]

- [x] 3.1 Create `lib/std/src/aio/event_loop.tml` — 377 lines
- [x] 3.2 Integrate Poller + TimerWheel with callback dispatch
- [x] 3.3 Implement I/O source registration with dynamic growth (64→128→...)
- [x] 3.4 Implement callback registration: on_readable/on_writable/on_error
- [x] 3.5 Implement main run loop (poller.wait + timer.advance + callback fire)
- [x] 3.6 Implement poll_once for external loop integration
- [x] 3.7 Test suite: 12 tests covering lifecycle/sources/timers/callbacks
- [x] 3.8 Create `lib/std/src/aio/mod.tml` module exports
- [x] 3.9 Update `lib/std/src/mod.tml` with aio submodule
- [x] 3.10 All aio tests passing (28/28, 100%)
- [x] 3.11 Documentation: created `docs/packages/27-AIO.md` (comprehensive)
- [x] 3.12 Updated CHANGELOG.md with aio + stream enhancements
- [x] 3.13 Updated ROADMAP.md with Phase 5 progress

## Phase 4: EventLoop Integration with std::net [DONE]

- [x] 4.1 Add `register_with_loop()` to `AsyncTcpListener` — enables event loop registration
- [x] 4.2 Add `register_with_loop()` to `AsyncTcpStream` — enables event loop registration
- [x] 4.3 Add `unregister_from_loop()` to `AsyncTcpStream` — cleanup
- [x] 4.4 Add `socket_handle()` getter for both TCP types
- [x] 4.5 Add `register_with_loop()` to `AsyncUdpSocket` — enables event loop registration
- [x] 4.6 Add `unregister_from_loop()` to `AsyncUdpSocket` — cleanup
- [x] 4.7 Add `socket_handle()` getter for UDP
- [x] 4.8 Created TCP socket_handle smoke test (simple.test.tml, tcp_socket_handle.test.tml)
- [x] 4.9 Created UDP socket_handle smoke test (udp_socket_handle.test.tml)
- [x] 4.10 Fixed test suite DLL crash — was caused by SocketAddr::parse() codegen, not EventLoop
- [x] 4.11 Verified all 31 aio tests passing (6 test files: event_loop, poller, timer_wheel, simple, tcp_socket_handle, udp_socket_handle)

## Phase 5: High-Level Async UDP [DONE]

- [x] 5.1 Create `lib/std/src/aio/udp.tml` (UdpHandle type) — 150+ lines
- [x] 5.2 Implement UdpHandle::bind() — create and bind UDP socket
- [x] 5.3 Implement UdpHandle::register() — register with EventLoop
- [x] 5.4 Implement UdpHandle::set_on_message() — set message callback
- [x] 5.5 Implement UdpHandle::set_on_error() — set error callback
- [x] 5.6 Implement UdpHandle::send_to() — send datagram without blocking
- [x] 5.7 Implement UdpHandle::close() / destroy() — cleanup
- [x] 5.8 Add tests for UdpHandle creation and callbacks
- [x] 5.9 Update `lib/std/src/aio/mod.tml` to export UdpHandle
- [x] 5.10 All UDP tests passing

## Phase 6: Stream Module Enhancements [DONE]

- [x] 6.1 Create `lib/std/src/stream/duplex.tml` (DuplexStream)
- [x] 6.2 Create `lib/std/src/stream/passthrough.tml` (PassThroughStream)
- [x] 6.3 Create `lib/std/src/stream/pipeline.tml` (PipelineStream)
- [x] 6.4 Create `lib/std/src/stream/transform.tml` (TransformStream)
- [x] 6.5 Add tests for duplex/passthrough/pipeline/transform streams
- [x] 6.6 All stream tests passing
- [x] 6.7 Updated documentation in `docs/packages/23-STREAM.md`

## Phase 7: Integration & Testing [DONE]

- [x] 7.1 Run `test --suite=std/aio` — all 31 tests passing ✓
- [x] 7.2 Run full test suite — no regressions ✓
- [x] 7.3 Created smoke tests for EventLoop + TCP/UDP integration
- [x] 7.4 Fixed test suite DLL crash issue (SocketAddr::parse codegen problem)

## Summary

**Async I/O Event Loop (std::aio)** is now fully integrated with **std::net** async types, plus high-level **UdpHandle** for callback-based UDP I/O.

**Deliverables:**
- Layer 1: Platform Poller (epoll/WSAPoll) — C FFI in poll.c
- Layer 2: TML wrappers (Poller, TimerWheel, EventLoop) — 1,200+ lines of pure TML
- Layer 3: EventLoop integration with AsyncTcpListener, AsyncTcpStream, AsyncUdpSocket
- Layer 4: High-level async handles (UdpHandle) — 150+ lines of pure TML
- Test coverage: 35+ tests passing
- Documentation: docs/packages/27-AIO.md (comprehensive guide)

**API Ready:**
```tml
// AsyncTcpListener + AsyncTcpStream + AsyncUdpSocket now support:
pub func register_with_loop(this, el: mut ref EventLoop, interests: U32) -> Outcome[U32, Str]
pub func unregister_from_loop(this, el: mut ref EventLoop, token: U32)
pub func socket_handle(this) -> I64
```

Users can now build event-driven TCP/UDP servers with the EventLoop, handling multiple concurrent connections efficiently via OS-level I/O polling.

**Future Phases:**
- Phase 6: High-Level Async TCP (TcpServer + TcpClient)
- Phase 7: async/await syntax (compiler support)
- Phase 8 onwards: Higher-level async frameworks (async iterators, async streams)
