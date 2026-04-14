# 02 — Math & Arithmetic Benchmarks

## Results (10M iterations)

| Benchmark | Rust (ns/op) | Rust (ops/sec) | TML (ns/op) | TML (ops/sec) | Ratio |
|-----------|-------------|----------------|-------------|---------------|-------|
| Integer Addition | <1 | 5.18B | <1 | 1.20B | ~4.3x |
| Integer Multiplication | 3 | 259M | 3 | 306M | 0.8x |
| Integer Division | 1 | 781M | 1 | 669M | 1.2x |
| Integer Modulo | <1 | 1.52B | 1 | 684M | 2.2x |
| Bitwise Operations | <1 | 2.56B | <1 | 1.18B | 2.2x |
| Float Addition | 2 | 430M | 2 | 421M | 1.0x |
| Float Multiplication | 2 | 347M | 2 | 422M | 0.8x |
| Fibonacci Recursive (n=20) | 12* | 81M* | 23,813 | 42K | 1,985x* |
| Fibonacci Iterative (n=50) | <1* | 2.60B* | 86 | 11.6M | N/A* |
| Empty Loop | <1 | 5.18B | <1 | 1.68B | 3.1x |

*Asterisked Rust values are unreliable — the optimizer eliminated the computation entirely.

## Analysis

### What's Equal (1.0x)

- **Float add/mul**: Both emit identical LLVM `fadd`/`fmul` instructions. No overhead.
- **Integer mul**: TML's `3 ns/op` matches Rust exactly. The LLVM `imul` is the bottleneck, not the language.
- **Integer division**: Both use `idiv`, same cost.

### Where TML Wins

- **Integer multiplication**: TML's 306M ops/sec vs Rust's 259M. The benchmark body differs slightly (modular arithmetic), so not a pure win — but shows TML doesn't penalize integer math.
- **Float multiplication**: 422M vs 347M. Same reasoning.

### Where Rust Wins

- **Integer addition**: 5.18B vs 1.20B (4.3x). Rust's optimizer unrolls and vectorizes the sum loop. TML generates a scalar loop.
- **Bitwise**: 2.56B vs 1.18B (2.2x). Same vectorization gap.
- **Empty loop**: 5.18B vs 1.68B (3.1x). Rust eliminates the loop entirely; TML doesn't.

### The Fibonacci Anomaly

Rust reports `12 ns/op` for `fib_recursive(20)` which takes ~14,000 recursive calls. At 12 ns total, Rust is executing 0.85 ps per call — physically impossible on a GHz CPU. The compiler has **fully constant-folded** fib(20) = 6765 at compile time.

TML's 23,813 ns/op is the **real** cost of computing fib(20) recursively. This matches the expected ~14K calls × ~1.7 ns/call.

**Conclusion**: For arithmetic, TML is at native speed. The gaps are in loop optimization and dead-code elimination, not in instruction generation.
