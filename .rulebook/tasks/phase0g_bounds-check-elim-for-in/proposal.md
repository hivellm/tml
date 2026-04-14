# Proposal: phase0g_bounds-check-elim-for-in

## Why
In the pattern `for i in 0 to list.len() { list.get(i) }`, the index `i` is structurally bounded to `[0, len)` by the loop — an out-of-bounds access is provably impossible. Despite this, `list.get(i)` still emits a runtime bounds check (compare + conditional panic branch) on every iteration. The wasted comparison and branch pollute the instruction stream and prevent LLVM from auto-vectorizing the loop body. TML List access benchmarks at 3 ns/op vs the theoretical bound of <1 ns/op without the check. Rust eliminates this check automatically via LLVM range metadata and loop invariant analysis. TML must do the same.

## What Changes
A new analysis pass (either in MIR or at LLVM IR emission) will detect the canonical `for i in 0 to container.len()` pattern and annotate the corresponding `get(i)` call as bounds-check-elided. The implementation will choose the most conservative approach from three options — (a) a targeted MIR rewrite pass that converts `BoundsChecked(get, i)` to `UncheckedGet(i)` when `i` is provably in range, (b) emitting LLVM `llvm.assume` to inform the optimizer that `i < len`, or (c) emitting `!range` metadata on the length load. The bounds check is only removed when the index source is the loop variable of a for-in loop bounded by the same collection's length. All other access patterns retain the full bounds check.

## Impact
- Affected specs: codegen/list-access, mir/bounds-analysis
- Affected code: new MIR analysis pass or `compiler/src/codegen/` for LLVM assume/range emission; `lib/std/src/collections/list.tml` may gain an `unchecked_get` intrinsic path
- Breaking change: NO
- User benefit: Up to 3x speedup for tight iteration loops over List. Enables LLVM auto-vectorization of numeric loops. Safety is fully preserved — the elimination only fires when provably correct.
