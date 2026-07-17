# Tasks: AI Library — Autograd Engine

**Status**: Done. 100% (15/15).
**Depends on**: phase9_ia-tensor-core (archived)
**Location**: lib/std/src/ia/autograd/
**Tests**: 8 tests in lib/std/tests/ia/autograd_backward.test.tml
**Note**: Flat tape encoding (tag+operands) avoids enum-in-List codegen issues. get_output returns by value (ref of List::get() codegen bug).

## Phase 1: Computation Graph

- [x] 1.1 `ia/autograd/variable.tml` — Variable type (tensor + requires_grad + tape link)
- [x] 1.2 `ia/autograd/graph.tml` — Tape-based computation graph recording
- [x] 1.3 `ia/autograd/graph.tml` — BackpropOp enum (tracks which op created a tensor)

## Phase 2: Backward Pass

- [x] 2.1 `ia/autograd/backward.tml` — backward() — reverse topological order traversal
- [x] 2.2 `ia/autograd/backward.tml` — Gradient accumulation into GradStore
- [x] 2.3 `ia/autograd/optim.tml` — zero_grad(), no_grad() context

## Phase 3: Gradient Functions

- [x] 3.1 `ia/autograd/grad_fn.tml` — Gradients for: add, sub, mul, div
- [x] 3.2 Gradients for: matmul
- [x] 3.3 Gradients for: exp, log, pow, sqrt
- [x] 3.4 Gradients for: sum, mean
- [x] 3.5 Gradients for: relu, sigmoid, tanh, softmax
- [x] 3.6 Gradients for: cross_entropy_loss
- [x] 3.7 `ia/autograd/mod.tml` — Autograd exports

## Phase 4: Tests

- [x] 4.1 Tests: gradient correctness via numerical gradient checking
- [x] 4.2 Tests: backward through chain of ops
- [x] 4.3 Tests: no_grad context prevents recording
