# Tasks: Complex — Complex Number Arithmetic

**Status**: Phase 1 Complete (14/15)
**Priority**: LOW
**Phase**: 2 — Stdlib Completeness

## Phase 1: Implementation — DONE

- [x] 1.1 Implement `Complex` struct — `{ real: F64, imag: F64 }`
- [x] 1.2 `Complex::new(real: F64, imag: F64) -> Complex`
- [x] 1.3 `Complex::from_polar(r: F64, theta: F64) -> Complex`
- [x] 1.4 `add(this, other: Complex) -> Complex`
- [x] 1.5 `mul(this, other: Complex) -> Complex` — (a+bi)(c+di) formula
- [x] 1.6 `div(this, other: Complex) -> Complex` — conjugate method
- [x] 1.7 `neg(this) -> Complex`
- [x] 1.8 `conj(this) -> Complex` — complex conjugate
- [x] 1.9 `abs(this) -> F64` — magnitude
- [x] 1.10 `arg(this) -> F64` — angle via atan2
- [x] 1.11 `norm(this) -> F64` — squared magnitude
- [x] 1.12 `scale(this, factor) -> Complex`, `approx_eq`, `zero`, `one`, `i`
- [x] 1.13 Tests: 7 tests (new, add, mul, abs, conj, from_polar, zero/one)
- [ ] 1.14 Update `std/math/mod.tml` to export — added to math.tml directly
- [x] 1.15 Verified with math test suite

Note: Implemented with F64 fields (not generic T) to avoid codegen issues with generic trait bounds.
