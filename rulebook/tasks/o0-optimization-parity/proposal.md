# Proposal: O0 Optimization Parity with Rust

## Why

Rust applies a significant number of MIR optimizations even at `-C opt-level=0` (debug mode). Rust's `mir-opt-level` defaults to **1** regardless of the LLVM optimization level, which means ~40 MIR passes still run in debug builds. These are semantics-preserving transformations that don't affect debuggability but significantly improve debug-mode performance.

TML currently only runs 3 passes at O0: `SretConversion` (ABI correctness), `InstSimplify`, and `StrengthReduction`. This means debug builds are significantly slower than they need to be — carrying dead code, redundant loads, unoptimized control flow, and unnecessary memory operations that Rust eliminates even in debug mode.

The goal is to bring TML's O0 pipeline to parity with Rust's always-on MIR optimizations, organized by priority and safety.

## What Changes

### O0 Pipeline Expansion

Add safe, semantics-preserving MIR passes to the O0 pipeline in `mir_pass.cpp`. These are passes that:
1. Do NOT change observable behavior
2. Do NOT affect debug info quality
3. Are already implemented in TML (just not enabled at O0)
4. Match what Rust does at `mir-opt-level=1`

### New Passes to Implement

Some Rust O0 passes have no TML equivalent yet:
- `DestinationPropagation` — eliminates intermediate copies
- `DataflowConstProp` — SSA-aware constant propagation
- `RemoveUnneededDrops` — elide drops for types that don't need them
- `UnreachablePropagation` — mark unreachable branches after const-prop
- `InstCombine` (Rust-specific) — algebraic simplifications beyond what InstSimplify does
- Niche enum layout optimization (partial — only `Maybe[T]` for primitives is done)

### Components Affected

- `compiler/src/mir/mir_pass.cpp` — Pipeline configuration (both overloads)
- `compiler/src/mir/passes/` — Individual pass implementations (existing and new)
- `compiler/include/mir/passes/` — Pass headers

### Breaking Changes

None. All changes are internal optimizations. Observable behavior is identical.

## Impact

- Affected specs: None (internal optimization only)
- Affected code: `compiler/src/mir/mir_pass.cpp`, `compiler/src/mir/passes/`
- Breaking change: NO
- User benefit: 2-5x faster debug-mode executables, smaller IR, faster compile-test cycles

## Dependencies

- Existing MIR pass infrastructure (already functional)
- Individual passes must be verified safe for O0 (no optimization assumptions)

## Success Criteria

1. O0 pipeline includes all safe passes listed in Phase 1-2
2. Full test suite passes at O0 with new pipeline
3. No regressions in compile time (passes should be fast)
4. Debug-mode executables run measurably faster on benchmarks
5. MIR output at O0 shows elimination of dead code, redundant copies, etc.
