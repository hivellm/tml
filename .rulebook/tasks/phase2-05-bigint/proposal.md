# Proposal: BigInt — Arbitrary Precision Integers

## Status: PROPOSED

## Summary

An arbitrary-precision integer type (`BigInt`) supporting arithmetic, comparison, display, and cryptographic operations (modular exponentiation, Miller-Rabin primality). Integers are represented as sign-magnitude with a `List[U64]` of base-2^64 limbs. Operations include all standard arithmetic operators plus `mod_pow`, `gcd`, `bit_length`, and bitwise operations.

## Motivation

`I64` overflows at ±9.2×10¹⁸. Real-world use cases that exceed this range include: RSA key generation (2048-bit integers), precise financial arithmetic (no floating-point rounding), factorial of large numbers, cryptographic hash preimage search, and Fibonacci sequences in competitive programming.

Go ships `math/big` in the standard library. TML's crypto module cannot implement RSA without BigInt — currently it delegates entirely to OpenSSL. Having a TML-native BigInt means the crypto module can implement protocols that OpenSSL doesn't support, and pure-TML environments (no OpenSSL) can still do public-key crypto.

## Design

`BigInt` is a struct `{ negative: Bool, limbs: List[U64] }`. The limbs array stores digits in base 2^64, least-significant first, with no leading zeros (the zero value has an empty limbs list). This representation makes addition/subtraction straightforward carry-propagation loops.

Arithmetic uses schoolbook algorithms for correctness and simplicity. Karatsuba multiplication is a future optimization (not in scope for this task).

`mod_pow` uses the square-and-multiply algorithm, which is the standard efficient method for RSA-scale exponentiation. `mod_inverse` uses the extended Euclidean algorithm. `is_probably_prime` uses Miller-Rabin with the given number of witness rounds (25 rounds gives 2^-50 false-positive probability).

All math operations are implemented in pure TML using `List[U64]` — no C code, no `lowlevel` blocks except where U64 overflow behavior is needed.

## What Changes

- New: `lib/std/src/math/` directory
- New: `lib/std/src/math/mod.tml` — module exports
- New: `lib/std/src/math/bigint.tml` — BigInt with all arithmetic and crypto operations
- Modified: `lib/std/src/mod.tml` — export `math` module
- New: `lib/std/tests/math/bigint_arithmetic.test.tml`
- New: `lib/std/tests/math/bigint_display.test.tml`
- New: `lib/std/tests/math/bigint_crypto.test.tml`

## Dependencies

- Depends on: `List[T]`, `Outcome[T,E]`, `Display`, `Debug`, `Ord` from core
- Enables: RSA implementation in `std/crypto` without OpenSSL dependency
- Enables: `phase2-06-complex-numbers` (Complex[BigInt] for exact arithmetic)
- Related: `phase2-04-seek-behavior` (BigInt parsing can use seekable streams for large inputs)

## Risks

- Schoolbook multiplication is O(n²) in the number of limbs; for 2048-bit RSA (32 limbs) this is 1024 operations per multiply — acceptable, but not suitable for very large numbers (10000+ bits) without Karatsuba
- `from_str` decimal parsing requires repeated division by 10 on the BigInt, which is O(n²) in the number of decimal digits — for large inputs, consider base-conversion using a power-of-ten table
- `is_probably_prime` with 25 rounds takes O(25 × log(n) × M(n)) time where M(n) is multiplication cost; for 2048-bit numbers this is the bottleneck in key generation and must be documented
