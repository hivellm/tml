# Proposal: phase0e_inline-list-push-pop-get

## Why
`List.push()`, `List.pop()`, `List.get()`, and `List.set()` are not inlined at call sites. Each access pays a full function-call overhead: save/restore registers, build a new stack frame, then execute a trivial bounds check + pointer arithmetic. Benchmarks show TML `push` (reserved capacity) at 5 ns/op (170M ops/sec) vs Rust at 1 ns/op (604M ops/sec) — a 3.6x gap. TML `get` at 3 ns/op (258M ops/sec) vs Rust at <1 ns/op (1.4B ops/sec) — a 5.5x gap. Rust's `Vec::push` is marked `#[inline]` and the compiler eliminates the call entirely. The same approach must be applied to TML's List.

## What Changes
The hot List methods (`push`, `pop`, `get`, `set`, `len`) in `lib/std/src/collections/list.tml` will be annotated with `@inline`. If the `@inline` attribute is not yet supported by the compiler, it will be implemented first: the compiler will emit LLVM `alwaysinline` metadata for annotated functions, causing LLVM to inline the call site unconditionally. The resulting IR for `list.get(i)` should reduce to a single `getelementptr` + `load` with no `call` instruction.

## Impact
- Affected specs: std/collections/list
- Affected code: `lib/std/src/collections/list.tml` (annotation), and optionally `compiler/src/codegen/` (attribute support)
- Breaking change: NO
- User benefit: 2–5x speedup for all code that iterates or appends to List, including the standard library, compiler internals, and user programs. No source-level changes required.
