# Machine Learning with TML

This chapter covers TML's machine learning library (`std::ia`), which provides everything needed to build and train neural networks — from tensor operations to automatic differentiation to optimizers.

## Overview

The `std::ia` package includes:
- **Tensors** — Multi-dimensional arrays with NumPy-style broadcasting
- **Automatic Differentiation** — Tape-based reverse-mode autograd
- **Neural Network Layers** — Linear, embedding, attention, normalization
- **Activation & Loss Functions** — ReLU, sigmoid, softmax, MSE, cross-entropy
- **Optimizers** — SGD and Adam with gradient clipping and scheduling
- **Training Infrastructure** — DataLoader, checkpoints, loss tracking

## Tensors — The Foundation

A tensor is a multi-dimensional array of numbers. Think of it as:
- 1-D tensor: a vector `[1, 2, 3]`
- 2-D tensor: a matrix `[[1, 2], [3, 4]]`
- 3-D tensor: a volume `[[[1, 2], [3, 4]], [[5, 6], [7, 8]]]`

### Creating Tensors

```tml
use std::ia::tensor::*

func main() {
    // From literal lists
    let x = from_list_f64(List[F64] { 1.0, 2.0, 3.0 }, List[I64] { 3 })!

    // Pre-filled
    let zeros = zeros(List[I64] { 3, 4 })           // 3x4 matrix of zeros
    let ones = ones(List[I64] { 2, 3, 4 })          // 2x3x4 tensor of ones
    let full = full(List[I64] { 5 }, 3.14)          // vector of 3.14 repeated

    // Random
    let uniform = rand(List[I64] { 10 })            // uniform [0, 1)
    let normal = randn(List[I64] { 10 })            // standard normal

    // Ranges
    let range = arange(0.0, 10.0, 1.0)              // [0, 1, 2, ..., 9]
    let space = linspace(0.0, 1.0, 10)              // 10 evenly spaced points
}
```

### Tensor Operations

All operations support NumPy-style broadcasting:

```tml
use std::ia::tensor::*

func main() {
    let a = rand(List[I64] { 3, 4 })
    let b = rand(List[I64] { 3, 4 })
    let scalar = randn(List[I64] { 1 })

    // Element-wise arithmetic
    let sum = add(ref a, ref b)
    let product = mul(ref a, ref scalar)  // broadcasts scalar
    let divided = div(ref a, ref b)

    // Activations
    let relu_output = relu(ref a)
    let sigmoid_output = sigmoid(ref a)

    // Matrix multiplication
    let mat_product = matmul(ref a, ref b)

    // Reductions
    let total = sum(ref a)                 // sum all elements
    let average = mean(ref a)              // mean across all dims
    let col_sums = sum_dim(ref a, 0)       // sum along dimension 0

    // Reshaping
    let flattened = reshape(ref a, List[I64] { 12 })
    let transposed = transpose(ref a)
}
```

## Automatic Differentiation

Automatic differentiation lets the network learn by computing gradients automatically.

### Variables and Gradients

A `Variable` wraps a tensor and tracks its computation for differentiation:

```tml
use std::ia::autograd::*
use std::ia::tensor::*

func main() {
    // Create a tensor
    let x_data = randn(List[I64] { 3, 4 })

    // Wrap in a variable that tracks gradients
    var x = Variable::new(x_data)
    x.enable_grad()

    // Compute something
    var y = Variable::new(mul(ref x.tensor, ref x.tensor))
    y.enable_grad()

    // Backward pass computes gradients
    backward(mut y)

    // Now x has computed gradients for the chain x -> y
}
```

### Training Loop Pattern

```tml
use std::ia::autograd::*
use std::ia::tensor::*
use std::ia::nn::*
use std::ia::optim::*

func train_step(
    x: ref Tensor,
    y_true: ref Tensor,
    model: ref Linear,
    optimizer: mut SGD
) -> F64 {
    // Forward pass
    var y_pred = Variable::new(model.forward(x))
    y_pred.enable_grad()

    // Compute loss
    let loss = mse_loss(ref y_pred.tensor, y_true)

    // Backward pass
    var loss_var = Variable::new(loss)
    loss_var.enable_grad()
    backward(mut loss_var)

    // Update weights
    optimizer.step()

    // Return loss for monitoring
    return sum(ref loss)
}
```

## Neural Network Layers

