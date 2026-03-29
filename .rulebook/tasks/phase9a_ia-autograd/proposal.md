# Proposal: IA Autograd Engine

## Why
Automatic differentiation is required for training neural networks. Tape-based recording of operations enables backward pass gradient computation.

## What Changes
- Variable type (tensor + requires_grad + tape link)
- Computation graph recording (tape-based)
- backward() — reverse topological order traversal
- Gradient functions for all ops: arithmetic, matmul, activations, loss
- zero_grad(), no_grad() context

## Impact
- Affected code: lib/ia/src/autograd/ (new)
- Breaking change: NO
- User benefit: Train neural networks with automatic gradient computation
