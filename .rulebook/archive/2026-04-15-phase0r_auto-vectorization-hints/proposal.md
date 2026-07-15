# Proposal: phase0r_auto-vectorization-hints

## Why
TML for-in loops over arrays and List slices do not emit LLVM vectorization metadata. LLVM can auto-vectorize scalar loops using SSE/AVX when the loop body is simple (no aliasing, fixed stride, numeric ops) and the `llvm.loop` metadata block includes `llvm.loop.vectorize.enable` and `llvm.loop.vectorize.width`. Rust's `for x in slice` loops emit this metadata automatically and routinely achieve 2-4x throughput on numeric workloads via SIMD. TML's array bulk-fill benchmark already shows TML winning at bulk copy (`memset`/`memcpy`), but element-wise numeric transforms are scalar. This gap matters for codec, image, signal-processing, and ml workloads. See `docs/analysis/benchmark/05-memory-structs.md`.

## What Changes
1. The MIR→LLVM emission for `ForInInst` over a contiguous range (array index loop or slice iteration) will attach `llvm.loop` metadata to the back-edge of the loop basic block with:
   - `!{!"llvm.loop.vectorize.enable", i1 true}`
   - `!{!"llvm.loop.vectorize.width", i32 0}` (0 = LLVM chooses best width)
   - `!{!"llvm.loop.interleave.count", i32 0}`
2. A `@vectorize` attribute on for-in loops will let users force or disable vectorization: `@vectorize(width=8) for i in 0 to n { ... }`.
3. Aliasing: for slice/array bodies with no pointer aliasing between reads and writes, emit `noalias` metadata on the load/store instructions.

## Impact
- Affected specs: codegen/loop-vectorization
- Affected code: MIR→LLVM emission for loop instructions in `compiler/src/codegen/instructions.cpp`
- Breaking change: NO
- User benefit: 2-4x speedup for numeric array loops on modern CPUs with SSE2/AVX2. Zero source-level changes required for common patterns.
