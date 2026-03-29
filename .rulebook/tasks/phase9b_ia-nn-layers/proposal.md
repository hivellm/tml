# Proposal: IA Neural Network Layers

## Why
Pre-built neural network layers (Linear, Conv, Attention, Normalization) enable building models without manual tensor ops. Module behavior provides train/eval modes and parameter tracking.

## What Changes
- Module behavior (forward, parameters, train/eval, to_device)
- Layers: Linear, Conv1d, Conv2d, Embedding, Dropout
- Normalization: LayerNorm, RMSNorm, BatchNorm
- Activation: ReLU, GELU, SiLU, Sigmoid, Tanh, Softmax
- Attention: MultiHeadAttention with GQA support
- Loss: CrossEntropy, MSE, L1, BCE
- Sequential container

## Impact
- Affected code: lib/ia/src/nn/ (new)
- Breaking change: NO
- User benefit: Build neural networks with high-level API
