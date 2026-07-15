# Proposal: phase0y_simd-list-iteration

## Why

TML `for x in list` iteration runs at 2.6B ops/s while Rust `for x in vec.iter()`
runs at 17.7B ops/s — a 6.8× gap. Rust achieves this by:

1. **Pointer-stepping without Option check**: Rust's `slice::Iter::next()` returns
   `Option<&T>`, but LLVM eliminates the `Option` discriminant check via bounds
   analysis — the loop condition (`ptr != end`) already guarantees the item exists.
   After inlining + SimplifyCFG, the loop body is just `load + add + ptr_advance`.

2. **SIMD auto-vectorization**: With the branch-free loop body, LLVM's LoopVectorizer
   widens to `<2 x i64>` or `<4 x i64>` loads + adds, processing 2-4 elements per
   cycle. The Rust IR shows `vector.body:` with `load <2 x i64>` and `add <2 x i64>`.

TML's current `ListIter::next()` returns `Maybe[T]` with a pointer comparison
(`ptr == end`) + branch, which the LLVM vectorizer cannot eliminate because:
- The `Maybe` enum is a `{ i32, T }` struct (discriminant + payload), not a nullable ptr
- The discriminant check (`icmp eq i32 %tag, 1`) creates control flow in the loop body
- LLVM's "Control flow cannot be substituted for a select" blocks vectorization

## What Changes

### 1. Codegen: `for x in list` desugaring bypasses Maybe[T] entirely

Instead of calling `ListIter::next() -> Maybe[T]` and pattern-matching the result,
the for-in codegen for `IntoIterator` types will desugar directly to:

```llvm
; Preheader: extract ptr, end, stride from ListIter
%ptr_init = ...  ; data pointer
%end = ...       ; one-past-end pointer

; Header: phi-based pointer stepping
loop.header:
  %ptr = phi ptr [ %ptr_init, %preheader ], [ %ptr.next, %loop.body ]
  %done = icmp eq ptr %ptr, %end
  br i1 %done, label %exit, label %loop.body

loop.body:
  %val = load i64, ptr %ptr, align 8     ; direct load, no Maybe wrapper
  ; ... user body code using %val ...
  %ptr.next = getelementptr i8, ptr %ptr, i64 8  ; advance by stride
  br label %loop.header, !llvm.loop !vectorize
```

This is the EXACT pattern Rust generates. No `Maybe`, no discriminant, no branch
in the body. LLVM sees a canonical counted loop with `getelementptr` and vectorizes.

### 2. Add `!alias.scope` metadata to loop loads

Rust emits `!alias.scope` and `!noalias` metadata on iterator loads, telling LLVM
the read doesn't alias any writes. TML should emit the same for `for-in` loads over
immutable collections.

### 3. Remove `personality ptr @__CxxFrameHandler3` from leaf functions

Functions that don't use try/catch/throw don't need unwind personality. Removing it
lets LLVM be more aggressive with optimizations. Rust only adds personality to
functions that actually have landing pads.

## Impact

- Affected specs: codegen/loop, codegen/iterator, std/collections/list
- Affected code:
  - `compiler/src/codegen/llvm/control/loop.cpp` — new `gen_for_pointer_stepping` path
  - `compiler/src/codegen/llvm/core/generate_support.cpp` — alias scope metadata
  - `compiler/src/codegen/llvm/decl/func.cpp` — conditional personality
  - `lib/std/src/collections/behaviors.tml` — ListIter kept for manual Iterator use
- Breaking change: NO (for-in semantics unchanged, only IR shape changes)
- User benefit: 6-7× speedup for list iteration → Rust parity at 15-18B ops/s
