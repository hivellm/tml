# Proposal: Complex[T] — Complex Number Arithmetic

## Status: PROPOSED

## Summary

A generic complex number type `Complex[T]` representing a + bi, where T can be any numeric type supporting the required arithmetic operations. Provides standard complex arithmetic (add, sub, mul, div), geometric operations (abs/magnitude, arg/angle, conjugate), polar form construction, and standard behavior implementations (Display, Clone, PartialEq).

## Motivation

Complex numbers appear in signal processing (FFT), control systems (transfer functions), electrical engineering (impedance), physics (quantum mechanics), and pure mathematics. Go's `math/cmplx` is part of the standard library for this reason.

The TML standard library targets scientific and systems programming. Without `Complex[T]`, implementing FFT in TML requires embedding complex arithmetic inline everywhere, coupling numeric logic with algorithm logic. A standard type enables code reuse across domains.

`Complex[F64]` is the primary use case, but making the type generic over T enables `Complex[BigInt]` for exact arithmetic and `Complex[F32]` for SIMD-friendly computations.

## Design

`Complex[T]` is a simple struct `{ real: T, imag: T }` with no hidden state. The constraint on T is `Add + Sub + Mul + Div + Neg` (all arithmetic behaviors).

Arithmetic implementations follow standard complex number rules:
- Addition/subtraction: component-wise
- Multiplication: `(a+bi)(c+di) = (ac-bd) + (ad+bc)i`
- Division: multiply numerator and denominator by the conjugate of the denominator

`abs` (magnitude) and `arg` (angle) require `F64` arithmetic regardless of T — these are provided as concrete methods on `Complex[F64]` rather than as generic methods, avoiding the need for a `Sqrt` behavior.

`from_polar(r, theta)` constructs from polar coordinates using `cos`/`sin` from `std/math` — also concrete on `Complex[F64]`.

## What Changes

- New: `lib/std/src/math/complex.tml` — Complex[T] struct with all operations
- Modified: `lib/std/src/math/mod.tml` — export Complex (requires `phase2-05-bigint` to have created this module)
- New: `lib/std/tests/math/complex_arithmetic.test.tml`
- New: `lib/std/tests/math/complex_polar.test.tml`

## Dependencies

- Depends on: `phase2-05-bigint` to have created `lib/std/src/math/` module structure
- Depends on: `std/math` for `cos`, `sin`, `sqrt`, `atan2`
- Enables: FFT implementation, scientific computing libraries built on TML

## Risks

- The generic constraint `T: Add + Sub + Mul + Div + Neg` requires the behavior system to support compound constraints; if this syntax is not yet working, the type may need to be constrained to `Complex[F64]` and `Complex[F32]` only for the initial implementation
- `PartialEq` on `Complex[F64]` inherits the floating-point equality problem — two mathematically equal complex numbers computed differently will not compare equal; this must be documented and an `approx_eq(epsilon)` method provided
