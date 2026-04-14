# 19 — Per-Operation Performance Heatmap

All measured TML operations ranked by absolute speed, with Rust comparison where available.

## Sub-Nanosecond (<1 ns/op) — Native Speed

| Operation | TML (ns) | TML (ops/sec) | Module | vs Rust |
|-----------|---------|---------------|--------|---------|
| Array Init | <1 | 1.62B | core::array | — |
| Empty Loop | <1 | 1.68B | (compiler) | 3.1x |
| Array Copy 1000 | <1 | 1.15B | core::array | **TML wins** |
| Array Fill 1000 | <1 | 1.46B | core::array | **TML wins** |
| Linear Search (arr) | <1 | 1.14B | core::array | — |
| Manual Loop (array) | <1 | 1.24B | (compiler) | — |
| Fold/Reduce sim | <1 | 1.01B | core::iter (sim) | — |
| Loop iteration | <1 | 1.19B | (compiler) | 1.0x |

## 1-2 ns/op — Excellent

| Operation | TML (ns) | TML (ops/sec) | Module | vs Rust |
|-----------|---------|---------------|--------|---------|
| Integer Addition | <1 | 1.20B | core::ops::arith | 4.3x* |
| Integer Division | 1 | 669M | core::ops::arith | 1.2x |
| Integer Modulo | 1 | 684M | core::ops::arith | 2.2x* |
| Bitwise Ops | <1 | 1.18B | core::ops::bit | 2.2x* |
| Float Addition | 2 | 421M | core::ops::arith | 1.0x |
| Float Multiplication | 2 | 422M | core::ops::arith | **TML wins** |
| Int Widen I32→I64 | 1 | 866M | core::num | 6x* |
| Int Narrow I64→I32 | 1 | 896M | core::num | 6x* |
| U64→I64 cast | 1 | 899M | core::num | 6x* |
| I64→U64 cast | 1 | 877M | core::num | 6x* |
| F64→I64 cast | 1 | 837M | core::num | 2.1x |
| Byte Chain cast | 1 | 671M | core::num | 2.6x |
| Inline func call | 1 | 727M | core::ops::fn | 1.0x |
| Direct func call | 1 | 727M | core::ops::fn | 1.0x |
| If-else (4 branch) | 1 | 509M | (compiler) | 8.4x* |
| Nested if (4 level) | 1 | 668M | (compiler) | **TML wins** |
| Loop + continue | 1 | 718M | (compiler) | — |
| Ternary chain | 1 | 527M | (compiler) | — |
| Array Sequential | 1 | 956M | core::array | — |
| Array Random | 1 | 579M | core::array | — |
| Array Write | 1 | 641M | core::array | — |
| Sequential mem access | 1 | 678M | core::ptr | 6.7x* |
| Random mem access | 1 | 665M | core::ptr | 2.6x |
| Function Pointer | 1 | 593M | core::ops::fn | 8.2x* |
| Filter sim | 1 | 729M | core::iter (sim) | 8.4x* |
| Map sim | 1 | 669M | core::iter (sim) | — |
| Chain sim | 1 | 714M | core::iter (sim) | — |
| Fn Ptr Switch | 2 | 440M | core::ops::fn | — |
| Stack Struct Small | 2 | 412M | (layout) | 11.6x* |
| Struct Field Access | 2 | 475M | (layout) | 10.9x* |
| Float Narrow F64→F32 | 2 | 421M | core::num | 6.9x* |
| I64→F64 cast | 2 | 441M | core::num | 9.2x* |

*Asterisked ratios reflect Rust's optimizer eliminating the computation, not real performance gaps.

## 3-5 ns/op — Good

