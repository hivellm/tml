# Proposal: Compiler Hints — Optimization Intrinsics

## Status: PROPOSED

## Summary

This task adds a `core::hint` module exposing LLVM optimization intrinsics that TML code cannot currently access: `unreachable_unchecked` (eliminate dead branches), `black_box` (prevent benchmark constant-folding), `spin_loop_hint` (x86 PAUSE instruction for busy-wait loops), and `likely`/`unlikely` (branch prediction hints). These are thin wrappers over LLVM intrinsics with no runtime overhead.

## Motivation

High-performance TML code — particularly the HTTP server, async executor, and collection implementations — needs these primitives. Without `likely`/`unlikely`, the compiler generates equivalent code for hot and cold paths. Without `spin_loop_hint`, busy-wait loops on x86 burn CPU cycles unnecessarily and interfere with hyperthreading. Without `black_box`, benchmark functions are constant-folded away and measure nothing. Without `unreachable_unchecked`, exhaustive match arms that are logically impossible still generate dead code that the optimizer cannot eliminate without the hint.

Rust's `core::hint` has been stable since 1.27. TML's async executor already has `spin_lock`/`spin_unlock` but uses a manual inline asm block rather than a proper hint abstraction.

## Design

Each hint maps to a specific LLVM intrinsic or IR construct:

| Function | LLVM IR |
|---|---|
| `unreachable_unchecked()` | `unreachable` instruction |
| `black_box[T](x)` | inline asm with `""` constraints + `"memory"` clobber |
| `spin_loop_hint()` | `call void @llvm.x86.sse2.pause()` |
| `cold()` attribute | `cold` function attribute on the containing function |
| `likely(b)` | `call i1 @llvm.expect.i1(i1 %b, i1 true)` |
| `unlikely(b)` | `call i1 @llvm.expect.i1(i1 %b, i1 false)` |

All functions are implemented as `lowlevel` blocks in `lib/core/src/hint.tml` — this is one of the legitimate uses of lowlevel (FFI to LLVM intrinsics). No C files are added.

## What Changes

- New: `lib/core/src/hint.tml` — all six hint functions
- Modified: `lib/core/src/mod.tml` — add `hint` module export
- New: `lib/core/tests/hint/basic.test.tml` — smoke tests that the functions compile and return correct types
- Future (not in this task): update async executor's inline spin to use `spin_loop_hint()`

## Dependencies

- Depends on: nothing (lowlevel intrinsics, no library dependencies)
- Enables: `phase2-02-semaphore` (spin paths), `phase5-01-simd-optimization` (likely/unlikely in hot paths), any benchmarking infrastructure

## Risks

- `unreachable_unchecked` is undefined behavior if reached — must be clearly documented; misuse causes silent memory corruption
- `spin_loop_hint` is x86-specific; on ARM the equivalent is `YIELD`; the implementation should use `#if X86_64` / `#if ARM64` conditional compilation
- `cold` as a function-level attribute rather than a site-level hint has limited granularity — consider whether it belongs on the call site or the function definition
