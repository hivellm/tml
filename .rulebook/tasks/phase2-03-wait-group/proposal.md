# Proposal: WaitGroup — Wait for N Tasks to Complete

## Status: PROPOSED

## Summary

A synchronization primitive that allows one goroutine/thread to wait until N other threads have finished. `add(n)` increments the pending count before spawning work; `done()` decrements it when work completes; `wait()` blocks until the count reaches zero. This is Go's `sync.WaitGroup`, the most common "fan-out, fan-in" primitive.

## Motivation

TML's `Barrier` requires the participant count to be fixed at creation time, making it unsuitable for dynamic fan-out where the number of tasks is determined at runtime. The common pattern of "spawn N tasks, wait for all to finish" requires either a `Barrier` of known size (limiting) or manual bookkeeping with an `AtomicI64` and `CondVar` (verbose, error-prone).

`WaitGroup` is the standard solution. It is used in TML's test suite infrastructure, the HTTP server's graceful shutdown sequence, and any parallel data pipeline.

## Design

`WaitGroup` contains an `AtomicI64` counter and a `CondVar` (protected by a `Mutex`) for efficient blocking. The counter starts at 0.

`add(delta)` atomically increments the counter. It must be called before the spawned task starts executing, not after — this is a documented invariant, matching Go's requirement.

`done()` is equivalent to `add(-1)`. When the counter transitions to 0, it calls `CondVar::notify_all` to wake all callers blocked in `wait()`.

`wait()` acquires the mutex, checks if the counter is already 0 (fast path, no blocking), and otherwise calls `CondVar::wait` in a loop. The loop handles spurious wakeups.

`count()` is a non-blocking read of the current counter value, useful for monitoring but not for synchronization decisions (the value can change immediately after reading).

## What Changes

- New: `lib/std/src/sync/wait_group.tml` — WaitGroup
- Modified: `lib/std/src/sync/mod.tml` — export WaitGroup
- New: `lib/std/tests/sync/wait_group_basic.test.tml`
- New: `lib/std/tests/sync/wait_group_fanout.test.tml`

## Dependencies

- Depends on: `Mutex`, `CondVar`, `AtomicI64` from `std/sync`
- Enables: graceful shutdown in HTTP server, parallel test execution coordination
- Related: `phase2-02-semaphore` (same Mutex+CondVar pattern, can share implementation patterns)

## Risks

- Calling `add` after `wait` has already started is a race condition; TML should panic (in debug builds) if `add` is called on a WaitGroup with active waiters and a counter at 0
- The `done()` → counter underflow scenario (more `done` calls than `add`) must panic with a clear message, not silently wrap to negative
