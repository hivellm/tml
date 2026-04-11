# Proposal: phase30d_operator-overloading

## Why
Numeric user types (BigInt, Vec2, Matrix) must use `.add()`, `.sub()` method calls instead of `+`, `-` operators. This makes mathematical code unreadable. 20+ method-based operations in stdlib could use operators. No grammar change needed — operators already parse; this is purely a type-checker + codegen change that resolves `+` on non-primitive types to a behavior method call.

Source: docs/analyses/language/04-operator-overload.md

## What Changes
1. **Core behaviors**: Define `Add`, `Sub`, `Mul`, `Div`, `Rem`, `Neg`, `Not`, `Index` behaviors in `lib/core/src/ops/`
2. **Type checker**: When a binary/unary op is applied to a non-primitive type, resolve it to the corresponding behavior method (e.g., `a + b` → `Add::add(a, b)`)
3. **Codegen**: Emit method call for operator on non-primitive types; keep built-in LLVM ops for primitives (I32, I64, F64 etc.)
4. **Index syntax**: `a[i]` on non-primitive types resolves to `Index::index(a, i)`

## Impact
- Affected specs: 03-GRAMMAR.md (added §3.3.1 Operator Behaviors table)
- Affected code: `compiler/src/typechecker/`, `compiler/src/mir/thir_mir_builder_expr.cpp`
- Breaking change: NO (additive — existing method calls still work)
- User benefit: Natural math syntax for BigInt, vectors, matrices; `[]` indexing for custom collections