### Linear Layer (Fully Connected)

Maps an input vector to an output vector via a learned weight matrix and bias:

```tml
use std::ia::nn::Linear

func main() {
    // Input dimension 784 (28x28 image), output dimension 10 (10 classes)
    let layer = Linear::new(784, 10)

    let input = randn(List[I64] { 32, 784 })  // batch of 32
    let output = layer.forward(ref input)      // output shape: [32, 10]
}
```

### Activation Functions

Introduce non-linearity so networks can learn complex patterns:

```tml
use std::ia::nn::*
use std::ia::tensor::*

func main() {
    let x = randn(List[I64] { 5 })

    let relu_out = relu(ref x)                    // max(0, x)
    let sigmoid_out = sigmoid(ref x)              // 1 / (1 + exp(-x))
    let tanh_out = tanh_act(ref x)                // tanh(x)
    let gelu_out = gelu(ref x)                    // GELU activation
    let softmax_out = softmax(ref x, 0)           // softmax along dim 0
}
```

### Embedding Layer

Maps discrete indices (word IDs, token IDs) to continuous vectors:

```tml
use std::ia::nn::Embedding

func main() {
    // Vocabulary of 10,000 words, each embedding is 128-dimensional
    let embedding = Embedding::new(10000, 128)

    let word_indices = from_list_i64(List[I64] { 5, 42, 123 }, List[I64] { 3 })!
    let embeddings = embedding.forward(ref word_indices)
    // output shape: [3, 128] — one 128-D embedding per word
}
```

### Dropout

Randomly zeroes elements during training to prevent overfitting:

```tml
use std::ia::nn::Dropout

func main() {
    let dropout = Dropout::new(0.5)  // drop 50% during training

    let x = randn(List[I64] { 32, 100 })

    // During training
    let train_output = dropout.forward(ref x, true)    // applies dropout

    // During inference
    let eval_output = dropout.forward(ref x, false)    // no dropout
}
```

### Normalization

Stabilizes training by normalizing hidden activations:

```tml
use std::ia::nn::{LayerNorm, RMSNorm}

func main() {
    // Layer normalization
    let layer_norm = LayerNorm::new(ref List[I64] { 512 })
    let x = randn(List[I64] { 32, 512 })
    let norm_out = layer_norm.forward(ref x)

    // RMS normalization (lighter weight)
    let rms_norm = RMSNorm::new(512, 1e-5)
    let rms_out = rms_norm.forward(ref x)
}
```

## Loss Functions

Measure how wrong predictions are:

```tml
use std::ia::nn::*
use std::ia::tensor::*

func main() {
    let predictions = randn(List[I64] { 32, 10 })
    let targets = randn(List[I64] { 32, 10 })

    // Regression: measure mean squared error
    let mse = mse_loss(ref predictions, ref targets)

    // Classification: cross-entropy
    let ce = cross_entropy_loss(ref predictions, ref targets)

    // L1 loss: sum of absolute differences
    let l1 = l1_loss(ref predictions, ref targets)

    // Binary cross-entropy
    let bce = bce_loss(ref predictions, ref targets)
}
```

## Optimizers

Learn weights by following gradient directions:

### SGD — Stochastic Gradient Descent

```tml
use std::ia::optim::SGD
use std::ia::nn::Linear

func main() {
    let layer = Linear::new(10, 5)

    // Get references to parameters to optimize
    var params = List[ref Tensor]::new()
    params.push(ref layer.weight)
    // params.push(ref layer.bias) if it exists

    // Create optimizer
    var sgd = SGD::new(ref params, 0.01)  // learning rate 0.01
        .with_momentum(0.9)                 // add momentum

    // In training loop:
    // 1. Compute loss and gradients
    // 2. sgd.step()  — updates weights
}
```

### Adam — Adaptive Moment Estimation

More sophisticated: adapts learning rates per parameter:

```tml
use std::ia::optim::Adam

func main() {
    var params = List[ref Tensor]::new()
    // ... add parameters ...

    var adam = Adam::new(ref params, 0.001)  // learning rate 0.001
        .with_weight_decay(0.0001)            // L2 regularization

    // In training loop:
    // adam.step()
}
```

### Gradient Clipping

Prevent exploding gradients:

