# Proposal: phase30g_closure-type-inference

## Why
Closures passed to higher-order functions require full type annotations even when the expected signature is known from context: `list.filter(do(x: I64) -> Bool { return x > 0 })` could be `list.filter(do(x) { x > 0 })`. The PEG grammar already allows optional types in closure params (`ClosureParam = "mut"? ~ Ident ~ (":" ~ Type)?`). This is a type-checker change only. Also adds implicit return for single-expression closure bodies.

Note: `_` placeholder syntax was REJECTED (ambiguous with wildcard pattern, breaks LL(1)).

Source: docs/analyses/language/07-closure-shorthand.md

## What Changes
1. **Type checker**: When a closure is passed to a function with a known `func(T) -> U` parameter type, propagate T into the closure's parameter types (bidirectional type inference)
2. **Implicit return**: If a closure body is a single expression (not a block with statements), treat it as the return value

## Impact
- Affected specs: PEG grammar (updated ClosureParam to allow optional type)
- Affected code: `compiler/src/typechecker/`, `compiler/src/mir/thir_mir_builder_expr.cpp`
- Breaking change: NO (existing annotated closures still work)
- User benefit: More concise functional-style code with iterators
