# TML Optimization Benchmark Report

Generated: 2026-01-08 18:23:08

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
| O1 | 27 | 529 | 3.8% |
| O2 | 8 | 542 | 1.5% |
| O3 | 2 | 570 | -3.6% |

**Total instruction reduction (O0 → O2): 1.5%**

### algorithms.tml

| Opt Level | Functions | Instructions | Reduction |
|-----------|-----------|--------------|-----------|
| O0 | 21 | 525 | - |
| O1 | 21 | 449 | 14.5% |
| O2 | 21 | 1597 | -204.2% |
| O3 | 21 | 6531 | -1144.0% |

**Total instruction reduction (O0 → O2): -204.2%**

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
| O1 Instructions | 529 | 179 |
| O1 Functions | 27 | 7 |
| O2 Instructions | 542 | 179 |
| O2 Functions | 8 | 7 |
| O3 Instructions | 570 | 179 |
| O3 Functions | 2 | 7 |
| **Reduction (O0→O2)** | **1.5%** | **83.8%** |

## Notes

- Both TML and Rust generate LLVM IR which is then compiled by LLVM/Clang
- TML applies optimizations at the MIR level before LLVM IR generation
- Rust applies optimizations at the MIR level and then LLVM applies more
- The LLVM IR shown here is BEFORE LLVM's own optimization passes