```tml
use std::ia::optim::{clip_grad_value, clip_grad_norm}

func main() {
    var gradients = List[Tensor]::new()
    // ... compute gradients ...

    // Clip gradients to [-1.0, 1.0]
    clip_grad_value(mut gradients, 1.0)

    // Or clip by global norm
    clip_grad_norm(mut gradients, 1.0)
}
```

### Learning Rate Scheduling

Adjust learning rate during training:

```tml
use std::ia::optim::{CosineAnnealing, LinearWarmup, StepLR}

func main() {
    let scheduler = StepLR {
        optimizer: ref optimizer,
        step_size: 10,      // reduce LR every 10 epochs
        gamma: 0.1,         // multiply LR by 0.1
    }

    // In training loop, update scheduler each epoch
}
```

## Training Loop

Putting it all together:

```tml
use std::ia::tensor::*
use std::ia::autograd::*
use std::ia::nn::*
use std::ia::optim::*
use std::ia::train::*

@entity("training_data")
type Sample {
    @primary_column
    id: I64,
    @column
    x: Buffer,
    @column
    y: Buffer,
}

func main() {
    // Create model
    let layer1 = Linear::new(784, 128)
    let layer2 = Linear::new(128, 10)

    // Create optimizer
    var params = List[ref Tensor]::new()
    params.push(ref layer1.weight)
    params.push(ref layer2.weight)
    var optimizer = Adam::new(ref params, 0.001)

    // Create dataset
    var dataset = Dataset[Tensor, Tensor]::new()
    // ... load data ...

    // Create dataloader
    let mut loader = DataLoader[Tensor, Tensor]::new(ref dataset, 32)
        .with_shuffle()

    // Training loop
    for epoch in 1..100 {
        loader.reset()
        var total_loss = 0.0
        var batch_count = 0

        when loader.next_batch() {
            Just(batch) => {
                for (x, y) in batch {
                    // Forward pass
                    var h = Variable::new(layer1.forward(ref x))
                    h.enable_grad()

                    h.tensor = relu(ref h.tensor)

                    var output = Variable::new(layer2.forward(ref h.tensor))
                    output.enable_grad()

                    // Loss
                    let loss = cross_entropy_loss(ref output.tensor, ref y)

                    // Backward
                    var loss_var = Variable::new(loss)
                    loss_var.enable_grad()
                    backward(mut loss_var)

                    // Update
                    optimizer.step()

                    total_loss = total_loss + sum(ref loss)
                    batch_count = batch_count + 1
                }
            },
            Nothing => {},
        }

        let avg_loss = total_loss / (batch_count as F64)
        println(`Epoch {epoch}: loss = {avg_loss}`)
    }
}
```

## Best Practices

### 1. Normalize Inputs

Improves training stability:

```tml
// Normalize to mean 0, stddev 1
let mean = mean(ref x)
let normalized = sub(ref x, ref ones_like * mean)
```

### 2. Use Appropriate Activations

- **ReLU** — Most hidden layers (fast, prevents vanishing gradients)
- **Sigmoid** — Output layer for binary classification
- **Softmax** — Output layer for multi-class classification
- **Tanh** — Sometimes in RNNs (has better gradient properties than sigmoid)

### 3. Monitor Training

Check if loss is decreasing and not diverging:

```tml
if epoch % 10 == 0 {
    println(`Epoch {epoch}: loss = {avg_loss}`)
}
```

### 4. Use Checkpoints

Save best model weights:

```tml
use std::ia::train::CheckpointStore

var checkpoints = CheckpointStore::new()

if avg_loss < best_loss {
    best_loss = avg_loss
    let checkpoint = Checkpoint {
        epoch: epoch,
        loss: avg_loss,
        weights: List[Tensor] { /* save weights */ },
    }
    checkpoints.save_best(checkpoint)
}
```

### 5. Batch Normalization

For deeper networks, use batch normalization between layers:

```tml
let hidden = layer1.forward(ref x)
let batch_norm = LayerNorm::new(ref List[I64] { 128 })
let normalized = batch_norm.forward(ref hidden)
let activated = relu(ref normalized)
```

## See Also

- [AI/ML Package Reference](../packages/44-IA.md) — Complete API documentation
- [std::tensor](../packages/44-IA.md#tensor-core--multidimensional-arrays) — Tensor operations
- [std::autograd](../packages/44-IA.md#automatic-differentiation--autograd) — Gradient computation
