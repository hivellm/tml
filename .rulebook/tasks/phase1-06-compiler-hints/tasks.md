# Tasks: Compiler Hints — Optimization Intrinsics

**Status**: Proposed
**Priority**: MEDIUM
**Phase**: 1 — Foundation

## Motivation

Rust's `core::hint` provides `unreachable_unchecked` (tell optimizer code path is impossible), `black_box` (prevent compiler from optimizing away benchmark code), and `spin_loop` (CPU hint for busy-wait loops). TML already has `spin_lock`/`spin_unlock` in core/sync but lacks the general-purpose hints.

## Phase 1: Hint Functions (`lib/core/src/hint.tml`)

- [ ] 1.1 Create `lib/core/src/hint.tml`
- [ ] 1.2 `unreachable_unchecked() -> !` — UB if reached, allows optimizer to eliminate impossible paths. Emit LLVM `unreachable` instruction
- [ ] 1.3 `black_box[T](x: T) -> T` — prevent optimizer from constant-folding. Emit LLVM inline asm with memory clobber
- [ ] 1.4 `spin_loop_hint()` — emit x86 `pause` instruction (reduces power on busy-wait). Emit LLVM `@llvm.x86.sse2.pause` intrinsic
- [ ] 1.5 `cold()` — mark function as unlikely to be called (hint for branch prediction). Emit LLVM `cold` attribute
- [ ] 1.6 `likely(b: Bool) -> Bool` / `unlikely(b: Bool) -> Bool` — branch prediction hints. Emit LLVM `@llvm.expect.i1`
- [ ] 1.7 Update `core/mod.tml` to export `hint` module
- [ ] 1.8 Write tests: `lib/core/tests/hint/basic.test.tml` — verify functions compile and don't crash
- [ ] 1.9 Write IR tests: verify LLVM IR contains expected intrinsics
