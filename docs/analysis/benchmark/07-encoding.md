# 07 — Encoding Benchmarks

## Results (100K iterations)

| Benchmark | Rust (ns/op) | Rust (ops/sec) | TML (ns/op) | TML (ops/sec) | Ratio |
|-----------|-------------|----------------|-------------|---------------|-------|
| Base64 Encode (13B) | 43 | 23.1M | 134 | 7.4M | 3.1x |
| Base64 Encode (95B) | 145 | 6.9M | 346 | 2.9M | 2.4x |
| Base64 Decode (20 chars) | — | — | 299 | 3.3M | — |
| Hex Encode (13B) | 583* | 1.7M | 119 | 8.4M | **0.2x** |
| Hex Decode (26 chars) | — | — | 324 | 3.1M | — |
| Base32 Encode (13B) | — | — | 123 | 8.1M | — |

*Rust hex encode uses `format!("{:02x}", b)` per byte — a known anti-pattern. With a lookup-table implementation, Rust would be ~20-50 ns.

## Analysis

### Base64 — 2.4-3.1x Gap

The gap shrinks with larger inputs (3.1x → 2.4x at 95 bytes), suggesting per-call overhead dominates for small inputs. Likely causes:

1. **String allocation**: TML allocates a new string per encode call. Rust's `String::with_capacity()` pre-allocates.
2. **No SIMD**: TML's Base64 is scalar. Rust's manual implementation is also scalar, but benefits from better loop optimization.
3. **Function call overhead**: TML's encoding module goes through more indirection.

### Throughput Perspective

| Input Size | Rust (MB/s) | TML (MB/s) | Ratio |
|-----------|-------------|-------------|-------|
| 13 bytes | 302 | 97 | 3.1x |
| 95 bytes | 655 | 275 | 2.4x |

At 95 bytes, TML achieves 275 MB/s Base64 encoding throughput. For most applications (API payloads, JWT tokens), this is more than sufficient. The gap matters for bulk data processing.

### Hex Encode — TML Wins (5x)

TML: 119 ns (8.4M ops/sec). Rust: 583 ns (1.7M ops/sec). TML is 5x faster.

However, this is an unfair comparison — the Rust benchmark uses `format!()` which allocates per byte. A proper Rust hex encoder with a lookup table would be ~20-40 ns. Still, TML's hex implementation is well-optimized.

### Memory Leak Issue

The encoding benchmark reported:
```
200000 leak(s), 2800000 bytes lost
#1: 14 bytes per leak (tag=mem_alloc)
```

200K leaks at 14 bytes each = strings not being freed. Each encode call allocates a result string that isn't deallocated. This is a **critical issue** for any long-running process using encoding functions in loops.

## Improvement Opportunities

| Priority | Change | Expected Impact |
|----------|--------|-----------------|
| P0 | Fix memory leaks in encoding functions | Correctness fix |
| P1 | Pre-allocate output strings with known size | 30-50% improvement |
| P1 | SIMD Base64 encode (AVX2) | 4-8x improvement |
| P2 | Lookup-table hex encode | Already fast, minor gain |
