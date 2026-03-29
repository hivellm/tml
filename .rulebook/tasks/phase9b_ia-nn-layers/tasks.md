# Tasks: AI Library — Neural Network Layers

**Status**: Planning. 0% (0/20).
**Depends on**: phase9a_ia-autograd

## Phase 1: Module System

- [ ] 1.1 `ia/nn/module.tml` — Module behavior (forward, parameters, train/eval, to_device)
- [ ] 1.2 `ia/nn/sequential.tml` — Sequential container (chain layers)

## Phase 2: Core Layers

- [ ] 2.1 `ia/nn/linear.tml` — Linear(in_features, out_features, bias)
- [ ] 2.2 `ia/nn/embedding.tml` — Embedding(vocab_size, embed_dim)
- [ ] 2.3 `ia/nn/conv.tml` — Conv1d, Conv2d
- [ ] 2.4 `ia/nn/pool.tml` — MaxPool2d, AvgPool2d
- [ ] 2.5 `ia/nn/dropout.tml` — Dropout(p)

## Phase 3: Normalization

- [ ] 3.1 `ia/nn/norm.tml` — LayerNorm(dim)
- [ ] 3.2 `ia/nn/norm.tml` — RMSNorm(dim, eps) — used by LLaMA/Mistral
- [ ] 3.3 `ia/nn/norm.tml` — BatchNorm(num_features)

## Phase 4: Activation & Attention

- [ ] 4.1 `ia/nn/activation.tml` — ReLU, GELU, SiLU/Swish, Sigmoid, Tanh, Softmax
- [ ] 4.2 `ia/nn/attention.tml` — ScaledDotProductAttention
- [ ] 4.3 `ia/nn/attention.tml` — MultiHeadAttention (with GQA support)

## Phase 5: Loss Functions

- [ ] 5.1 `ia/nn/loss.tml` — CrossEntropyLoss
- [ ] 5.2 `ia/nn/loss.tml` — MSELoss, L1Loss, BCELoss
- [ ] 5.3 `ia/nn/mod.tml` — NN module exports

## Phase 6: Tests

- [ ] 6.1 Tests: Linear forward + backward
- [ ] 6.2 Tests: Attention correctness
- [ ] 6.3 Tests: Loss function gradients
