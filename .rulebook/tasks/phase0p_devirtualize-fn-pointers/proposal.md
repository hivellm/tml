# Proposal: phase0p_devirtualize-fn-pointers

## Why
Function pointer calls in TML emit an indirect `call` via a pointer load, even when the callee is statically known at the call site (e.g., a closure passed to `map` that is immediately constructed, or a behavior method called on a concrete type). Benchmarks show TML function-pointer call at 8 ns/op (112M ops/sec) vs Rust at 1 ns/op (1.1B ops/sec) — an 8x gap. Rust's monomorphizer devirtualizes known-concrete-type behavior method calls and inlines them. When the callee is always the same function, an indirect `call` can be replaced with a direct `call` (or inlined entirely), eliminating the branch target buffer pressure and enabling further inlining. See `docs/analysis/benchmark/06-functions-closures.md`.

## What Changes
Two complementary changes:
1. **Behavior method devirtualization (MIR)**: in the MIR builder, when a method call's receiver type is statically known (concrete, not a type parameter), look up the impl directly and emit a direct `CallInst` instead of a vtable indirect call.
2. **Closure devirtualization**: when a closure value is created and passed to a function in the same scope (never stored or returned), replace the function-pointer indirect call with a direct call to the closure body function. This mirrors Rust's closure inlining after monomorphization.

## Impact
- Affected specs: codegen/function-calls, codegen/closures
- Affected code: `compiler/src/mir/thir_mir_builder_expr.cpp` (method call dispatch), `compiler/src/codegen/instructions.cpp` (CallInst emission)
- Breaking change: NO
- User benefit: Up to 8x improvement for behavior-method dispatch and closure calls on concrete types. Critical for iterator chains (map/filter/fold) and parser combinators.
