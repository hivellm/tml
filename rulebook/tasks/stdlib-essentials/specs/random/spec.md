# Spec: Random Module

## Overview

Random number generation with cryptographic and non-cryptographic options.

## Types

### Rng Behavior

Interface for random number generators.

```tml
pub behavior Rng {
    /// Generate random u32
    func next_u32(this) -> U32

    /// Generate random u64
    func next_u64(this) -> U64

    /// Fill buffer with random bytes
    func fill_bytes(this, buf: mut ref [U8])

    // Provided methods
    func gen[T: Random](this) -> T {
        T::random(this)
    }

    func gen_range[T: SampleUniform](this, range: Range[T]) -> T {
        T::sample_uniform(this, range)
    }

    func gen_bool(this, probability: F64) -> Bool {
        this.next_u64() as F64 / U64::MAX as F64 < probability
    }
}
```

### ThreadRng

Thread-local RNG seeded from system entropy.

```tml
pub type ThreadRng {
    // Internal state (e.g., ChaCha20 or Xoshiro256++)
}

extend ThreadRng with Rng {
    func next_u32(this) -> U32
    func next_u64(this) -> U64
    func fill_bytes(this, buf: mut ref [U8])
}

/// Get thread-local RNG
pub func thread_rng() -> ThreadRng
```

### StdRng

Standard RNG for reproducible sequences.

```tml
pub type StdRng {
    state: [U64; 4],  // Xoshiro256++ state
}

extend StdRng {
    /// Create from seed
    pub func from_seed(seed: [U8; 32]) -> StdRng

    /// Create from u64 seed
    pub func seed_from_u64(seed: U64) -> StdRng
}

extend StdRng with Rng { ... }
```

### OsRng

Cryptographically secure RNG from OS.

```tml
pub type OsRng { }

extend OsRng with Rng {
    func next_u32(this) -> U32
    func next_u64(this) -> U64
    func fill_bytes(this, buf: mut ref [U8])
}
```

## Convenience Functions

```tml
/// Generate random value of type T
pub func random[T: Random]() -> T {
    thread_rng().gen[T]()
}

/// Generate random value in range [min, max)
pub func random_range[T: SampleUniform](min: T, max: T) -> T {
    thread_rng().gen_range(min to max)
}

/// Generate random boolean
pub func random_bool() -> Bool {
    thread_rng().gen_bool(0.5)
}

/// Shuffle slice in-place
pub func shuffle[T](slice: mut ref [T]) {
    let rng = thread_rng()
    let len = slice.len()
    for i in (1 to len).rev() {
        let j = rng.gen_range(0 to i + 1)
        slice.swap(i, j)
    }
}

/// Choose random element from slice
pub func choose[T](slice: ref [T]) -> Maybe[ref T] {
    if slice.is_empty() {
        Nothing
    } else {
        Just(ref slice[random_range(0, slice.len())])
    }
}
```

## Distributions

### Uniform

Uniform distribution over a range.

```tml
pub type Uniform[T] {
    low: T,
    high: T,
}

extend Uniform[T] where T: SampleUniform {
    pub func new(low: T, high: T) -> Uniform[T]
    pub func sample[R: Rng](this, rng: mut ref R) -> T
}
```

### Bernoulli

Boolean distribution with given probability.

```tml
pub type Bernoulli {
    probability: F64,
}

extend Bernoulli {
    pub func new(probability: F64) -> Outcome[Bernoulli, Error]
    pub func sample[R: Rng](this, rng: mut ref R) -> Bool
}
```

## Platform Implementation

### OsRng

**Windows:**
```c
BCryptGenRandom(NULL, buffer, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG)
```

**Unix:**
```c
getrandom(buffer, len, 0)  // or read from /dev/urandom
```

## Example

```tml
use std::random::{random, random_range, shuffle, thread_rng}

func main() {
    // Random primitives
    let x: I32 = random()
    let y: F64 = random()
    let b: Bool = random()

    println("Random i32: {x}")
    println("Random f64: {y}")
    println("Random bool: {b}")

    // Random in range
    let dice = random_range(1, 7)  // 1-6
    println("Dice roll: {dice}")

    // Shuffle
    var cards = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    shuffle(mut ref cards)
    println("Shuffled: {cards}")

    // Reproducible sequence
    let mut rng = StdRng::seed_from_u64(42)
    for _ in 0 to 5 {
        println("Seeded: {rng.gen_range(0 to 100)}")
    }
}
```
