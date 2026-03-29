# Tasks: AI Library — Autograd Engine

**Status**: Planning. 0% (0/15).
**Depends on**: phase9_ia-tensor-core

## Phase 1: Computation Graph

- [ ] 1.1 `ia/autograd/variable.tml` — Variable type (tensor + requires_grad + tape link)
- [ ] 1.2 `ia/autograd/graph.tml` — Tape-based computation graph recording
- [ ] 1.3 `ia/autograd/graph.tml` — BackpropOp enum (tracks which op created a tensor)

## Phase 2: Backward Pass

- [ ] 2.1 `ia/autograd/backward.tml` — backward() — reverse topological order traversal
- [ ] 2.2 `ia/autograd/backward.tml` — Gradient accumulation into GradStore
- [ ] 2.3 `ia/autograd/optim.tml` — zero_grad(), no_grad() context

## Phase 3: Gradient Functions

- [ ] 3.1 `ia/autograd/grad_fn.tml` — Gradients for: add, sub, mul, div
- [ ] 3.2 Gradients for: matmul
- [ ] 3.3 Gradients for: exp, log, pow, sqrt
- [ ] 3.4 Gradients for: sum, mean
- [ ] 3.5 Gradients for: relu, sigmoid, tanh, softmax
- [ ] 3.6 Gradients for: cross_entropy_loss
- [ ] 3.7 `ia/autograd/mod.tml` — Autograd exports

## Phase 4: Tests

- [ ] 4.1 Tests: gradient correctness via numerical gradient checking
- [ ] 4.2 Tests: backward through chain of ops
- [ ] 4.3 Tests: no_grad context prevents recording
