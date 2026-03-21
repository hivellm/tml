# Swiss Table-style Control Bytes in HashMap

**Category**: stdlib
**Tags**: stdlib, collections, hashmap, performance

## Description

HashMap[K, V] uses open-addressing with Swiss Table-inspired control bytes for probe metadata. FNV-1a hashing. Implemented in pure TML using lowlevel memory intrinsics. The control byte array enables fast SIMD-friendly probing (though actual SIMD probing is not yet used).

## When to Use

When working with or extending the HashMap implementation. Understanding the control byte layout is essential for debugging hash collisions or performance issues.
