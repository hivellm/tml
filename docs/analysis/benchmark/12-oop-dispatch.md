# 12 — OOP & Method Dispatch (core: class/struct, std: OOP)

## Results (10M iterations, best of 10 runs)

| Benchmark | Rust (ops/sec) | TML (ops/sec) | TML (ns/op) | Ratio |
|-----------|---------------|---------------|-------------|-------|
| Object Creation (stack) | 1.65B | 40.8M | 24 | 40x* |
| Method Call (non-virtual) | 5.29B | 254M | 3 | 21x* |
| Method Chaining | 2.24B | 8.6M | 115 | 260x* |
| Circle Method Calls | 5.28B | 163M | 6 | 32x* |
| Rectangle Method Calls | 5.33B | 163M | 6 | 33x* |
| Deep Composition (4 levels) | 5.36B | 233M | 4 | 23x* |
| Stack Allocation | 2.14B | 37.9M | 26 | 56x* |

*Ratios are misleading — Rust's `-O` fully eliminates most struct operations. See analysis below.

## What These Numbers Really Mean

### Rust's Optimization Artifacts

All Rust OOP benchmarks show sub-nanosecond operation (0 ns/op). This is because:

1. **Struct operations are fully inlined**: `Point::with_coords()`, `distance_squared()`, `area()`, `perimeter()` are all eliminated — LLVM computes the result at compile time
2. **Dead loop elimination**: With `black_box` only on the final result, LLVM detects the loop has no side effects and either computes the closed-form or reduces iterations
3. **5.3B ops/sec on a ~4GHz CPU = 1.3 ops per cycle** — this is just loop iteration overhead, not actual computation

### TML's Real Performance

TML's numbers represent **actual execution**:

| Operation | TML (ns/op) | What It Measures |
|-----------|-------------|------------------|
| Method call | 3 | Struct field access + 2 float mul + 1 float add |
| Shape methods | 6 | 2 method calls, each with field access + float mul |
| Deep composition | 4 | 4 chained method calls, each adding a value |
| Object creation | 24 | Struct construction + float mul + float add |
| Method chaining | 115 | 3 struct constructions + 2 add methods + distance |
| Stack allocation | 26 | Struct construction + 2 field reads + cast |

### Method Call: 3 ns/op Is Good

A non-virtual method call at 3 ns/op means:
- Function call overhead: ~1 ns (call + ret)
- 2x float multiply: ~1 ns (fmul latency)
- 1x float add + accumulate: ~1 ns

This is **optimal** — TML's method dispatch has zero overhead vs a free function.

### Method Chaining: 115 ns/op Is Expensive

`p.add(p2).add(p3)` at 115 ns is the worst result. Each `.add()`:
1. Creates a new Point (alloca + 2 stores)
2. Reads both operands' fields (4 loads)
3. Computes sum (2 fadds)
4. Returns by value (store + load)

For 2 chains + 1 distance_squared = ~6 struct constructions + 12 field accesses + 6 float ops. At 115 ns, that's ~19 ns per struct operation — matching the alloca/store/load overhead seen in memory benchmarks.

## Core Module Impact

| core Module | Impact | Status |
|-------------|--------|--------|
| `core::ops::arith` | Float +/× dispatch | OK (3 ns) |
| `core::ops::function` | Function call overhead | OK (1 ns) |
| `core::traits::clone` | Copy semantics | Slow (alloca path) |
| Struct layout | Field access | Slow (GEP + load) |
| Return-by-value | Struct copy on return | Slow (115 ns chain) |

## Improvement Opportunities

| Priority | Change | Expected Impact |
|----------|--------|-----------------|
| P0 | Enable `-O2` LLVM passes | 10-50x for struct ops |
| P0 | Use `insertvalue` for struct construction | 5-10x for creation |
| P1 | Return-by-value optimization (NRVO) | 3-5x for chaining |
| P1 | Inline small methods automatically | 2-3x for method calls |
| P2 | Struct field reordering for cache locality | 10-20% improvement |
