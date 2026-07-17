# Tasks: Semaphore — Bounded Concurrency Control

**Status**: COMPLETE (all phases done)
**Priority**: HIGH
**Phase**: 2 — Stdlib Completeness

## Phase 1: Counting Semaphore — DONE

- [x] 1.1 Implement `Semaphore` struct — atomic spin-wait based
- [x] 1.2 `Semaphore::new(permits: I64) -> Semaphore`
- [x] 1.3 `acquire(mut this)` — spin-wait with yield until permit available
- [x] 1.4 `try_acquire(mut this) -> Bool` — non-blocking, CAS-based
- [x] 1.5 `release(mut this)` — atomic fetch_add
- [x] 1.6 `available_permits(this) -> I64` and `max_permits(this) -> I64`
- [x] 1.7 Tests: 4 tests (new, acquire/release, try_acquire, zero permits)

## Phase 2: RAII Guard — DONE

- [x] 2.1 `SemaphoreGuard` struct with `Drop` impl — releases permit on scope exit
- [x] 2.2 `acquire_guard(mut this) -> SemaphoreGuard` — blocking acquire + guard
- [x] 2.3 `try_acquire_guard(mut this) -> Maybe[SemaphoreGuard]` — non-blocking
- [x] 2.4 Tests: acquire_guard (block scope drop), try_acquire_guard (success + fail)
- [x] 2.5 Total: 6 tests passing (4 original + 2 new)
