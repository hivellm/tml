# Tasks: BigInt — Arbitrary Precision Integers

**Status**: Proposed
**Priority**: MEDIUM
**Phase**: 2 — Stdlib Completeness

## Motivation

BigInt is needed for cryptographic operations (RSA key generation, modular exponentiation), financial calculations (exact arithmetic without floating-point), and any domain requiring integers larger than I64 (±9.2×10¹⁸). Go includes `math/big` in stdlib. Rust leaves it to crates.

## Phase 1: Core BigInt (`lib/std/src/math/bigint.tml`)

- [ ] 1.1 Create `lib/std/src/math/` directory and `mod.tml`
- [ ] 1.2 Implement `BigInt` struct — sign + magnitude representation (sign: Bool, digits: List[U64])
- [ ] 1.3 `BigInt::from_i64(n: I64) -> BigInt`
- [ ] 1.4 `BigInt::from_str(s: Str) -> Outcome[BigInt, Str]` — parse decimal string
- [ ] 1.5 `BigInt::zero() -> BigInt` and `BigInt::one() -> BigInt`
- [ ] 1.6 Implement `Add` — schoolbook addition with carry propagation
- [ ] 1.7 Implement `Sub` — subtraction with borrow
- [ ] 1.8 Implement `Mul` — schoolbook multiplication O(n²) (Karatsuba can be added later)
- [ ] 1.9 Implement `Div` and `Rem` — long division
- [ ] 1.10 Implement `Neg` — sign flip
- [ ] 1.11 Write tests: basic arithmetic, large numbers, edge cases (0, negative, overflow I64)

## Phase 2: Comparison & Display

- [ ] 2.1 Implement `PartialEq`, `Eq` — magnitude + sign comparison
- [ ] 2.2 Implement `PartialOrd`, `Ord` — total ordering
- [ ] 2.3 Implement `Display` — decimal string representation
- [ ] 2.4 Implement `Debug`
- [ ] 2.5 `to_i64(this) -> Maybe[I64]` — convert back if fits
- [ ] 2.6 `to_hex(this) -> Str` — hexadecimal representation
- [ ] 2.7 `abs(this) -> BigInt` — absolute value
- [ ] 2.8 `pow(this, exp: I64) -> BigInt` — exponentiation
- [ ] 2.9 `gcd(this, other: BigInt) -> BigInt` — greatest common divisor (Euclidean)
- [ ] 2.10 Write tests: comparison, display, hex, pow, gcd

## Phase 3: Crypto-Relevant Operations

- [ ] 3.1 `mod_pow(this, exp: BigInt, modulus: BigInt) -> BigInt` — modular exponentiation (for RSA)
- [ ] 3.2 `mod_inverse(this, modulus: BigInt) -> Maybe[BigInt]` — modular multiplicative inverse
- [ ] 3.3 `is_probably_prime(this, rounds: I64) -> Bool` — Miller-Rabin primality test
- [ ] 3.4 `random_bigint(bits: I64) -> BigInt` — random BigInt of given bit length
- [ ] 3.5 Implement `BitAnd`, `BitOr`, `BitXor`, `Shl`, `Shr` — bitwise operations
- [ ] 3.6 `bit_length(this) -> I64` — number of bits
- [ ] 3.7 Update `std/math/mod.tml` and `std/mod.tml` to export
- [ ] 3.8 Write tests: mod_pow, primality, bitwise, random
- [ ] 3.9 Run full math test suite
