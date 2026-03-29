# Tasks: AI Library — Tensor Core (CPU)

**Status**: Planning. 0% (0/25).
**Reference**: docs/analyses/ia/00-strategic-plan.md
**Location**: lib/ia/ (separate library)

## Phase 1: Library Scaffold

- [ ] 1.1 Create `lib/ia/` directory (src/, tests/, README.md, CHANGELOG.md)
- [ ] 1.2 `lib/ia/src/mod.tml` — root module exports
- [ ] 1.3 `lib/ia/src/error.tml` — IaError enum, IaResult[T]

## Phase 2: DType & Shape

- [ ] 2.1 `ia/tensor/dtype.tml` — DType enum (F32, F64, I32, I64, Bool, U8, U32)
- [ ] 2.2 `ia/tensor/shape.tml` — Shape type (List[I64] wrapper with rank, numel, strides)
- [ ] 2.3 `ia/tensor/shape.tml` — Layout { shape, stride, start_offset }
- [ ] 2.4 `ia/tensor/shape.tml` — broadcast_shape(a, b) for broadcasting rules

## Phase 3: Tensor Type

- [ ] 3.1 `ia/tensor/tensor.tml` — Tensor { storage: Buffer, layout, dtype }
- [ ] 3.2 `ia/tensor/init.tml` — zeros, ones, full, from_list, arange, linspace
- [ ] 3.3 `ia/tensor/init.tml` — rand, randn (random initialization)
- [ ] 3.4 `ia/tensor/ops.tml` — Element-wise: add, sub, mul, div, neg, abs
- [ ] 3.5 `ia/tensor/ops.tml` — Math: exp, log, sqrt, pow, clamp
- [ ] 3.6 `ia/tensor/matmul.tml` — CPU matrix multiply (naive + blocked)
- [ ] 3.7 `ia/tensor/reduce.tml` — sum, mean, max, min, argmax, argmin (with dim)
- [ ] 3.8 `ia/tensor/reshape.tml` — reshape, transpose, permute, contiguous
- [ ] 3.9 `ia/tensor/reshape.tml` — squeeze, unsqueeze, view, expand
- [ ] 3.10 `ia/tensor/index.tml` — indexing, slicing, gather, scatter
- [ ] 3.11 `ia/tensor/compare.tml` — eq, gt, lt, ne, where_cond
- [ ] 3.12 `ia/tensor/mod.tml` — Tensor module exports

## Phase 4: Tests

- [ ] 4.1 Tests: creation (zeros, ones, from_list, rand)
- [ ] 4.2 Tests: element-wise ops (add, sub, mul, div)
- [ ] 4.3 Tests: matmul correctness (small matrices)
- [ ] 4.4 Tests: reductions (sum, mean, argmax)
- [ ] 4.5 Tests: reshape, transpose, slicing
- [ ] 4.6 Tests: broadcasting rules
