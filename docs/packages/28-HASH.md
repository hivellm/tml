# TML Standard Library: Hash

> `std::hash` — Fast non-cryptographic hash functions (FNV-1a, MurmurHash2).

## Overview

Provides non-cryptographic hash functions optimized for speed in hash tables, checksums, and ETags. Includes FNV-1a (32-bit and 64-bit) and MurmurHash2 (32-bit and 64-bit). Results are returned as `Hash32` or `Hash64` wrapper types with hex formatting.

**Not cryptographically secure.** For cryptographic hashing (SHA-256, BLAKE3, etc.), use `std::crypto::hash`.

## Import

```tml
use std::hash::{fnv1a32, fnv1a64, murmur2_64, etag_weak, Hash32, Hash64}
```

---

## Hash32

```tml
pub type Hash32 {
    value: U32,
}
```

### Methods

```tml
func raw(this) -> U32        // Raw hash value
func to_hex(this) -> Str     // Hex string (e.g., "a1b2c3d4")
func to_i64(this) -> I64     // Convert to I64
```

---

## Hash64

```tml
pub type Hash64 {
    value: U64,
}
```

### Methods

```tml
func raw(this) -> U64        // Raw hash value
func to_hex(this) -> Str     // Hex string (e.g., "a1b2c3d4e5f60718")
func to_i64(this) -> I64     // Convert to I64
```

---

## FNV-1a Hash Functions

```tml
func fnv1a32(input: Str) -> Hash32
func fnv1a32_bytes(input: ref Buffer) -> Hash32
func fnv1a64(input: Str) -> Hash64
func fnv1a64_bytes(input: ref Buffer) -> Hash64
```

## MurmurHash2 Functions

```tml
func murmur2_32(input: Str, seed: U32) -> Hash32
func murmur2_32_bytes(input: ref Buffer, seed: U32) -> Hash32
func murmur2_64(input: Str, seed: U64) -> Hash64
func murmur2_64_bytes(input: ref Buffer, seed: U64) -> Hash64
```

## ETag Convenience Functions

Generate HTTP ETag values from content.

```tml
func etag_weak(input: Str) -> Str              // W/"<hash>"
func etag_strong(input: Str) -> Str             // "<hash>"
func etag_weak_bytes(input: ref Buffer) -> Str  // W/"<hash>"
func etag_strong_bytes(input: ref Buffer) -> Str // "<hash>"
```

---

## Example

```tml
use std::hash::{fnv1a64, murmur2_64, etag_weak}

func main() {
    let data = "hello world"

    let h1 = fnv1a64(data)
    print("FNV-1a64:  {h1.to_hex()}\n")

    let h2 = murmur2_64(data, 0u64)
    print("Murmur2-64: {h2.to_hex()}\n")

    let tag = etag_weak(data)
    print("ETag: {tag}\n")
}
```
