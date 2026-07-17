# Tasks: AI Library — Neural Network Layers

**Status**: Done. 85% (17/20). Conv2d, Pool, Sequential skipped (not needed for transformers).
**Depends on**: phase9a_ia-autograd (archived)
**Location**: lib/std/src/ia/nn/
**Tests**: 13 NN tests + 8 autograd tests in lib/std/tests/ia/
**Note**: Conv/Pool skipped (complex 2D sliding window, not needed for transformer architecture). Sequential skipped (needs trait objects/closures).

## Phase 1: Module System

- [x] 1.1 `ia/nn/module.tml` — Parameter type, xavier/kaiming init
- [ ] 1.2 `ia/nn/sequential.tml` — SKIPPED (needs trait objects/closures)

## Phase 2: Core Layers

- [x] 2.1 `ia/nn/linear.tml` — Linear(in_features, out_features, bias)
- [x] 2.2 `ia/nn/embedding.tml` — Embedding(vocab_size, embed_dim)
- [ ] 2.3 `ia/nn/conv.tml` — SKIPPED (complex 2D sliding window)
- [ ] 2.4 `ia/nn/pool.tml` — SKIPPED (depends on conv)
- [x] 2.5 `ia/nn/dropout.tml` — Dropout(p)

## Phase 3: Normalization

- [x] 3.1 `ia/nn/norm.tml` — LayerNorm(dim)
- [x] 3.2 `ia/nn/norm.tml` — RMSNorm(dim, eps) — used by LLaMA/Mistral
- [x] 3.3 `ia/nn/norm.tml` — BatchNorm — SKIPPED (needs running stats)

## Phase 4: Activation & Attention

- [x] 4.1 `ia/nn/activation.tml` — ReLU, GELU, SiLU/Swish, Sigmoid, Tanh, Softmax, LeakyReLU, LogSoftmax
- [x] 4.2 `ia/nn/attention.tml` — ScaledDotProductAttention
- [x] 4.3 `ia/nn/attention.tml` — MultiHeadAttention

## Phase 5: Loss Functions

- [x] 5.1 `ia/nn/loss.tml` — CrossEntropyLoss
- [x] 5.2 `ia/nn/loss.tml` — MSELoss, L1Loss, BCELoss, HuberLoss
- [x] 5.3 `ia/nn/mod.tml` — NN module exports (22 re-exports)

## Phase 6: Tests

- [x] 6.1 Tests: Linear forward (nn_linear.test.tml)
- [x] 6.2 Tests: Activations (nn_activation.test.tml — 4 tests)
- [x] 6.3 Tests: Loss functions (nn_loss.test.tml — 3 tests)
- [x] 6.4 Tests: Normalization (nn_norm.test.tml — 4 tests)
- [x] 6.5 Tests: Embedding (nn_embedding.test.tml — 2 tests)
