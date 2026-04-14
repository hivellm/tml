# 03 — Control Flow Benchmarks

## Results (10M iterations)

| Benchmark | Rust (ns/op) | Rust (ops/sec) | TML (ns/op) | TML (ops/sec) | Ratio |
|-----------|-------------|----------------|-------------|---------------|-------|
| If-Else Chain (4 branches) | <1 | 4.30B | 1 | 509M | 8.4x |
| Nested If (4 levels) | 1 | 636M | 1 | 668M | 0.95x |
| When/Match Dense (10 cases) | <1 | 3.16B | 3 | 331M | 9.5x |
| When/Match Sparse (10 cases) | <1 | 1.18B | 3 | 301M | 3.9x |
| Loop | <1 | optimized | <1 | 1.20B | N/A |
| Nested Loops (1000x1000) | — | — | 1 | 675M | — |
| Loop + Continue | — | — | 1 | 718M | — |
| Ternary Chain | — | — | 1 | 527M | — |
| Short-Circuit AND | 1 | 993M | 4 | 236M | 4.2x |
| Short-Circuit OR | 1 | 863M | 4 | 229M | 3.8x |

## Analysis

### Nested If — TML Wins (0.95x)

The only benchmark where TML outperforms Rust. Both generate similar branch chains, and TML's branch predictor interaction is slightly favorable here. This confirms TML's basic branching codegen is correct.

### If-Else Chain — 8.4x Gap

Rust: 4.30B ops/sec. This is a fully optimized computed-goto or predicated move. TML: 509M ops/sec, using a chain of `cmp` + conditional branches. The optimizer doesn't convert 4-way if-else into a table lookup or CMOV.

**Root cause**: TML doesn't lower if-else chains to select instructions or jump tables.

### When/Match Dense — 9.5x Gap

10 consecutive cases (0-9) — the ideal candidate for a jump table. Rust compiles this to a single indexed jump. TML generates a linear scan of comparisons.

**Root cause**: `when` expression doesn't emit LLVM `switch` instruction for dense integer patterns.

### When/Match Sparse — 3.9x Gap

10 sparse cases (1, 10, 100, 200, ..., 700). Less amenable to jump tables, but Rust still uses `switch` which LLVM lowers to a binary search tree. TML uses linear comparisons.

### Short-Circuit Booleans — 4x Gap

`and`/`or` short-circuit evaluation. TML: 4 ns/op. Rust: 1 ns/op. The 4x gap suggests TML generates extra basic blocks and phi nodes for each `and`/`or` operand, while Rust chains the conditions into a single conditional branch sequence.

**Root cause**: Boolean short-circuit codegen creates unnecessary phi-node merges.

### Loop-Related — No Gap

Plain loops, nested loops, loop+continue, and ternary chains all run at 500M-1.2B ops/sec. This matches expected single-instruction-per-iteration throughput. TML's loop codegen is solid.

## Codegen Improvement Opportunities

| Priority | Change | Expected Impact |
|----------|--------|-----------------|
| P0 | Emit `switch` for dense `when` | 5-10x improvement |
| P1 | Lower if-else chains to CMOV/select | 4-8x improvement |
| P1 | Optimize short-circuit boolean codegen | 2-4x improvement |
| P2 | Binary search for sparse `when` | 2x improvement |
