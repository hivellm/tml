# Tasks: Async I/O Event Loop (std::aio)

**Status**: Phase 3 Complete — Core infrastructure (100%), High-level APIs pending (Phase 4)

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

## Phase 4: EventLoop Integration with std::net [IN PROGRESS]

- [x] 4.1 Add `register_with_loop()` to `AsyncTcpListener` — enables event loop registration
- [x] 4.2 Add `register_with_loop()` to `AsyncTcpStream` — enables event loop registration
- [x] 4.3 Add `unregister_from_loop()` to `AsyncTcpStream` — cleanup
- [x] 4.4 Add `socket_handle()` getter for both TCP types
- [x] 4.5 Add `register_with_loop()` to `AsyncUdpSocket` — enables event loop registration
- [x] 4.6 Add `unregister_from_loop()` to `AsyncUdpSocket` — cleanup
- [x] 4.7 Add `socket_handle()` getter for UDP
- [ ] 4.8 Create integration test: TCP echo server via EventLoop
- [ ] 4.9 Create integration test: UDP ping-pong via EventLoop
- [ ] 4.10 Create integration test: 10 concurrent TCP clients + timers

## Phase 5: Async Functions (Compiler Support) [DEFERRED]

- [ ] 5.1 Add `async` keyword to lexer
- [ ] 5.2 Parse `async func` declarations
- [ ] 5.3 Type-check: async func returns Promise[T] or Task[T]
- [ ] 5.4 Codegen: transform async func into state machine
- [ ] 5.5 Add `await` expression parsing + codegen
- [ ] 5.6 Test: async func + await in TML code
- **Note**: Deferred to Phase 6. Callbacks work today; async/await is syntactic sugar for future enhancement.

## Phase 6: Stream Module Enhancements [DONE]

- [x] 6.1 Create `lib/std/src/stream/duplex.tml` (DuplexStream)
- [x] 6.2 Create `lib/std/src/stream/passthrough.tml` (PassThroughStream)
- [x] 6.3 Create `lib/std/src/stream/pipeline.tml` (PipelineStream)
- [x] 6.4 Create `lib/std/src/stream/transform.tml` (TransformStream)
- [x] 6.5 Add tests for duplex/passthrough/pipeline/transform streams
- [x] 6.6 All stream tests passing
- [x] 6.7 Updated documentation in `docs/packages/23-STREAM.md`

## Phase 7: Integration & Testing [IN PROGRESS]

- [x] 7.1 Run `test --suite=std/aio` — all 28 tests passing
- [x] 7.2 Run full test suite — no regressions
- [ ] 7.3 Create integration test: EventLoop + HTTP client (end-to-end)
- [ ] 7.4 Create integration test: EventLoop + multi-protocol (TCP + UDP + timers)
