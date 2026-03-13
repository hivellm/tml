# Slice/Array Iter Module Coverage Blockers (2026-03-08)

## slice/iter -- ALL 19 functions blocked

Two root causes:

### 1. Cross-file impl method resolution
Methods defined in `impl[T] Slice[T]` blocks inside submodule files (e.g., `slice/iter.tml`)
are not resolved by the compiler. Error: "Unknown method: chunks" (also iter, windows, chunks_exact).
Importing `use core::slice::iter::*` brings the types but not the impl methods.

Affected: `Slice::chunks`, `Slice::chunks_exact`, `Slice::windows`, `Slice::iter`,
`MutSlice::iter`, `MutSlice::iter_mut`

### 2. Named struct vs anonymous struct in LLVM IR
Constructing structs that contain `Slice[T]` fields emits anonymous `{ptr, i64}` literal
but the store expects named `%struct.Slice__I32 = type { ptr, i64 }`. LLVM rejects the mismatch.

Affected: ALL iterator types (SliceIter, SliceIterMut, Chunks, ChunksExact, Windows)
since they all have `slice: Slice[T]` fields.

Test files: `slice_iter_chunks.test.tml`, `slice_iter_elem.test.tml`

## array/iter -- ALL 19 functions blocked

### 1. Const generic not monomorphized in struct layout
`ArrayIter[T, const N: I64]` has `data: [T; N]` field. Codegen always emits `[0 x i32]`
instead of `[N x i32]` for concrete instantiation like `ArrayIter[I32, 3]`.
LLVM error: `'%t7' defined with type '[3 x i32]' but expected '[0 x i32]'`
`ArrayIter::new` crashes with ACCESS_VIOLATION due to same layout bug.

### 2. Cross-file impl method resolution (same as slice)
`Array::iter`, `Array::iter_mut`, `Array::into_iter` defined in `array/iter.tml` submodule.

Test file: `array_iter_new.test.tml`
