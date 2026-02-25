# TML Standard Library: Random

> `std::random` — Pseudo-random number generation using xoshiro256** algorithm.

## Overview

Provides fast, high-quality pseudo-random number generation. The core `Rng` type implements the xoshiro256** algorithm. `ThreadRng` wraps `Rng` with high-entropy seeding from the OS. Convenience free functions provide quick one-off random values without managing state.

**Not cryptographically secure.** For cryptographic randomness, use `std::crypto::random`.

## Import

```tml
use std::random::{Rng, ThreadRng, random_i64, random_range}
```

---

## Rng

A xoshiro256** pseudo-random number generator with explicit state.

### Construction

```tml
func Rng::new() -> Rng                // Default seed
func Rng::with_seed(seed: I64) -> Rng // Deterministic seed
```

### Methods

```tml
func next_i64(mut self) -> I64
func range(mut self, min: I64, max: I64) -> I64
func next_bool(mut self) -> Bool
func next_f64(mut self) -> F64
func range_f64(mut self, min: F64, max: F64) -> F64
func shuffle_i64(mut self, list: List[I64])
func shuffle_i32(mut self, list: List[I32])
```

---

## ThreadRng

A high-entropy seeded PRNG suitable for general-purpose use. Seeds itself from OS entropy on creation.

### Construction

```tml
func ThreadRng::new() -> ThreadRng
```

### Methods

```tml
func next_i64(mut self) -> I64
func range(mut self, min: I64, max: I64) -> I64
func next_bool(mut self) -> Bool
func next_f64(mut self) -> F64
func range_f64(mut self, min: F64, max: F64) -> F64
func reseed(mut self)  // Re-seed from OS entropy
```

---

## Free Functions

Convenience functions that use a thread-local `Rng` internally.

```tml
func random_i64() -> I64
func random_f64() -> F64
func random_bool() -> Bool
func random_range(min: I64, max: I64) -> I64
func thread_random_i64() -> I64
func thread_random_range(min: I64, max: I64) -> I64
```

---

## Example

```tml
use std::random::{Rng, random_range}

func main() {
    // Quick one-off random number
    let roll = random_range(1, 6)
    print("Dice roll: {roll}\n")

    // Deterministic sequence with explicit seed
    var rng = Rng::with_seed(42)
    loop i in 0 to 5 {
        let n = rng.range(0, 100)
        print("  {n}")
    }
    print("\n")

    // Shuffle a list
    var items = [1, 2, 3, 4, 5]
    rng.shuffle_i64(items)
    print("Shuffled: {items}\n")
}
```
