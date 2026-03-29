# Proposal: IA Tensor Core (CPU)

## Why
Foundation for the entire AI library. Tensor type with shape/dtype/storage, element-wise ops, matmul, reductions, reshaping. Everything else builds on this.

## What Changes
- Create lib/ia/ (separate library: src/, tests/, README.md, CHANGELOG.md)
- IaError enum and IaResult[T]
- DType enum, Shape/Layout types with broadcasting
- Tensor type with CPU storage (Buffer-backed)
- Creation: zeros, ones, rand, randn, arange, from_list
- Ops: add, sub, mul, div, exp, log, sqrt, matmul, sum, mean, argmax
- Reshape, transpose, permute, slice, gather, scatter

## Impact
- Affected code: lib/ia/ (new separate library)
- Breaking change: NO
- User benefit: NumPy-like tensor operations in TML
