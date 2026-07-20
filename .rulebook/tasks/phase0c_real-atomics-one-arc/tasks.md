# phase0c — Real atomics (all widths, honored Ordering) + one Arc

> Filed 2026-07-20 from `docs/analysis/language-deep-review/` finding L-102
> (06-runtime-concurrency.md). HARD prerequisite for phase0l (in-process
> parallel tests): running TML code on threads with fake atomics is unsound by
> library design.

## Motivation

The entire `std::sync::atomic` family is not atomic: `load`/`store`/`swap`/
`compare_exchange`/`fetch_add` on `AtomicI64`/`AtomicU64` are plain reads,
writes and RMWs inside `lowlevel` blocks, and the `Ordering` parameter is never
used (`lib/std/src/sync/atomic/i64.tml:25-85`, `u64.tml:25-58`). `std` Arc
builds its strong/weak counts on these (`lib/std/src/sync/arc.tml:227,391,433,458`)
— cross-thread clone/drop is a data race. Real atomics exist but only as
I32-only, seq_cst-only builtins (`compiler/src/codegen/llvm/builtins/atomic.cpp:36-141`);
a second, correct Arc (`core::alloc::sync::Sync[T]`) uses C FFI interlocked ops.

## 1. Implementation
- [ ] 1.1 Extend `builtins/atomic.cpp` to i64/u64/ptr widths and map the
  `Ordering` argument to LLVM orderings (constant-folded at codegen; a
  non-constant ordering is a compile error or falls back seq_cst — record the
  choice).
- [ ] 1.2 Rewrite `lib/std/src/sync/atomic/*.tml` methods on the intrinsics —
  no plain `lowlevel` reads/writes remain; `Ordering` honored end-to-end.
- [ ] 1.3 One Arc: collapse `lib/std/src/sync/arc.tml` onto
  `core::alloc::sync::Sync[T]` (or rebuild Sync's counters on the new
  intrinsics and alias std Arc to it). One implementation, atomic by
  construction; delete the other.
- [ ] 1.4 Cross-thread stress fixture: N `spawn_fn` threads × M clone/drop
  cycles on one Arc; exact final strong count; 20/20 reruns clean.
- [ ] 1.5 If trivial after 1.1: inline `Sync[T]` refcount ops via the atomic
  intrinsics instead of out-of-line FFI calls (L-107); otherwise record as
  follow-up here.

## 2. Tail (docs + tests — check or waive with tailWaiver)
- [ ] 2.1 Update documentation (atomics semantics + Ordering table; one Arc)
- [ ] 2.2 Write tests covering the new behavior (per-width per-op fixtures;
  ordering smoke tests; Arc stress)
- [ ] 2.3 Run tests and confirm they pass
