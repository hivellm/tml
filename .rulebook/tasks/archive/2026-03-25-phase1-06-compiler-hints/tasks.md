# Tasks: Compiler Hints — Optimization Intrinsics

**Status**: COMPLETE (9/9 items done)
**Priority**: MEDIUM
**Phase**: 1 — Foundation

## Motivation

Rust's `core::hint` provides `unreachable_unchecked` (tell optimizer code path is impossible), `black_box` (prevent compiler from optimizing away benchmark code), and `spin_loop` (CPU hint for busy-wait loops). TML already has `spin_lock`/`spin_unlock` in core/sync but lacks the general-purpose hints.

## Phase 1: Hint Functions (`lib/core/src/hint.tml`)

- [x] 1.1 Create `lib/core/src/hint.tml`
- [x] 1.2 `unreachable_unchecked()` — emits LLVM `unreachable` instruction
- [x] 1.3 `black_box_i64/bool/f64(x) -> T` — inline asm with memory clobber (monomorphic variants)
- [x] 1.4 `spin_loop_hint()` — emits x86 `pause` via inline asm (C++ intrinsic added)
- [x] 1.5 `assume(cond: Bool)` — emits `@llvm.assume(i1)` (replaces `cold()` — more useful)
- [x] 1.6 `likely(b: Bool) -> Bool` / `unlikely(b: Bool) -> Bool` — emits `@llvm.expect.i1`
- [x] 1.7 Updated `core/mod.tml` to export `hint` module
- [x] 1.8 Tests: 9 tests in `lib/core/tests/hint/basic.test.tml` — all passing
- [x] 1.9 C++ intrinsics added: `black_box` (inline asm) and `spin_loop_hint` (pause) in intrinsics_extended.cpp
