# Tasks: Thread Completeness

**Status**: Proposed
**Priority**: MEDIUM
**Phase**: 7 — Rust Parity

## Phase 1: Fix Stubs

- [ ] 1.1 `park()` — real implementation via OS event/futex (currently calls yield_now)
- [ ] 1.2 `unpark()` — wake parked thread (currently no-op)
- [ ] 1.3 `park_timeout(millis: I64)` — timed park
- [ ] 1.4 `Builder::spawn` — make functional (currently returns Err)
- [ ] 1.5 `sleep(duration: Duration)` — Duration-based sleep (has sleep_ms only)
- [ ] 1.6 Tests: park/unpark, Builder.spawn, park_timeout

## Phase 2: Thread-Local Storage

- [ ] 2.1 Add `tml_tls_alloc`, `tml_tls_get`, `tml_tls_set` to C runtime
- [ ] 2.2 `ThreadLocal[T]` type in `std::thread`
- [ ] 2.3 `ThreadLocal::new(init: func() -> T)` — create TLS slot
- [ ] 2.4 `ThreadLocal::get(this) -> ref T` — access thread-local value
- [ ] 2.5 Tests: TLS across threads, initial value, per-thread isolation

## Phase 3: Misc

- [ ] 3.1 `panicking() -> Bool` — check if current thread is panicking
- [ ] 3.2 `JoinHandle::into_inner` — detach without joining
- [ ] 3.3 Tests
