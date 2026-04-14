# Proposal: phase0f_fix-boolean-shortcircuit

## Why
`and`/`or` short-circuit expressions generate an excessive number of LLVM basic blocks and phi nodes. Every `a and b` compiles to three blocks (entry, eval_b, merge) even when both operands are pure comparisons with no side effects. Benchmarks show TML at 4 ns/op (236M ops/sec) vs Rust at 1 ns/op (993M ops/sec) — a 4x gap. For pure operands, `a and b` should lower to a single `and i1 %a, %b` instruction (one block, zero branches). For operands with side effects, the short-circuit should use the minimal 2-block form (entry → eval_b, skip → merge) rather than the current 3-block layout with redundant phi.

## What Changes
The codegen path for `BoolAnd`/`BoolOr` in `compiler/src/codegen/binary_ops.cpp` (or the equivalent MIR emission site) will be extended with a purity check. If both operands are side-effect-free (literals, variable reads, arithmetic, comparisons), the result is emitted as a single `and i1` / `or i1` instruction. If either operand has side effects, the short-circuit uses a 2-block layout: an entry block that conditionally branches to an `eval_b` block, with the merge phi taking the shortcut value directly from the branch condition rather than duplicating the block.

## Impact
- Affected specs: codegen/boolean-expressions
- Affected code: `compiler/src/codegen/binary_ops.cpp`, possibly `compiler/src/mir/thir_mir_builder_expr.cpp`
- Breaking change: NO
- User benefit: Up to 4x improvement in boolean-heavy code paths (filter loops, validation chains, parser predicates). All existing short-circuit semantics preserved for expressions with side effects.
