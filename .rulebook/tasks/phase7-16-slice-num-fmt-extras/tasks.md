# Tasks: Slice, Num, Fmt, Convert, Future Extras

**Status**: 80% complete (21/26 items done, 5 blocked by codegen)
**Priority**: LOW
**Phase**: 7 — Rust Parity

## Phase 1: Slice Extras — COMPLETE

- [x] 1.1 `split_first(this) -> Maybe[(ref T, Slice[T])]`
- [x] 1.2 `split_last(this) -> Maybe[(ref T, Slice[T])]`
- [x] 1.3 `split(this, pred: func(ref T) -> Bool) -> List[Slice[T]]` — predicate-based split
- [x] 1.4 `splitn(this, n: I64, pred: func(ref T) -> Bool) -> List[Slice[T]]`
- [x] 1.5 `rsplitn(this, n: I64, pred: func(ref T) -> Bool) -> List[Slice[T]]`
- [x] 1.6 `partition_point(this, pred: func(ref T) -> Bool) -> I64`
- [x] 1.7 Tests — 3 test files: slice_split_first_last, slice_partition_point, slice_split_pred, slice_splitn

## Phase 2: NonZero[T] Math Methods — PARTIAL (codegen blocked)

- [x] 2.1-2.3 `checked_add/sub/mul` — IMPLEMENTED but BLOCKED: `Maybe[NonZero[I32]]` return type generates IR with wrong type layout (NonZero__I64 instead of NonZero__I32)
- [x] 2.4-2.6 `saturating_add/sub/mul` — IMPLEMENTED but BLOCKED: same codegen bug as checked methods
- [x] 2.7 `count_ones(this) -> I32`, `leading_zeros(this) -> I32`, `trailing_zeros(this) -> I32` — WORKING
- [x] 2.8 `pow(this, exp: I32) -> NonZero[I32]` — WORKING
- [x] 2.9 `ilog2(this) -> I32` — WORKING
- [x] 2.10 Tests — nonzero_math.test.tml (5 passing tests for I32 methods)
- [ ] 2.11 NonZero[I64] methods — BLOCKED: having both `impl NonZero[I32]` and `impl NonZero[I64]` in same file causes type confusion in codegen

## Phase 3: Wrapping[T] Extras — COMPLETE

- [x] 3.1 `pow(this, exp: I32) -> Wrapping[I32]` — wrapping exponentiation
- [x] 3.2 `shift_left(this, n: I32) -> Wrapping[I32]` — (renamed from `shl` which is a keyword)
- [x] 3.3 `shift_right(this, n: I32) -> Wrapping[I32]` — (renamed from `shr` which is a keyword)
- [x] 3.4 Bitwise: `bitand`, `bitor`, `bitxor`, `bitnot` — all using LLVM intrinsics
- [x] 3.5 `ParseIntError` + `IntErrorKind` types in `core::num` — added with Display/Debug impls
- [x] 3.6 Tests — wrapping_extras.test.tml + parse_int_error.test.tml

## Phase 4: Fmt Extras — PARTIAL (codegen blocked)

- [ ] 4.1 `impl Write for Formatter` — BLOCKED: behavior default method dispatch passes Str as i32 instead of ptr (known codegen bug)
- [x] 4.2 `sign_plus(this) -> Bool` / `sign_minus(this) -> Bool` — shorthand accessors
- [x] 4.3 Tests — fmt_sign_plus_minus.test.tml

## Phase 5: Convert + Future Extras — PARTIAL

- [x] 5.1 `Infallible` type in `core::traits::convert` — uninhabited error type with Display/Debug
- [x] 5.2 `FutureExt::and_then` — uncommented and implemented
- [x] 5.3 `FutureExt::fuse` — uncommented and implemented
- [ ] 5.4 `impl Future for Flatten` — NOT IMPLEMENTED: requires associated type resolution + Pin + enum state mutation, all known codegen problem areas
- [x] 5.5 Tests — convert_infallible.test.tml (FutureExt methods cannot be tested in isolation due to async runtime requirements)

## Codegen Blockers Summary

1. **NonZero dual-impl confusion**: `impl NonZero[I32]` + `impl NonZero[I64]` in same file → codegen uses wrong type layout
2. **Maybe[NonZero[T]] return type**: Concrete generic specializations returning Maybe of the wrapper type get wrong LLVM type
3. **Write behavior default methods**: `write_char`/`write_fmt` pass Str as i32 instead of ptr
4. **Associated type + Pin**: Complex generic patterns needed for `impl Future for Flatten` are not reliable in current codegen
