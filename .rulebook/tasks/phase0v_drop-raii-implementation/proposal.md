# Proposal: phase0v_drop-raii-implementation

## Why
TML has no automatic resource cleanup — `Heap[T]`, file handles, socket descriptors, and other owned resources must be freed manually. Rust's `Drop` trait is called automatically at end-of-scope, making resource safety the default. Without RAII, TML programs accumulate leaks in every code path that returns early (error returns, panics, early `break`). The encoding leaks (phase0u) are a symptom of the deeper absence: there is no mechanism to guarantee cleanup regardless of exit path. Every long-running TML program today leaks memory. This task implements the `Drop` behavior and the compiler infrastructure to call `drop()` at scope exit, matching Rust's RAII model. See `docs/analysis/benchmark/09-memory-management.md`.

## What Changes
1. **`Drop` behavior in `core`**: `behavior Drop { func drop(self: mut Self) }` — types that implement `Drop` have their `drop` called at scope exit.
2. **Scope-exit insertion (MIR pass)**: the MIR builder tracks variables with `Drop`-implementing types. At each scope exit point (end of block, `return`, `break`, `continue`), insert a `CallInst` to `<Type>::drop(var)`.
3. **Automatic `Heap[T]` drop**: `Heap[T]` implements `Drop` by calling `mem::free(self.ptr)` — heap pointers are freed automatically when they go out of scope.
4. **`Text` and `Buffer` drop**: both implement `Drop` to free their internal buffer.
5. **Move semantics**: after a value is moved (passed to a function), the compiler does NOT insert a drop for it at the original scope exit (it was moved, not owned).

## Impact
- Affected specs: core/drop, codegen/scope-exit, memory/raii
- Affected code: `lib/core/src/drop.tml` (new), MIR scope-exit pass in `compiler/src/mir/`, `lib/core/src/heap.tml`, `lib/core/src/text.tml`
- Breaking change: POTENTIALLY (existing code that manually frees values now double-frees — must detect and warn)
- User benefit: Memory-safe resource management by default. Eliminates the entire class of leak bugs in phase0u and throughout the stdlib.
