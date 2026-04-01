# Proposal: Convert atomic.tml to Directory Module

## Status: PROPOSED

## Why

`lib/std/src/sync/atomic.tml` (1,507 lines) contains 9 public types + 1 public function. Each atomic type (AtomicBool, AtomicI32, etc.) is independent — they share no state or methods.

## Key Insight: Same Pattern as str.tml

The module resolver handles nested directories. `use std::sync::atomic` tries `sync/atomic.tml` then `sync/atomic/mod.tml`. The existing `sync/mod.tml` already re-exports atomic types individually — those paths (`std::sync::atomic::AtomicBool`) remain valid after the split.

## What Changes

Convert `atomic.tml` → `atomic/` directory with one file per type.

| File | Type | Lines |
|------|------|-------|
| `atomic/bool.tml` | AtomicBool | ~300 |
| `atomic/i32.tml` | AtomicI32 | ~190 |
| `atomic/i64.tml` | AtomicI64 | ~155 |
| `atomic/u32.tml` | AtomicU32 | ~155 |
| `atomic/u64.tml` | AtomicU64 | ~155 |
| `atomic/usize.tml` | AtomicUsize | ~155 |
| `atomic/isize.tml` | AtomicIsize | ~165 |
| `atomic/ptr.tml` | AtomicPtr[T] | ~115 |
| `atomic/hints.tml` | spin_loop_hint | ~15 |
| `atomic/mod.tml` | pub use all + Ordering | ~20 |

Each file needs `pub use std::sync::ordering::Ordering` since the methods use it.
