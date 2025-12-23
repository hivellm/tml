# TML Math Module

Comprehensive mathematical functions library based on Go's `math` package, implemented in pure TML.

## Overview

The math module provides basic mathematical constants, functions for trigonometry, logarithms, exponentials, special functions, and floating-point manipulation. All implementations are in pure TML without C dependencies.

## File Structure

The math module is organized into logical groups:

```
math/
├── mod.tml              # Main module with all exports
├── const.tml            # Mathematical constants (Pi, E, etc.)
├── abs.tml              # Absolute value functions
├── minmax.tml           # Min, max, clamp functions
├── dim.tml              # Positive difference
├── copysign.tml         # Sign manipulation
├── floor.tml            # Floor function
├── ceil.tml             # Ceiling function
├── round.tml            # Rounding functions
├── trunc.tml            # Truncation
├── mod.tml              # Modulo/remainder
├── sqrt.tml             # Square root
├── cbrt.tml             # Cube root
├── pow.tml              # Power functions
├── hypot.tml            # Hypotenuse
├── exp.tml              # Exponential functions
├── log.tml              # Logarithm functions
├── sin.tml              # Sine
├── cos.tml              # Cosine
├── tan.tml              # Tangent
├── asin.tml             # Arcsine
├── acos.tml             # Arccosine
├── atan.tml             # Arctangent
├── sinh.tml             # Hyperbolic sine
├── cosh.tml             # Hyperbolic cosine
├── tanh.tml             # Hyperbolic tangent
├── asinh.tml            # Inverse hyperbolic sine
├── acosh.tml            # Inverse hyperbolic cosine
├── atanh.tml            # Inverse hyperbolic tangent
├── gamma.tml            # Gamma function
├── erf.tml              # Error functions
├── bessel.tml           # Bessel functions
├── frexp.tml            # Break float into fraction and exponent
├── ldexp.tml            # Construct float from fraction and exponent
├── modf.tml             # Integer and fractional parts
├── nextafter.tml        # Next representable float
├── fma.tml              # Fused multiply-add
├── bits.tml             # Bit-level float manipulation
└── inf.tml              # Infinity and NaN
```

## Usage

```tml
use std::math::{Pi, sin, cos, sqrt, log, exp}

func main() {
    // Constants
    let circle_area: F64 = Pi * 5.0 * 5.0

    // Trigonometry
    let angle: F64 = Pi / 4.0
    let sine: F64 = sin(angle)
    let cosine: F64 = cos(angle)

    // Power and root
    let root: F64 = sqrt(16.0)

    // Logarithms and exponentials
    let logarithm: F64 = log(10.0)
    let exponential: F64 = exp(2.0)
}
```

## Constants

### Mathematical Constants
- `E` - Euler's number (2.71828...)
- `Pi` - Pi (3.14159...)
- `Phi` - Golden ratio (1.61803...)
- `Sqrt2`, `SqrtE`, `SqrtPi`, `SqrtPhi` - Square roots
- `Ln2`, `Ln10` - Natural logarithms
- `Log2E`, `Log10E` - Logarithm conversions

### Floating-Point Limits
- `MaxFloat64`, `MaxFloat32` - Largest finite values
- `SmallestNonzeroFloat64`, `SmallestNonzeroFloat32` - Smallest positive values

### Integer Limits
- `MaxInt8`, `MinInt8`, `MaxUint8`
- `MaxInt16`, `MinInt16`, `MaxUint16`
- `MaxInt32`, `MinInt32`
- `MaxInt64`, `MinInt64`

## Function Categories

### Basic Arithmetic
- `abs(x)` - Absolute value
- `min(x, y)`, `max(x, y)` - Minimum and maximum
- `clamp(value, min, max)` - Restrict value to range
- `dim(x, y)` - Positive difference (max(x-y, 0))
- `copysign(f, sign)` - Value with specific sign
- `signbit(x)` - Check if negative

