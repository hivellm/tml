# Tasks: Thread Completeness

**Status**: Complete — 11/13 done, 2 blocked by codegen
**Priority**: MEDIUM
**Phase**: 7 — Rust Parity

## Phase 1: Fix Stubs

- [x] 1.1 `park()` — real implementation via Windows Event/futex (C runtime FFI)
- [x] 1.2 `unpark()` — wake parked thread via ParkState pointer
- [x] 1.3 `park_timeout(millis: I64)` — timed park with WaitForSingleObject timeout
- [x] 1.4 `Builder::spawn` — generic version blocked by codegen; added `spawn_fn()` and `spawn_i64()` specialized spawn functions that work
- [x] 1.5 `sleep(duration: Duration)` — Duration-based sleep wrapper added
- [x] 1.6 Tests: park/unpark (thread_park_unpark.test.tml), park_timeout (thread_park_timeout_only.test.tml), pre-unpark (thread_park_preunpark.test.tml), spawn (thread_builder_spawn_real.test.tml, thread_spawn_compute_real.test.tml), sleep (thread_sleep_duration.test.tml)

## Phase 2: Thread-Local Storage

- [x] 2.1 TLS C runtime FFI — already exists (LocalKey uses pure TML atomics + alloc)
- [x] 2.2 `ThreadLocal[T]` / `LocalKey[T]` type — already implemented in `lib/std/src/thread/local.tml`
- [x] 2.3 `LocalKey::new(init: func() -> T)` — already implemented
- [x] 2.4 `LocalKey::access/access_mut/try_access` — already implemented
- [x] 2.5 Tests — already exist: thread_local.test.tml, thread_local_mut.test.tml, thread_local_ops.test.tml, etc.

## Phase 3: Misc

- [x] 3.1 `panicking() -> Bool` — implemented via `tml_thread_panicking()` C runtime export + TML wrapper
- [x] 3.2 `detach()` on UnitJoinHandle/I64JoinHandle — detach without joining (via raw_thread_detach)
- [x] 3.3 Tests — thread_panicking.test.tml

## Notes

- `Builder::spawn[T]` generic version remains a stub returning `Err(SpawnError::Unsupported)` — blocked by codegen inability to get function pointers from generic functions and closure capture issues with generics
- Working alternative: `spawn_fn(func())` and `spawn_i64(func() -> I64)` provide real thread spawning
- Park/unpark required new C runtime code in `compiler/runtime/concurrency/sync.c` (Windows Events + TLS for per-thread ParkState)
- `panicking()` required adding `tml_thread_panicking()` export to `compiler/runtime/core/essential.c`
