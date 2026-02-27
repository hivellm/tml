# Tasks: Async I/O Event Loop (std::aio)

**Status**: Phase 5 Complete — Async/Await Compiler Support (100%)

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

## Phase 5: Async Functions (Compiler Support) [DONE]

- [x] 5.1 Add `async` keyword to lexer — already exists in token.hpp
- [x] 5.2 Parse `async func` declarations — implemented in parser_decl.cpp
- [x] 5.3 Type-check: async func returns Poll[T] — FuncDecl::is_async field
- [x] 5.4 Codegen: transform async func to return Poll[T] — llvm/decl/func.cpp
- [x] 5.5 Add `await` expression parsing + codegen — parser_expr.cpp + llvm/expr/await.cpp
- [x] 5.6 Test: async func + await in TML code — 12+ tests passing

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

**Async I/O Event Loop (std::aio)** Phases 1-4 complete. Phase 5 begins async/await compiler support.

**Phase 1-5 Deliverables:**
- Layer 1: Platform Poller (epoll/WSAPoll) — C FFI in poll.c
- Layer 2: TML wrappers (Poller, TimerWheel, EventLoop) — 1,200+ lines of pure TML
- Layer 3: EventLoop integration with AsyncTcpListener, AsyncTcpStream, AsyncUdpSocket
- Layer 4: Async/Await language support — `async func`, `await expr`, `Poll[T]` type
- Test coverage: 35+ tests passing (aio suite) + 12+ async/await tests
- Documentation: docs/packages/27-AIO.md (comprehensive guide)

**Phase 5 Status:**
- ✅ `async` keyword in lexer
- ✅ Parser: `async func` declarations
- ✅ Type checker: `is_async` field in FuncDecl
- ✅ Codegen: Transform to `Poll[T]` return type
- ✅ `await expr` parsing and codegen
- ✅ Synchronous execution model (no state machine yet)

**API Ready:**
```tml
// AsyncTcpListener + AsyncTcpStream + AsyncUdpSocket now support:
pub func register_with_loop(this, el: mut ref EventLoop, interests: U32) -> Outcome[U32, Str]
pub func unregister_from_loop(this, el: mut ref EventLoop, token: U32)
pub func socket_handle(this) -> I64
```

Users can now build event-driven TCP/UDP servers with the EventLoop, handling multiple concurrent connections efficiently via OS-level I/O polling.

**Future Phases:**
- Phase 6: High-Level Async TCP (TcpServer + TcpClient with EventLoop)
- Phase 7: High-Level Async UDP (UdpHandle with EventLoop)
- Phase 8: State Machine Transformation (true async/await, not just Poll[T])
- Phase 9 onwards: Higher-level async frameworks (async iterators, async streams)