| Operation | TML (ns) | TML (ops/sec) | Module | vs Rust |
|-----------|---------|---------------|--------|---------|
| Integer Multiply | 3 | 306M | core::ops::arith | 1.0x |
| When Dense (10) | 3 | 331M | (compiler) | 9.5x* |
| When Sparse (10) | 3 | 301M | (compiler) | 3.9x* |
| List Random Access | 3 | 258M | std::collections::list | 5.5x* |
| List Iteration | 3 | 263M | std::collections::list | 65x* |
| Point Creation | 3 | 324M | (layout) | 10.7x* |
| Nested Struct | 3 | 284M | (layout) | 18x* |
| OOP Method Call | 3 | 254M | (class) | 21x* |
| Many Params (6) | 4 | 212M | core::ops::fn | — |
| Short-Circuit AND | 4 | 236M | (compiler) | 4.2x |
| Short-Circuit OR | 4 | 229M | (compiler) | 3.8x |
| List Set | 4 | 224M | std::collections::list | 13x* |
| Deep Composition (4) | 4 | 233M | (class) | 23x* |
| Float Widen F32→F64 | 4 | 201M | core::num | 14.3x* |
| Mixed Type Arith | 5 | 194M | core::num | 4.2x |
| List Push (reserved) | 5 | 170M | std::collections::list | 3.6x |
| List Pop | 5 | 191M | std::collections::list | 2.4x |
| Fn Composition | 5 | 194M | core::ops::fn | — |

## 5-30 ns/op — Moderate

| Operation | TML (ns) | TML (ops/sec) | Module | vs Rust |
|-----------|---------|---------------|--------|---------|
| Circle Methods | 6 | 163M | (class) | 32x* |
| Rectangle Methods | 6 | 163M | (class) | 33x* |
| Stack Struct Medium | 7 | 140M | (layout) | 9.2x* |
| List Push (grow) | 8 | 115M | std::collections::list | 2.0x |
| Higher Order Fn | 12 | 78M | core::ops::fn | N/A* |
| HashMap Lookup | 15 | 66M | std::collections::hashmap | 1.4x |
| HashMap Contains | 13 | 75M | std::collections::hashmap | 1.3x |
| OOP Object Create | 24 | 40.8M | (class) | 40x* |
| OOP Stack Alloc | 26 | 37.9M | (class) | 56x* |

## 30-200 ns/op — Slow

| Operation | TML (ns) | TML (ops/sec) | Module | vs Rust |
|-----------|---------|---------------|--------|---------|
| Fib Iterative (n=50) | 86 | 11.6M | (recursion) | N/A* |
| HashMap Insert | 158 | 6.3M | std::collections::hashmap | 1.6x |
| HashMap Insert (res) | 109 | 9.1M | std::collections::hashmap | 1.5x |
| HashMap Remove | 122 | 8.2M | std::collections::hashmap | **0.77x** |
| OOP Method Chain | 115 | 8.6M | (class) | 260x* |
| Mutual Recursion (100) | 169 | 5.9M | (recursion) | — |
| Base64 Encode (13B) | 134 | 7.4M | core::encoding::base64 | 3.1x |
| Hex Encode (13B) | 119 | 8.4M | core::encoding::hex | **TML wins** |
| Base32 Encode (13B) | 123 | 8.1M | core::encoding::base32 | — |

## 200+ ns/op — Expensive

| Operation | TML (ns) | TML (ops/sec) | Module | vs Rust |
|-----------|---------|---------------|--------|---------|
| Base64 Decode (20c) | 299 | 3.3M | core::encoding::base64 | — |
| Hex Decode (26c) | 324 | 3.1M | core::encoding::hex | — |
| Base64 Encode (95B) | 346 | 2.9M | core::encoding::base64 | 2.4x |
| Fib Recursive (n=20) | 23,813 | 42K | (recursion) | 2x (real) |

## Performance Tiers

```
Tier 1: Native     (<2 ns)   — arithmetic, casts, array ops, loops
Tier 2: Fast       (2-10 ns) — collections access, struct ops, function calls
Tier 3: Moderate   (10-100 ns) — HashMap ops, method dispatch
Tier 4: Slow       (100-500 ns) — encoding, method chaining, struct-heavy ops
Tier 5: Expensive  (>500 ns) — recursion, complex algorithms
```

**Tier 1 matches Rust. Tier 2-3 have optimization opportunities. Tier 4-5 gaps are mostly from struct alloca/load/store overhead.**
