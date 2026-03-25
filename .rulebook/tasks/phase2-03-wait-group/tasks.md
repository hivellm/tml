# Tasks: WaitGroup — Wait for N Tasks to Complete

**Status**: Proposed
**Priority**: MEDIUM
**Phase**: 2 — Stdlib Completeness

## Motivation

Go's `sync.WaitGroup` is the most-used concurrency primitive for "fan-out, fan-in" patterns. TML has Barrier (static count set at creation) but WaitGroup allows dynamic add/done. Essential for "spawn N tasks, wait for all" patterns.

## Phase 1: Implementation (`lib/std/src/sync/wait_group.tml`)

- [ ] 1.1 Implement `WaitGroup` struct — atomic counter + CondVar for signaling
- [ ] 1.2 `WaitGroup::new() -> WaitGroup` — create with count 0
- [ ] 1.3 `add(mut this, delta: I64)` — increment counter (call before spawning task)
- [ ] 1.4 `done(mut this)` — decrement counter (call when task finishes). Equivalent to `add(-1)`
- [ ] 1.5 `wait(mut this)` — block until counter reaches 0
- [ ] 1.6 `count(this) -> I64` — current pending count
- [ ] 1.7 Write tests: basic add/done/wait, concurrent done from multiple threads
- [ ] 1.8 Write test: fan-out pattern — spawn 10 threads, wait for all
- [ ] 1.9 Update `sync/mod.tml` to export WaitGroup
- [ ] 1.10 Run full sync test suite
