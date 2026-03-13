## 1. Phase 1 — Quick Wins (no compiler changes)
- [x] 1.1 Pin module tests: Pin[T] new, into_inner_unchecked (8 tests in pin_basic.test.tml)
- [x] 1.1b Pin[ref T] tests: new, get_ref, get_mut, get_unchecked_mut, deref, deref_mut (8 tests in pin_ref_methods.test.tml)
- [ ] 1.1c Pin[ref T] ref-ref blockers: 3 placeholder tests blocked by ref-ref type codegen mismatch
- [x] 1.2a Future Fuse tests: Fuse::new, Fuse::is_terminated (2 tests in future_fuse.test.tml)
- [ ] 1.2b Future blockers: Ready::poll, Pending::poll blocked by generic unsized type + trait dispatch bugs
- [x] 1.3a AsyncIter basic tests: Once/Empty/Repeat size_hint, Take::new, Take size_hint (12 tests, commit cbb0b8db)
- [x] 1.3b AsyncIter FromIter tests: from_iter, FromIter poll_next/size_hint (6 tests in from_iter.test.tml — untracked, needs commit)

## 2. Phase 2 — Task & Poll Module
- [x] 2.1 Poll tests: is_ready, is_pending, to_string, debug_string, map (6+4 tests in ops/async_function*.test.tml)
- [ ] 2.1b Poll::map_ok, Poll::map_err, Poll::eq — no dedicated tests yet
- [ ] 2.2 Waker FFI bridge to C runtime TmlWaker (3 functions: wake, wake_by_ref, duplicate)
- [ ] 2.2b Task module tests — no test files found under lib/core/tests/task/

## 3. Phase 3 — Async Networking (loopback tests)
- [x] 3.1 AsyncTcpListener + AsyncTcpStream tests (25 tests in async_tcp_loopback.test.tml)
- [x] 3.2 AsyncUdpSocket + UdpHandle tests (24 tests in async_udp_loopback.test.tml)
- [x] 3.3 EventEmitter async tests (6 tests in emitter_async.test.tml)

## 4. Validation
- [ ] 4.1 Commit from_iter.test.tml (untracked)
- [ ] 4.2 Run full coverage and verify all async modules at 100%

## Blockers Summary
- Pin[ref T] ref-ref: type system rejects `ref ref T` from accessing ref field through ref Self
- Future Ready/Pending::poll: generic unsized alloca + trait dispatch returning void
- LazyFuture: 2 tests blocked by generic monomorphization codegen bug
