# Tasks: Slice, Num, Fmt, Convert, Future Extras

**Status**: Proposed
**Priority**: LOW
**Phase**: 7 — Rust Parity

## Phase 1: Slice Extras

- [ ] 1.1 `split_first(this) -> Maybe[(ref T, Slice[T])]`
- [ ] 1.2 `split_last(this) -> Maybe[(ref T, Slice[T])]`
- [ ] 1.3 `split(this, pred: func(ref T) -> Bool) -> List[Slice[T]]` — predicate-based split
- [ ] 1.4 `splitn(this, n: I64, pred: func(ref T) -> Bool) -> List[Slice[T]]`
- [ ] 1.5 `rsplitn(this, n: I64, pred: func(ref T) -> Bool) -> List[Slice[T]]`
- [ ] 1.6 `partition_point(this, pred: func(ref T) -> Bool) -> I64`
- [ ] 1.7 Tests

## Phase 2: NonZero[T] Math Methods

- [ ] 2.1 `checked_add(this, other: NonZero[T]) -> Maybe[NonZero[T]]`
- [ ] 2.2 `checked_sub(this, other: NonZero[T]) -> Maybe[NonZero[T]]`
- [ ] 2.3 `checked_mul(this, other: NonZero[T]) -> Maybe[NonZero[T]]`
- [ ] 2.4 `saturating_add(this, other: NonZero[T]) -> NonZero[T]`
- [ ] 2.5 `saturating_sub(this, other: NonZero[T]) -> NonZero[T]`
- [ ] 2.6 `saturating_mul(this, other: NonZero[T]) -> NonZero[T]`
- [ ] 2.7 `count_ones(this) -> I32`, `leading_zeros(this) -> I32`, `trailing_zeros(this) -> I32`
- [ ] 2.8 `pow(this, exp: I32) -> NonZero[T]`
- [ ] 2.9 `ilog2(this) -> I32`
- [ ] 2.10 Tests

## Phase 3: Wrapping[T] Extras

- [ ] 3.1 `pow(this, exp: I32) -> Wrapping[T]`
- [ ] 3.2 `shl(this, n: I32) -> Wrapping[T]`
- [ ] 3.3 `shr(this, n: I32) -> Wrapping[T]`
- [ ] 3.4 Bitwise: `bitand`, `bitor`, `bitxor`, `not`
- [ ] 3.5 `ParseIntError` + `IntErrorKind` types in `core::num`
- [ ] 3.6 Tests

## Phase 4: Fmt Extras

- [ ] 4.1 `impl Write for Formatter` — make Formatter a write sink
- [ ] 4.2 `sign_plus(this) -> Bool` / `sign_minus(this) -> Bool` — shorthand accessors
- [ ] 4.3 Tests

## Phase 5: Convert + Future Extras

- [ ] 5.1 `Infallible` type in `core::traits::convert` — uninhabited error type
- [ ] 5.2 `FutureExt::and_then` — uncomment and implement
- [ ] 5.3 `FutureExt::fuse` — uncomment and implement
- [ ] 5.4 `impl Future for Flatten` — complete the type
- [ ] 5.5 Tests
