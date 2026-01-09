# TML Optimization Benchmark Report

Generated: 2026-01-08 18:39:21

## Measurement Method

**Both TML and Rust are measured using LLVM IR instruction counts.**

This ensures a fair comparison since both compilers target LLVM.

## Optimization Levels

| Level | TML | Rust |
|-------|-----|------|
| O0 | No optimizations | No optimizations |
| O1 | Constant folding/propagation | Basic optimizations |
| O2 | O1 + CSE, copy prop, DCE, inlining | Release mode (-O) |
| O3 | O2 + aggressive inlining | Maximum optimization |

## TML Results (LLVM IR)

### optimization_bench.tml

| Opt Level | Functions | Instructions | Reduction |
|-----------|-----------|--------------|-----------|
| O0 | 28 | 550 | - |
| O1 | 27 | 406 | 26.2% |
| O2 | 25 | 247 | 55.1% |
| O3 | 25 | 234 | 57.5% |

**Total instruction reduction (O0 → O2): 55.1%**

### algorithms.tml

| Opt Level | Functions | Instructions | Reduction |
|-----------|-----------|--------------|-----------|
| O0 | 21 | 525 | - |
| O1 | 21 | 374 | 28.8% |
| O2 | 21 | 332 | 36.8% |
| O3 | 21 | 332 | 36.8% |

**Total instruction reduction (O0 → O2): 36.8%**

## Rust Results (LLVM IR)

### optimization_bench.rs (equivalent to optimization_bench.tml)

| Opt Level | Functions | Instructions | Reduction |
|-----------|-----------|--------------|-----------|
| O0 | 42 | 1103 | - |
| O1 | 7 | 179 | 83.8% |
| O2 | 7 | 179 | 83.8% |
| O3 | 7 | 179 | 83.8% |

**Rust instruction reduction (O0 → O2): 83.8%**

## TML vs Rust Comparison

Direct comparison using LLVM IR instruction counts:

### optimization_bench.tml

| Metric | TML | Rust |
|--------|-----|------|
| O0 Instructions | 550 | 1103 |
| O0 Functions | 28 | 42 |
| O1 Instructions | 406 | 179 |
| O1 Functions | 27 | 7 |
| O2 Instructions | 247 | 179 |
| O2 Functions | 25 | 7 |
| O3 Instructions | 234 | 179 |
| O3 Functions | 25 | 7 |
| **Reduction (O0→O2)** | **55.1%** | **83.8%** |

## Notes

- Both TML and Rust generate LLVM IR which is then compiled by LLVM/Clang
- TML applies optimizations at the MIR level before LLVM IR generation
- Rust applies optimizations at the MIR level and then LLVM applies more
- The LLVM IR shown here is BEFORE LLVM's own optimization passes
