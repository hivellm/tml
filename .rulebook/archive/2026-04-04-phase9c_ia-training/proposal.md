# Proposal: IA Training Loop + Optimizers

## Why
Complete training infrastructure: optimizers (Adam, SGD), LR schedulers, DataLoader, training loop, and checkpoint management.

## What Changes
- Optimizer behavior + SGD (momentum), Adam, AdamW
- LR schedulers: CosineAnnealing, LinearWarmup, StepLR
- Gradient clipping (by norm, by value)
- Dataset behavior, DataLoader (batching, shuffling, parallel)
- Trainer (epochs, eval, logging), checkpoint save/load

## Impact
- Affected code: lib/ia/src/optim/ (new), lib/ia/src/train/ (new)
- Breaking change: NO
- User benefit: Train models end-to-end in TML
