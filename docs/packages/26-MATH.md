# TML Standard Library: Math

> `std::math` — Mathematical functions and constants wrapping LLVM intrinsics and libc FFI.

## Overview

Provides standard mathematical constants and transcendental functions for `F64` and `F32` types. Functions map directly to LLVM intrinsics where available (e.g., `sin`, `cos`, `sqrt`, `floor`, `ceil`) and fall back to libc FFI for others (e.g., `atan2`, `cbrt`, `hypot`). All `F64` functions have `F32` variants with a `_f32` suffix.

## Import

```tml
use std::math::{PI, sin, cos, sqrt}
```

---

## Constants

| Constant | Type | Value |
|----------|------|-------|
| `PI` | `F64` | 3.14159265358979323846 |
| `E` | `F64` | 2.71828182845904523536 |
| `TAU` | `F64` | 6.28318530717958647692 |
| `SQRT_2` | `F64` | 1.41421356237309504880 |
| `LN_2` | `F64` | 0.69314718055994530942 |
| `LN_10` | `F64` | 2.30258509299404568402 |
| `LOG2_E` | `F64` | 1.44269504088896340736 |
| `LOG10_E` | `F64` | 0.43429448190325182765 |
| `FRAC_1_PI` | `F64` | 0.31830988618379067154 |
| `FRAC_2_PI` | `F64` | 0.63661977236758134308 |
| `FRAC_1_SQRT_2` | `F64` | 0.70710678118654752440 |
| `PI_F32` | `F32` | 3.14159265 |

---

## Trigonometric Functions

```tml
func sin(x: F64) -> F64
func cos(x: F64) -> F64
func tan(x: F64) -> F64
func asin(x: F64) -> F64
func acos(x: F64) -> F64
func atan(x: F64) -> F64
func atan2(y: F64, x: F64) -> F64
func sinh(x: F64) -> F64
func cosh(x: F64) -> F64
func tanh(x: F64) -> F64
```

## Exponential and Logarithmic Functions

```tml
func exp(x: F64) -> F64
func ln(x: F64) -> F64
func log2(x: F64) -> F64
func log10(x: F64) -> F64
func pow(base: F64, exp: F64) -> F64
```

## Rounding Functions

```tml
func floor(x: F64) -> F64
func ceil(x: F64) -> F64
func round(x: F64) -> F64
func trunc(x: F64) -> F64
func fract(x: F64) -> F64
```

## Arithmetic and Utility Functions

```tml
func abs(x: F64) -> F64
func sqrt(x: F64) -> F64
func cbrt(x: F64) -> F64
func hypot(x: F64, y: F64) -> F64
func min(a: F64, b: F64) -> F64
func max(a: F64, b: F64) -> F64
func clamp(x: F64, lo: F64, hi: F64) -> F64
func mul_add(a: F64, b: F64, c: F64) -> F64
func copysign(magnitude: F64, sign: F64) -> F64
func signum(x: F64) -> F64
func to_radians(degrees: F64) -> F64
func to_degrees(radians: F64) -> F64
```

## F32 Variants

All functions above have `F32` variants with a `_f32` suffix:

```tml
func sin_f32(x: F32) -> F32
func cos_f32(x: F32) -> F32
func sqrt_f32(x: F32) -> F32
// ... and so on for every function listed above
```

---

## Example

```tml
use std::math::{PI, sin, cos, sqrt, atan2, to_radians}

func main() {
    let angle = to_radians(45.0)
    let s = sin(angle)
    let c = cos(angle)
    print("sin(45) = {s}, cos(45) = {c}\n")

    let dist = sqrt(3.0 * 3.0 + 4.0 * 4.0)
    print("distance = {dist}\n")

    let heading = atan2(1.0, 1.0)
    print("heading = {heading} rad\n")
}
```
