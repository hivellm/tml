# Proposal: Semaphore — Bounded Concurrency Control

## Status: PROPOSED

## Summary

A counting semaphore that limits concurrent access to a shared resource. `Semaphore::new(n)` creates a permit pool of size n; `acquire` blocks until a permit is available; `release` returns a permit and wakes one waiter. A `SemaphoreGuard` RAII type releases automatically on drop. Matches the semantics of `tokio::sync::Semaphore` (without async) and Go's buffered channel idiom.

## Motivation

TML has `Mutex[T]` (binary lock) and `Barrier` (synchronization point with fixed count) but no counting semaphore. The HTTP server's connection pool needs to cap concurrent database connections at a fixed limit. Without a semaphore, code must manually maintain an atomic counter with a condvar — which is exactly what a semaphore is, and gets reimplemented incorrectly in each call site.

Semaphores are also the standard primitive for rate limiting (max N concurrent requests) and bounded producer/consumer queues (block producers when queue is full).

## Design

`Semaphore` is a struct containing an `I64` permit count, a `Mutex` for serialized access to the count, and a `CondVar` for waking blocked waiters. This avoids spinning.

`acquire` locks the mutex, checks the count, and if zero, calls `CondVar::wait` in a loop (handling spurious wakeups). On success it decrements the count. `release` locks, increments, and calls `CondVar::notify_one`.

`SemaphoreGuard` holds a mutable reference to the `Semaphore` and calls `release` in its `Drop` implementation, preventing permit leaks. `acquire_guard` is the primary ergonomic entry point.

`acquire_timeout` uses `CondVar::wait_timeout` — available in TML's existing condvar implementation.

`close` sets a closed flag (atomic bool) and wakes all waiters with an error path, enabling graceful shutdown where no new work is accepted but in-flight work completes.

## What Changes

- New: `lib/std/src/sync/semaphore.tml` — Semaphore, SemaphoreGuard
- Modified: `lib/std/src/sync/mod.tml` — export Semaphore
- New: `lib/std/tests/sync/semaphore_basic.test.tml`
- New: `lib/std/tests/sync/semaphore_guard.test.tml`

## Dependencies

- Depends on: `Mutex`, `CondVar` from `std/sync`
- Enables: `phase4-03-package-manager` connection pool, HTTP server connection limiting
- Enables: `phase2-03-wait-group` (WaitGroup uses the same Mutex+CondVar pattern)

## Risks

- `acquire_timeout` semantics on Windows vs POSIX differ slightly in condvar timeout precision; test with actual thread sleep to verify
- `close` + in-flight `acquire` interactions must be tested: acquiring on a closed semaphore must fail fast, not block
