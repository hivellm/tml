# TML Optimization Benchmark Report

Generated: 2026-01-08 17:50:23

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

## Results

### optimization_bench.tml

| Opt Level | Functions | Blocks | Instructions | Reduction |
|-----------|-----------|--------|--------------|-----------|
| O0 | 0 | 0 | 0 | - |
| O1 | 0 | 0 | 0 | 0.0% |
| O2 | 0 | 0 | 0 | 0.0% |
| O3 | 0 | 0 | 0 | 0.0% |

**Total instruction reduction (O0 → O2): 0.0%**

### algorithms.tml

| Opt Level | Functions | Blocks | Instructions | Reduction |
|-----------|-----------|--------|--------------|-----------|
| O0 | 0 | 0 | 0 | - |
| O1 | 0 | 0 | 0 | 0.0% |
| O2 | 0 | 0 | 0 | 0.0% |
| O3 | 0 | 0 | 0 | 0.0% |

**Total instruction reduction (O0 → O2): 0.0%**

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
