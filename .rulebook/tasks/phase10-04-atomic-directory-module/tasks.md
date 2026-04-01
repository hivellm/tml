# Tasks: Convert atomic.tml to Directory Module

**Status**: Proposed — 0/14
**Priority**: Low
**Risk**: MEDIUM (same proven pattern as sync/mod.tml)
**Target**: atomic.tml 1,507 lines → 10 files, each <300 lines

## Phase 1: Setup + AtomicBool — 0/3

- [ ] 1.1 Create `lib/std/src/sync/atomic/` directory
- [ ] 1.2 Create `atomic/bool.tml`: AtomicBool type + all methods + `pub use std::sync::ordering::Ordering`
- [ ] 1.3 Create minimal `atomic/mod.tml`, verify `mcp__tml__check` passes

## Phase 2: Integer atomics — 0/4

- [ ] 2.1 Create `atomic/i32.tml`: AtomicI32
- [ ] 2.2 Create `atomic/i64.tml`: AtomicI64
- [ ] 2.3 Create `atomic/u32.tml`: AtomicU32
- [ ] 2.4 Create `atomic/u64.tml`: AtomicU64

## Phase 3: Size + Ptr atomics — 0/4

- [ ] 3.1 Create `atomic/usize.tml`: AtomicUsize
- [ ] 3.2 Create `atomic/isize.tml`: AtomicIsize
- [ ] 3.3 Create `atomic/ptr.tml`: AtomicPtr[T]
- [ ] 3.4 Create `atomic/hints.tml`: spin_loop_hint

## Phase 4: Finalize — 0/3

- [ ] 4.1 Update `atomic/mod.tml`: pub use all 9 submodules + re-export Ordering + all types
- [ ] 4.2 Delete `lib/std/src/sync/atomic.tml`
- [ ] 4.3 Run `mcp__tml__test suite="std/sync"` — all tests must pass
