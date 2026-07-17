# Tasks: AI Library — Tensor Core (CPU)

**Status**: Done. 100% (25/25).
**Reference**: docs/analyses/ia/00-strategic-plan.md
**Location**: lib/std/src/ia/ (under std library — compiler only resolves core/std/test prefixes)
**Tests**: 23 tests across 6 files in lib/std/tests/ia/
**Note**: Closure codegen bug required rewriting ops.tml and compare.tml to avoid lambdas (explicit loops instead)

## Phase 1: Library Scaffold

- [x] 1.1 Create `lib/ia/` directory (src/, tests/, README.md, CHANGELOG.md)
- [x] 1.2 `lib/ia/src/mod.tml` — root module exports
- [x] 1.3 `lib/ia/src/error.tml` — IaError enum, IaResult[T]

## Phase 2: DType & Shape

- [x] 2.1 `ia/tensor/dtype.tml` — DType enum (F32, F64, I32, I64, Bool, U8, U32)
- [x] 2.2 `ia/tensor/shape.tml` — Shape type (List[I64] wrapper with rank, numel, strides)
- [x] 2.3 `ia/tensor/shape.tml` — Layout { shape, stride, start_offset }
- [x] 2.4 `ia/tensor/shape.tml` — broadcast_shape(a, b) for broadcasting rules

## Phase 3: Tensor Type

- [x] 3.1 `ia/tensor/tensor.tml` — Tensor { storage: Buffer, layout, dtype }
- [x] 3.2 `ia/tensor/init.tml` — zeros, ones, full, from_list, arange, linspace
- [x] 3.3 `ia/tensor/init.tml` — rand, randn (random initialization)
- [x] 3.4 `ia/tensor/ops.tml` — Element-wise: add, sub, mul, div, neg, abs
- [x] 3.5 `ia/tensor/ops.tml` — Math: exp, log, sqrt, pow, clamp
- [x] 3.6 `ia/tensor/matmul.tml` — CPU matrix multiply (naive + blocked)
- [x] 3.7 `ia/tensor/reduce.tml` — sum, mean, max, min, argmax, argmin (with dim)
- [x] 3.8 `ia/tensor/reshape.tml` — reshape, transpose, permute, contiguous
- [x] 3.9 `ia/tensor/reshape.tml` — squeeze, unsqueeze, view, expand
- [x] 3.10 `ia/tensor/index.tml` — indexing, slicing, gather, scatter
- [x] 3.11 `ia/tensor/compare.tml` — eq, gt, lt, ne, where_cond
- [x] 3.12 `ia/tensor/mod.tml` — Tensor module exports

## Phase 4: Tests

- [x] 4.1 Tests: creation (zeros, ones, from_list, rand)
- [x] 4.2 Tests: element-wise ops (add, sub, mul, div)
- [x] 4.3 Tests: matmul correctness (small matrices)
- [x] 4.4 Tests: reductions (sum, mean, argmax)
- [x] 4.5 Tests: reshape, transpose, slicing
- [x] 4.6 Tests: broadcasting rules
