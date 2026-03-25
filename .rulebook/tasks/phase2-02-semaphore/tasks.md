# Tasks: Semaphore — Bounded Concurrency Control

**Status**: Proposed
**Priority**: HIGH
**Phase**: 2 — Stdlib Completeness

## Motivation

Semaphores limit concurrent access to a resource. Essential for connection pools (max 50 DB connections), rate limiting (max 100 concurrent requests), and bounded parallelism (max N worker threads). Rust has `tokio::Semaphore`, Go can emulate with buffered channels. TML has Mutex (binary) and Barrier but no counting semaphore.

## Phase 1: Counting Semaphore (`lib/std/src/sync/semaphore.tml`)

- [ ] 1.1 Implement `Semaphore` struct — counting semaphore backed by Mutex + CondVar
- [ ] 1.2 `Semaphore::new(permits: I64) -> Semaphore` — create with N permits
- [ ] 1.3 `acquire(mut this)` — block until permit available, then decrement. O(1) amortized
- [ ] 1.4 `try_acquire(mut this) -> Bool` — non-blocking acquire, returns false if no permits
- [ ] 1.5 `release(mut this)` — increment permit count, wake one waiter
- [ ] 1.6 `available_permits(this) -> I64` — current count
- [ ] 1.7 Write tests: basic acquire/release, try_acquire when full, concurrent access

## Phase 2: RAII Guard & Advanced

- [ ] 2.1 `SemaphoreGuard` — RAII type that auto-releases permit on drop
- [ ] 2.2 `acquire_guard(mut this) -> SemaphoreGuard` — acquire and return guard
- [ ] 2.3 `acquire_timeout(mut this, timeout_ms: I64) -> Bool` — acquire with timeout
- [ ] 2.4 `close(mut this)` — prevent new acquires (for graceful shutdown)
- [ ] 2.5 `is_closed(this) -> Bool`
- [ ] 2.6 Write tests: guard auto-release, timeout, close semantics
- [ ] 2.7 Update `sync/mod.tml` to export Semaphore
- [ ] 2.8 Run full sync test suite
