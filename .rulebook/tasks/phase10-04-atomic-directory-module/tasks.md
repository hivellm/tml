# Tasks: Convert atomic.tml to Directory Module

**Status**: Done — 14/14
**Priority**: Low
**Risk**: MEDIUM (same proven pattern as sync/mod.tml)
**Result**: atomic.tml 1,507 lines → 10 files in atomic/ directory (9 submodules + mod.tml)

## Phase 1: Setup + AtomicBool — 3/3

- [x] 1.1 Create `lib/std/src/sync/atomic/` directory
- [x] 1.2 Create `atomic/bool.tml`: AtomicBool type + all methods + Ordering import
- [x] 1.3 Create minimal `atomic/mod.tml`

## Phase 2: Integer atomics — 4/4

- [x] 2.1 Create `atomic/i32.tml`: AtomicI32
- [x] 2.2 Create `atomic/i64.tml`: AtomicI64
- [x] 2.3 Create `atomic/u32.tml`: AtomicU32
- [x] 2.4 Create `atomic/u64.tml`: AtomicU64

## Phase 3: Size + Ptr atomics — 4/4

- [x] 3.1 Create `atomic/usize.tml`: AtomicUsize
- [x] 3.2 Create `atomic/isize.tml`: AtomicIsize
- [x] 3.3 Create `atomic/ptr.tml`: AtomicPtr[T]
- [x] 3.4 Create `atomic/hints.tml`: spin_loop_hint

## Phase 4: Finalize — 3/3

- [x] 4.1 mod.tml: pub use all 9 submodules + re-export Ordering + all types + Send/Sync impls
- [x] 4.2 Renamed `atomic.tml` → `atomic.tml.bak`
- [x] 4.3 All atomic tests pass (atomic, atomic_bool, atomic_i32, atomic_ptr, atomic_usize, atomic_coverage). Pre-existing arc failures unrelated.

## Notes

- Each submodule imports `pub use std::sync::ordering::Ordering` for method params
- Send/Sync impls for all types live in mod.tml
- 3 pre-existing arc test failures (getelementptr sized error) unrelated to this change
