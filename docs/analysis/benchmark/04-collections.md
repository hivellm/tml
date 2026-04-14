# 04 — Collection Benchmarks

## List vs Vec (10M operations)

| Benchmark | Rust (ns/op) | Rust (ops/sec) | TML (ns/op) | TML (ops/sec) | Ratio |
|-----------|-------------|----------------|-------------|---------------|-------|
| Push (grow from empty) | 4 | 231M | 8 | 115M | 2.0x |
| Push (pre-reserved) | 1 | 604M | 5 | 170M | 3.6x |
| Random Access | <1 | 1.41B | 3 | 258M | 5.5x |
| Iteration | <1 | 17.1B | 3 | 263M | 65x* |
| Pop | 2 | 461M | 5 | 191M | 2.4x |
| Set | <1 | 2.95B | 4 | 224M | 13.2x* |

*Rust's iteration and set benchmarks are fully optimized away by the compiler (17B and 2.9B ops/sec are physically impossible for per-element work). Real gap is likely 2-4x.

## Realistic List Gaps (excluding optimized-away)

| Operation | Realistic Ratio | Confidence |
|-----------|----------------|------------|
| Push (grow) | **2.0x** | High — both do real work |
| Push (reserved) | **3.6x** | High — TML has per-push overhead |
| Random Access | **3-5x** | Medium — TML bounds checks |
| Pop | **2.4x** | High — both do real work |

### Root Causes

1. **Bounds checking**: TML's `list.get(i)` checks `i < len` on every access. Rust's `vec[i]` also checks, but `-O` can elide the check when the index is provably in-range (e.g., `for i in 0..vec.len()`).

2. **Push overhead**: TML's push with pre-reserved capacity is 5 ns vs Rust's 1 ns. The 4 ns gap suggests TML does extra work per push — likely a function call overhead (push isn't inlined) + capacity check even when unnecessary.

3. **No auto-vectorization**: Rust's iteration can be vectorized by LLVM when the loop body is simple. TML's `.get()` method call prevents vectorization.

## HashMap (1M operations)

| Benchmark | Rust (ns/op) | Rust (ops/sec) | TML (ns/op) | TML (ops/sec) | Ratio |
|-----------|-------------|----------------|-------------|---------------|-------|
| Insert | 97 | 10.2M | 158 | 6.3M | 1.6x |
| Insert (reserved) | 72 | 13.8M | 109 | 9.1M | 1.5x |
| Lookup | 11 | 86.4M | 15 | 66.0M | 1.4x |
| Contains | 10 | 92.1M | 13 | 75.2M | 1.3x |
| Remove | 158 | 6.3M | 122 | 8.2M | **0.77x** |

### TML HashMap Wins Remove

TML's HashMap remove is **23% faster** than Rust's. Possible reasons:
- Different collision resolution strategy (Robin Hood vs linear probing)
- Different tombstone handling (lazy compaction vs immediate)
- Simpler hash function with less overhead

### Overall HashMap Assessment

HashMap is TML's **best-performing collection**. The 1.3-1.6x gap for insert/lookup is remarkably small. This suggests:
- The hash function is competitive with Rust's SipHash/default hasher
- Memory layout of entries is efficient
- Probe sequence is well-implemented

## Collections — Array (Fixed-Size, 10M operations)

| Benchmark | TML (ns/op) | TML (ops/sec) |
|-----------|-------------|---------------|
| Sequential Read | 1 | 956M |
| Random Access | 1 | 579M |
| Write | 1 | 641M |
| Initialization | <1 | 1.62B |
| Linear Search | <1 | 1.14B |
| Accumulate Sum | 1 | 705M |

Fixed-size arrays in TML are fast — no bounds checking overhead, direct memory access. Initialization and linear search exceed 1B ops/sec.

## Improvement Opportunities

| Priority | Change | Expected Impact |
|----------|--------|-----------------|
| P0 | Inline `List.push()` and `List.get()` | 2x improvement on all List ops |
| P1 | Bounds-check elimination for `for-in` loops | 2-3x for iteration |
| P1 | SIMD vectorization for List iteration | 4-8x for simple loop bodies |
| P2 | Profile HashMap hash function vs Rust | Could close remaining 1.3-1.5x gap |
