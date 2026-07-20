# Proposal: phase0c_real-atomics-one-arc

## Why
The advertised atomic API is a mirage — plain non-atomic ops with `Ordering`
ignored — so `std` Arc across threads is undefined behavior today, and any
parallel execution of TML code (including the planned in-process test runner)
is unsound (analysis L-102).

## What Changes
Atomic builtins gain i64/u64/ptr widths with real LLVM orderings; the
std atomic types are reimplemented on them; the two Arc implementations
collapse into one sound one.

## Impact
- Affected specs: docs/specs concurrency/memory sections
- Affected code: compiler/src/codegen/llvm/builtins/atomic.cpp, lib/std/src/sync/atomic/*.tml, lib/std/src/sync/arc.tml, lib/core/src/alloc/sync.tml
- Breaking change: NO (API shape unchanged; semantics become what the docs already claim)
- User benefit: trustworthy concurrency primitives; prerequisite for parallel testing and any threaded workload