### Rounding
- `floor(x)` - Greatest integer ≤ x
- `ceil(x)` - Least integer ≥ x
- `round(x)` - Round to nearest integer
- `round_to_even(x)` - Round ties to even
- `trunc(x)` - Truncate toward zero

### Power and Root
- `sqrt(x)` - Square root
- `cbrt(x)` - Cube root
- `pow(x, y)` - x raised to power y
- `powi(x, n)` - x raised to integer power n
- `pow10(n)` - 10 raised to power n
- `hypot(p, q)` - sqrt(p² + q²)

### Exponential
- `exp(x)` - e^x
- `exp2(x)` - 2^x
- `expm1(x)` - e^x - 1 (accurate for small x)

### Logarithmic
- `log(x)` - Natural logarithm
- `log2(x)` - Base-2 logarithm
- `log10(x)` - Base-10 logarithm
- `log1p(x)` - log(1 + x) (accurate for small x)
- `logb(x)` - Binary exponent
- `ilogb(x)` - Binary exponent as integer

### Trigonometric
- `sin(x)`, `cos(x)`, `tan(x)` - Sine, cosine, tangent
- `asin(x)`, `acos(x)`, `atan(x)` - Inverse functions
- `atan2(y, x)` - Two-argument arctangent
- `sincos(x)` - Compute sin and cos simultaneously

### Hyperbolic
- `sinh(x)`, `cosh(x)`, `tanh(x)` - Hyperbolic functions
- `asinh(x)`, `acosh(x)`, `atanh(x)` - Inverse hyperbolic functions

### Special Functions
- `gamma(x)` - Gamma function
- `lgamma(x)` - Natural log of gamma function
- `erf(x)`, `erfc(x)` - Error functions
- `erfinv(x)`, `erfcinv(x)` - Inverse error functions
- `j0(x)`, `j1(x)`, `jn(n, x)` - Bessel functions (first kind)
- `y0(x)`, `y1(x)`, `yn(n, x)` - Bessel functions (second kind)

### Floating-Point Manipulation
- `frexp(f)` - Break into fraction and exponent
- `ldexp(frac, exp)` - Construct from fraction and exponent
- `modf(f)` - Integer and fractional parts
- `nextafter(x, y)` - Next representable value
- `fma(x, y, z)` - Fused multiply-add (x*y + z)

### Special Values
- `inf(sign)` - Infinity
- `is_inf(f, sign)` - Check if infinity
- `nan()` - Not-a-number
- `is_nan(f)` - Check if NaN

## Implementation Details

All functions are implemented in pure TML using:
- **Taylor series** for trigonometric and exponential functions
- **Newton-Raphson method** for square root and cube root
- **Lanczos approximation** for gamma function
- **Fast exponentiation by squaring** for integer powers
- **Range reduction** for accurate results across input ranges

### Precision
- F64 functions target ~10^-7 relative accuracy
- Iterative methods use convergence thresholds of ~10^-8
- Maximum iterations prevent infinite loops

### Type Variants
Many functions provide type-specific variants:
- `abs_i32()`, `abs_i64()`, `abs_f64()` for different types
- `sqrt_i32()` for integer square root
- Similar variants for min, max, clamp, etc.

## Testing

Run the math module tests:

```bash
tml test packages/std/tests/stdlib/math.test.tml
```

Tests cover:
- All basic arithmetic operations
- Rounding functions
- Power and root calculations
- Trigonometric accuracy
- Special cases (zero, negative, edge cases)

## Compatibility

This module is based on Go's `math` package but adapted for TML's type system:
- Go's `float64` maps to TML's `F64`
- Go's `float32` maps to TML's `F32`
- Go's `int` maps to TML's `I32`
- Some functions have simplified implementations due to TML limitations

## Future Enhancements

Planned improvements:
- [ ] Hardware FMA instruction support
- [ ] True IEEE 754 NaN and infinity handling
- [ ] Bit-level float manipulation (requires unsafe)
- [ ] SIMD vectorized implementations
- [ ] Extended precision variants (F128)
- [ ] Complex number support

## License

Part of the TML Standard Library.
