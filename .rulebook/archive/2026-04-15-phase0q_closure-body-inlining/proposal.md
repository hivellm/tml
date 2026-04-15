# Proposal: phase0q_closure-body-inlining

## Why
TML closures are compiled to heap-allocated function objects with an indirect `call` through a function pointer. When a closure is passed immediately to a higher-order function (`map`, `filter`, `fold`, `for_each`) and the closure body is simple (one expression, no captures that escape), the allocation and indirect call are unnecessary overhead. Benchmarks show TML `map` with a closure at 3-5 ns/op vs Rust at <1 ns/op — a 3-5x gap. Rust inlines the closure body into the loop after monomorphization, eliminating both the allocation and the indirect call. This task implements closure body inlining for non-escaping closures passed directly to known higher-order functions. See `docs/analysis/benchmark/06-functions-closures.md`.

## What Changes
1. **Inlining at MIR level**: when a `ClosureLiteral` is passed as an argument to a function whose parameter is `Fn[A, B]` (or `do(A) B`) and the closure does not escape (not stored in a struct, not returned, not put in a list), the MIR builder substitutes the closure body inline at the call site, replacing the `call fn_ptr` with the body instructions.
2. **`@inline` attribute on HOF parameters**: standard library functions (`List.map`, `List.filter`, etc.) will be annotated to mark their closure parameter as inlineable — similar to Rust's `#[inline]` on `Iterator::map`.
3. For closures with captured variables: captures are stack-allocated if all captured values are live at the call site (no heap allocation needed for trivial captures).

## Impact
- Affected specs: codegen/closures, std/collections/list
- Affected code: `compiler/src/mir/thir_mir_builder_expr.cpp` (closure inlining), `lib/std/src/collections/list.tml` (inline annotation)
- Breaking change: NO
- User benefit: 3-5x improvement for iterator-style code (`map`, `filter`, `fold`). Critical for functional-style TML programs.
