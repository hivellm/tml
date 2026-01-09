# TML Optimization Benchmark Report

Generated: 2026-01-08 18:36:21

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
| O2 | 25 | 356 | 35.3% |
| O3 | 25 | 343 | 37.6% |

**Total instruction reduction (O0 → O2): 35.3%**

### algorithms.tml

| Opt Level | Functions | Instructions | Reduction |
|-----------|-----------|--------------|-----------|
| O0 | 21 | 525 | - |
| O1 | 21 | 449 | 14.5% |
| O2 | 21 | 407 | 22.5% |
| O3 | 21 | 407 | 22.5% |

**Total instruction reduction (O0 → O2): 22.5%**

## Notes

- Both TML and Rust generate LLVM IR which is then compiled by LLVM/Clang
- TML applies optimizations at the MIR level before LLVM IR generation
- Rust applies optimizations at the MIR level and then LLVM applies more
- The LLVM IR shown here is BEFORE LLVM's own optimization passes
