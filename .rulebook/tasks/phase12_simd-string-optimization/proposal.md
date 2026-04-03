# Proposal: SIMD-Accelerated String Operations

## Why
TML string operations (contains, find, split, replace, to_lowercase) are all scalar byte-by-byte
loops. The infrastructure exists (core::simd with I8x16, SSE2 intrinsics, CPUID detection) but
none of it is used by the string library. Rust's std::str uses SIMD for memchr/contains and
processes 16-32 bytes per cycle vs TML's 1 byte per cycle — a 10-30x throughput gap on long strings.

## What Changes
- SIMD-accelerated implementations of core string operations using SSE2 (baseline x86-64)
- Optional AVX2 paths for wider 32-byte processing when detected at runtime
- Every SIMD function has a mandatory scalar fallback for non-x86 platforms (ARM, WASM, etc.)
- Runtime CPU feature detection via core::simd::detect (already implemented)
- String concat optimization: reuse buffer when LHS is a temporary

## Impact
- Affected code: lib/core/src/str/ (search, split, replace, transform, basic)
- Breaking change: NO (same API, faster implementation)
- User benefit: 5-20x faster string operations for contains/find/split on strings >64 bytes
