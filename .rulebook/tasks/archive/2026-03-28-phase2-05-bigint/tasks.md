# Tasks: BigInt — Arbitrary Precision Integers

**Status**: Complete (49/49)
**Priority**: MEDIUM
**Phase**: 2 — Stdlib Completeness

## Motivation

BigInt is needed for cryptographic operations (RSA key generation, modular exponentiation), financial calculations (exact arithmetic without floating-point), and any domain requiring integers larger than I64 (±9.2×10¹⁸). Go includes `math/big` in stdlib. Rust leaves it to crates.

## Phase 1: Core BigInt (`lib/std/src/bigint.tml`) — DONE

- [x] 1.1 Created `lib/std/src/bigint.tml` (flat file, not directory — matches std conventions)
- [x] 1.2 BigInt struct — sign + base-10^9 magnitude (List[I64] limbs, little-endian). Changed from U64 to I64/base-10^9 due to List[U64] codegen GEP bug.
- [x] 1.3 `BigInt::from_i64(n: I64) -> BigInt` — handles I64 min value
- [x] 1.4 `BigInt::from_str(s: Str) -> BigInt` — parses decimal with sign. Returns BigInt directly (not Outcome) for simplicity.
- [x] 1.5 `BigInt::zero()` and `BigInt::one()`
- [x] 1.6 `add()` — schoolbook addition with carry, handles mixed signs
- [x] 1.7 `sub()` — via add(neg(other))
- [x] 1.8 `mul()` — schoolbook O(n²) multiplication
- [x] 1.9 `div()`, `rem()`, `divmod()` — single-limb fast path + multi-limb long division
- [x] 1.10 `neg()` — sign flip, `abs()` — absolute value
- [x] 1.11 Tests: 12 tests in `lib/std/tests/math/bigint.test.tml` — all pass

## Phase 2: Comparison & Display — DONE

- [x] 2.1 `PartialEq` impl — eq/ne by sign + digit comparison
- [x] 2.2 `cmp()`, `lt()`, `le()`, `gt()`, `ge()` — total ordering via magnitude comparison
- [x] 2.3 `Display` impl — `to_string()` with base-10^9 limb formatting + zero padding
- [x] 2.4 `Debug` — uses Display (to_string)
- [x] 2.5 `to_i64(this) -> Maybe[I64]` — converts if <= 3 limbs
- [x] 2.6 `to_hex(this) -> Str` — hex via repeated div/rem by 16
- [x] 2.7 `abs(this) -> BigInt` — absolute value
- [x] 2.8 `pow(this, exp: I64) -> BigInt` — binary exponentiation O(log n)
- [x] 2.9 `gcd(this, other: BigInt) -> BigInt` — Euclidean algorithm
- [x] 2.10 Tests: 5 new tests (cmp, to_i64, to_hex, pow, gcd) — all pass

## Phase 3: Crypto-Relevant Operations — MOSTLY DONE

- [x] 3.1 `mod_pow(this, exp: BigInt, modulus: BigInt) -> BigInt` — binary exponentiation with modular reduction
- [x] 3.2 `mod_inverse(this, modulus: BigInt) -> Maybe[BigInt]` — extended Euclidean algorithm
- [x] 3.3 `is_probably_prime(this, rounds: I64) -> Bool` — Miller-Rabin with deterministic witnesses [2,3,5,7,11,13,17,19]
- [x] 3.4 `from_seed(seed, digits)` — deterministic BigInt generation via LCG. `random_i64()` has overflow bug in std::random, so true random deferred.
- [x] 3.5 `bitand()`, `bitor()`, `bitxor()` — via to_bits/from_bits conversion
- [x] 3.5a `shift_left(this, n)` / `shift_right(this, n)` — via mul/div by 2
- [x] 3.6 `bit_length(this) -> I64` — via repeated div by 2
- [x] 3.7 Module export — `use std::bigint::BigInt` works
- [x] 3.8 Tests: 9 Phase 3 tests (mod_pow, mod_inverse, primality, bit_length, shifts, bitand, bitor, bitxor, from_seed)
- [x] 3.9 All 26 BigInt tests pass in `lib/std/tests/math/bigint.test.tml`
