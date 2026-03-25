# Tasks: Complex[T] — Complex Number Arithmetic

**Status**: Proposed
**Priority**: LOW
**Phase**: 2 — Stdlib Completeness

## Motivation

Complex numbers are needed for signal processing (FFT), control systems, physics simulations, and mathematical computing. Go includes `math/cmplx` in stdlib. Simple enough to include in std/math.

## Phase 1: Implementation (`lib/std/src/math/complex.tml`)

- [ ] 1.1 Implement `Complex[T]` struct — `{ real: T, imag: T }` where T: Add + Sub + Mul + Div + Neg
- [ ] 1.2 `Complex::new(real: T, imag: T) -> Complex[T]`
- [ ] 1.3 `Complex::from_polar(r: F64, theta: F64) -> Complex[F64]`
- [ ] 1.4 Implement `Add`, `Sub` — component-wise
- [ ] 1.5 Implement `Mul` — (a+bi)(c+di) = (ac-bd) + (ad+bc)i
- [ ] 1.6 Implement `Div` — division by conjugate multiplication
- [ ] 1.7 Implement `Neg` — negate both components
- [ ] 1.8 `conj(this) -> Complex[T]` — complex conjugate
- [ ] 1.9 `abs(this) -> F64` — magnitude (modulus) = sqrt(real² + imag²)
- [ ] 1.10 `arg(this) -> F64` — argument (angle) = atan2(imag, real)
- [ ] 1.11 `norm(this) -> T` — squared magnitude = real² + imag²
- [ ] 1.12 Implement `PartialEq`, `Eq`, `Display`, `Debug`, `Clone`, `Default`
- [ ] 1.13 Write tests: arithmetic, polar, conjugate, magnitude, display
- [ ] 1.14 Update `std/math/mod.tml` to export Complex
- [ ] 1.15 Run math test suite
