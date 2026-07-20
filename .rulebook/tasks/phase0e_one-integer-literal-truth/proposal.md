# Proposal: phase0e_one-integer-literal-truth

## Why
Three engines default unsuffixed integer literals differently (I64 vs I32 vs
I32), producing probe-proven silent miscompilation on the MIR path and forcing
the project-wide ":I64 annotation" workaround (analysis L-001, gotcha T6).

## What Changes
One recorded default agreed by spec+checker+HIR+codegen; literals become
inference variables unified across uses with a hard error on conflicting
widths; downstream stages consume the checker's resolved type instead of
re-deriving.

## Impact
- Affected specs: docs/specs/04-TYPES.md
- Affected code: compiler/src/types/checker/expr.cpp, compiler/src/hir/hir_builder_expr.cpp, compiler/src/codegen/llvm/expr/infer*.cpp
- Breaking change: POSSIBLY (code silently relying on divergent defaults now errors or changes width — each instance was a latent bug)
- User benefit: no silent truncation; the ":I64 everywhere" tax dies; both codegen paths agree on core semantics
