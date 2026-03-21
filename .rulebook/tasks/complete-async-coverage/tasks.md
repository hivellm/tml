## 1. Phase 1 — Quick Wins (no compiler changes)
- [x] 1.1 Pin module tests: Pin[T] new, into_inner_unchecked (8 tests)
- [x] 1.1b Pin[ref T] tests: new, get_ref, get_mut, get_unchecked_mut, deref, deref_mut (8 tests)
- [ ] 1.1c Pin[ref T] ref-ref blockers: 3 placeholder tests blocked by ref-ref type codegen
- [x] 1.2a Future Fuse tests: Fuse::new, Fuse::is_terminated (2 tests)
- [x] 1.2b Future Ready — UNBLOCKED: Pin dispatch + cross-module field resolution fixed (v0.2.1), future_ready_value.test.tml passes
- [x] 1.3a AsyncIter basic tests: Once/Empty/Repeat size_hint, Take (12 tests)
- [x] 1.3b AsyncIter FromIter tests: from_iter, poll_next/size_hint (6 tests)

## 2. Phase 2 — Task & Poll Module
- [x] 2.1 Poll tests: is_ready, is_pending, to_string, debug_string, map (10 tests)
- [ ] 2.1b Poll::map_ok, Poll::map_err, Poll::eq — no dedicated tests yet
- [ ] 2.2 Waker FFI bridge to C runtime TmlWaker (3 functions)
- [x] 2.2b Task module tests — 8/9 pass (waker_basic pre-existing async runtime issue)

## 3. Phase 3 — Async Networking (loopback tests)
- [x] 3.1 AsyncTcpListener + AsyncTcpStream tests (25 tests)
- [x] 3.2 AsyncUdpSocket + UdpHandle tests (24 tests)
- [x] 3.3 EventEmitter async tests (6 tests)

## 4. Validation
- [ ] 4.1 Commit from_iter.test.tml (untracked)
- [ ] 4.2 Run full coverage and verify all async modules

## Blockers Summary (updated 2026-03-21)
- Pin[ref T] ref-ref: type system rejects `ref ref T` — pre-existing
- Waker FFI: needs C runtime TmlWaker bridge — separate task
- Ready::poll full path: Pin dispatch works, field resolution works, but `when this.value` inside generated trait method body needs cross-module enum pattern match in monomorphized context
