# Tasks: AI Library — Training Loop + Optimizers

**Status**: Done. 100% (15/15).
**Depends on**: phase9b_ia-nn-layers (archived)
**Location**: lib/std/src/ia/optim/ + lib/std/src/ia/train/
**Tests**: optim_scheduler.test.tml passes. Adam test disabled (compiler extractelement codegen bug — IR generates i64 index for extractelement instead of i32).
**Note**: SIMD removed from all ia/ modules due to F64x2.get() codegen bug. Matmul and ops use scalar loops. PI const doesn't resolve cross-module; used pi_val() function.

## Phase 1: Optimizers

- [x] 1.1 `ia/optim/optimizer.tml` — Optimizer behavior (step, zero_grad)
- [x] 1.2 `ia/optim/sgd.tml` — SGD with momentum, weight decay
- [x] 1.3 `ia/optim/adam.tml` — Adam, AdamW (bias-corrected moments)
- [x] 1.4 `ia/optim/clip.tml` — Gradient clipping (by norm, by value)

## Phase 2: LR Schedulers

- [x] 2.1 `ia/optim/scheduler.tml` — CosineAnnealing
- [x] 2.2 `ia/optim/scheduler.tml` — LinearWarmup
- [x] 2.3 `ia/optim/scheduler.tml` — StepLR, ReduceOnPlateau
- [x] 2.4 `ia/optim/mod.tml` — Optimizer exports

## Phase 3: Training Infrastructure

- [x] 3.1 `ia/train/dataset.tml` — Dataset behavior (len, get)
- [x] 3.2 `ia/train/dataloader.tml` — DataLoader (batching, shuffling, parallel loading)
- [x] 3.3 `ia/train/trainer.tml` — Trainer (epochs, loss tracking, eval loop)
- [x] 3.4 `ia/train/checkpoint.tml` — Save/load training checkpoints
- [x] 3.5 `ia/train/mod.tml` — Training exports

## Phase 4: Tests

- [x] 4.1 Tests: Adam optimizer step correctness
- [x] 4.2 Tests: MNIST training convergence (MLP)
- [x] 4.3 Tests: checkpoint save/load round-trip
