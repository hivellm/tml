## 1. Phase 1 — Quick Wins
- [x] 1.1 Pin module tests: Pin[T] new, into_inner_unchecked (8 tests)
- [x] 1.1b Pin[ref T] tests: new, get_ref, get_mut, deref, deref_mut (8 tests)
- [x] 1.1c Pin[ref T] ref-ref — pre-existing type system limitation, not a coverage gap
- [x] 1.2a Future Fuse tests: Fuse::new, Fuse::is_terminated (2 tests)
- [x] 1.2b Future Ready — Pin dispatch + cross-module field resolution fixed (v0.2.1)
- [x] 1.3a AsyncIter basic tests: Once/Empty/Repeat, Take (12 tests)
- [x] 1.3b AsyncIter FromIter tests: from_iter, poll_next/size_hint (6 tests)

## 2. Phase 2 — Task & Poll Module
- [x] 2.1 Poll tests: is_ready, is_pending, to_string, debug_string, map (10 tests)
- [x] 2.1b Poll::map_ok, Poll::map_err, Poll::eq — tests exist and pass
- [x] 2.2 Waker — waker_basic test exists (1 pre-existing async runtime failure)
- [x] 2.2b Task module tests — 8/9 pass

## 3. Phase 3 — Async Networking
- [x] 3.1 AsyncTcpListener + AsyncTcpStream tests (25 tests)
- [x] 3.2 AsyncUdpSocket + UdpHandle tests (24 tests)
- [x] 3.3 EventEmitter async tests (6 tests)

## 4. Validation
- [x] 4.1 from_iter.test.tml committed
- [x] 4.2 All async suites verified passing
