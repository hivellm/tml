# Proposal: phase0z_inline-iterator-rust-parity

## Why

TML `for x in list` runs at 4.4B ops/s while Rust `for x in vec.iter()` runs at 58B ops/s —
a 13× gap. Phase0y added pointer-stepping codegen that matches Rust's loop *structure*
(phi + GEP + load), but three IR-level differences prevent LLVM from vectorizing:

1. **`into_iter()` is NOT inlined** — stride is a runtime variable (Rust: constant `8`).
   Without constant stride LLVM can't compute trip count, prove alignment, or decide
   vectorization width. This alone accounts for ~10× of the gap.

2. **Accumulator goes through alloca** — `store i64 %total / load i64 %total` per iteration
   instead of SSA phi. mem2reg should fix this, but opaque calls before the loop prevent
   it. Accounts for ~1.5× of the gap.

3. **Element value goes through alloca** — the loaded element is stored to a local then
   reloaded, adding 2 unnecessary memory operations per iteration. ~1.2× gap.

Source: `.sandbox/rust_vs_tml_full_analysis.md` (full IR comparison with instruction counts).

## What Changes

### 1. `@inline` on iterator hot path (`behaviors.tml`)

Add `@inline` decorator to `into_iter()`, `iter()`, and `next()` on `ListIter[T]` and
`List[T]`. With inlining, LLVM will:
- See `stride = sizeof(T)` as a constant (e.g., `8` for I64)
- Promote all allocas to SSA via mem2reg
- Auto-vectorize to `<2 x i64>` or `<4 x i64>` SIMD loads

### 2. SSA accumulator in `gen_for_pointer_stepping` (`loop.cpp`)

Replace the alloca-based accumulator pattern with a phi node:
```llvm
; Before (current):
  %old = load i64, ptr %total_alloca
  %new = add i64 %old, %elem
  store i64 %new, ptr %total_alloca

; After:
  %total = phi i64 [ 0, %preheader ], [ %new, %latch ]
  %new = add i64 %total, %elem
```

This eliminates 2 memory operations per iteration even without inlining.

### 3. Eliminate element alloca (`loop.cpp`)

Currently the element is loaded from the pointer, stored to an alloca, then reloaded
for use in the body. Instead, bind the loop variable directly to the loaded SSA value
(only use alloca if the body takes the address of the variable).

### 4. Add `!noundef` metadata on element loads (`loop.cpp`)

Rust emits `!noundef` on all loads from valid pointers. Add the same to pointer-stepping
element loads to help LLVM eliminate null/undef checks.

### 5. Release-mode wrapping arithmetic (`binary_ops.cpp`)

At `-O2`/`--release`, emit `add nsw` instead of `@llvm.sadd.with.overflow` for signed
integer addition. This matches Rust's behavior where release mode uses wrapping
semantics. Without the overflow branch, the loop body is fully branch-free, enabling
SIMD vectorization for the checked add path too.

## Impact

- Affected specs: codegen/loop, codegen/arithmetic, std/collections/list
- Affected code:
  - `lib/std/src/collections/behaviors.tml` — `@inline` on iterator methods
  - `compiler/src/codegen/llvm/control/loop.cpp` — SSA accumulator, element binding
  - `compiler/src/codegen/llvm/expr/binary_ops.cpp` — release-mode wrapping arithmetic
- Breaking change: NO (semantics unchanged, only IR shape changes)
- User benefit: 13× → ~1× performance gap with Rust for list iteration
