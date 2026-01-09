# TML Optimization Benchmark Report

Generated: 2026-01-08 18:08:05

## Summary

This report shows the effectiveness of TML's optimization passes at each level:

| Level | Description |
|-------|-------------|
| O0 | No optimizations |
| O1 | Basic optimizations (constant folding, propagation) |
| O2 | Standard optimizations (O1 + CSE, copy prop, DCE) |
| O3 | Aggressive optimizations (O2 + second pass) |

## HIR Optimizations

- Constant folding (integers, floats, booleans)
- Dead code elimination (constant if conditions)
- Short-circuit evaluation optimization

## MIR Optimizations

- Constant folding
- Constant propagation
- Copy propagation
- Common subexpression elimination (CSE)
- Dead code elimination
- Unreachable code elimination

## TML Results

### optimization_bench.tml

| Opt Level | Functions | Blocks | Instructions | Reduction |
|-----------|-----------|--------|--------------|-----------|
| O0 | 27 | 85 | 650 | - |
| O1 | 27 | 85 | 650 | 0.0% |
| O2 | 8 | 109 | 538 | 17.2% |
| O3 | 2 | 129 | 564 | 13.2% |

**Total instruction reduction (O0 → O2): 17.2%**

### algorithms.tml

| Opt Level | Functions | Blocks | Instructions | Reduction |
|-----------|-----------|--------|--------------|-----------|
| O0 | 21 | 166 | 666 | - |
| O1 | 21 | 166 | 666 | 0.0% |
| O2 | 21 | 678 | 1597 | -139.8% |
| O3 | 21 | 3062 | 6531 | -880.6% |

**Total instruction reduction (O0 → O2): -139.8%**

## Rust Comparison

Equivalent Rust code compiled with rustc for comparison:

### Rust: optimization_bench.rs (equivalent to optimization_bench.tml)

| Opt Level | Functions | Blocks | LLVM IR Instrs | Binary Size | Reduction |
|-----------|-----------|--------|----------------|-------------|-----------|
| O0 | 42 | 42 | 1008 | 143KB | - |
| O1 | 7 | 7 | 179 | 132KB | 82.2% |
| O2 | 7 | 7 | 179 | 132KB | 82.2% |
| O3 | 7 | 7 | 179 | 132KB | 82.2% |

**Rust instruction reduction (O0 → O2): 82.2%**

## TML vs Rust Comparison

### optimization_bench.tml

| Metric | TML | Rust |
|--------|-----|------|
| Optimization Reduction (O0→O2) | 17.2% | 82.2% |
| O2 Instructions (MIR/LLVM IR) | 538 | 179 |

## Optimization Categories Tested

### 1. Constant Folding
- Integer arithmetic: `10 + 20 + 30` → `60`
- Float arithmetic: `1.5 + 2.5 + 3.0` → `7.0`
- Boolean logic: `true and true and true` → `true`
- Bitwise: `0xFF & 0x0F` → `15`

### 2. Dead Code Elimination
- Unused variables removed
- Unreachable code after `return` removed
- Dead branches in constant conditions removed

### 3. Common Subexpression Elimination
- `x + y` computed once when used multiple times

### 4. Copy Propagation
- `let a = x; let b = a; use b` → `use x`

### 5. HIR-specific Optimizations
- `if true { a } else { b }` → `a`
- `false and expensive()` → `false`
- `true or expensive()` → `true`
