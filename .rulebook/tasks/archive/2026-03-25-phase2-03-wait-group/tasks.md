# Tasks: WaitGroup — Wait for N Tasks to Complete

**Status**: COMPLETE (9/10 — threaded test deferred, needs multi-thread test harness)
**Priority**: MEDIUM
**Phase**: 2 — Stdlib Completeness

## Phase 1: Implementation — DONE

- [x] 1.1 Implement `WaitGroup` struct — atomic counter based
- [x] 1.2 `WaitGroup::new() -> WaitGroup` — count 0
- [x] 1.3 `add(mut this, delta: I64)` — atomic fetch_add
- [x] 1.4 `done(mut this)` — fetch_add(-1)
- [x] 1.5 `wait(mut this)` — spin-wait with yield until count == 0
- [x] 1.6 `count(this) -> I64` — atomic load
- [x] 1.7 Tests: 4 tests (new, add/done, wait already zero, add/done/wait)
- [ ] 1.8 Test: fan-out pattern with real threads — needs thread spawn in test
- [x] 1.9 Update `sync/mod.tml` to export WaitGroup
- [x] 1.10 Verified with sync test suite
