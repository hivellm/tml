# Proposal: Slice, Num, Fmt, Convert, Future Extras

## Why

Slice missing split_first/split_last/predicate-split (7 methods). NonZero[T] has only new/get — missing 14 checked/saturating math methods. Fmt Formatter doesn't implement Write. Convert missing Infallible. Future combinators and_then/fuse commented out.

## What Changes

Add missing methods across slice, num, fmt, convert, and future modules. All pure TML — no compiler changes.

## Impact
- Affected specs: core::slice, core::num, core::fmt, core::traits::convert, core::future
- Affected code: `lib/core/src/slice/`, `lib/core/src/num/`, `lib/core/src/fmt/`, `lib/core/src/traits/convert.tml`, `lib/core/src/future/`
- Breaking change: NO
- User benefit: Complete slice operations, NonZero math, Formatter as Write sink, Infallible type, future combinators
