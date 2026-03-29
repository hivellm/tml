# Tasks: AI Library — Training Loop + Optimizers

**Status**: Planning. 0% (0/15).
**Depends on**: phase9b_ia-nn-layers

## Phase 1: Optimizers

- [ ] 1.1 `ia/optim/optimizer.tml` — Optimizer behavior (step, zero_grad)
- [ ] 1.2 `ia/optim/sgd.tml` — SGD with momentum, weight decay
- [ ] 1.3 `ia/optim/adam.tml` — Adam, AdamW (bias-corrected moments)
- [ ] 1.4 `ia/optim/clip.tml` — Gradient clipping (by norm, by value)

## Phase 2: LR Schedulers

- [ ] 2.1 `ia/optim/scheduler.tml` — CosineAnnealing
- [ ] 2.2 `ia/optim/scheduler.tml` — LinearWarmup
- [ ] 2.3 `ia/optim/scheduler.tml` — StepLR, ReduceOnPlateau
- [ ] 2.4 `ia/optim/mod.tml` — Optimizer exports

## Phase 3: Training Infrastructure

- [ ] 3.1 `ia/train/dataset.tml` — Dataset behavior (len, get)
- [ ] 3.2 `ia/train/dataloader.tml` — DataLoader (batching, shuffling, parallel loading)
- [ ] 3.3 `ia/train/trainer.tml` — Trainer (epochs, loss tracking, eval loop)
- [ ] 3.4 `ia/train/checkpoint.tml` — Save/load training checkpoints
- [ ] 3.5 `ia/train/mod.tml` — Training exports

## Phase 4: Tests

- [ ] 4.1 Tests: Adam optimizer step correctness
- [ ] 4.2 Tests: MNIST training convergence (MLP)
- [ ] 4.3 Tests: checkpoint save/load round-trip
