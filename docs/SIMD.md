# SIMD Optimization Guide

This document covers the SIMD (Single Instruction, Multiple Data) optimizations in the TML compiler and runtime. These optimizations accelerate vector math, string operations, collection operations, lexer scanning, and array math using x86-64 SIMD instruction sets.

## Supported ISA

| ISA | Registers | Width | Used For |
|-----|-----------|-------|----------|
| SSE2 | xmm0-xmm15 | 128-bit | String search/trim/case, fallback for all ops |
| SSE4.2 | xmm0-xmm15 | 128-bit | PCMPESTRI substring search, CRC32C hashing |
| AVX2 | ymm0-ymm15 | 256-bit | Distance functions, math sum, batch operations |
| AVX2+FMA | ymm0-ymm15 | 256-bit | Dot product, cosine similarity (fused multiply-add) |
| AVX-512 | zmm0-zmm31 | 512-bit | Detected but not yet used (future) |
| ARM NEON | v0-v31 | 128-bit | Planned (not yet implemented) |

## Runtime Dispatch

All SIMD functions use runtime CPUID detection via `simd_detect.hpp`. The dispatch pattern is:

```cpp
if (has_avx2() && has_fma()) {
    // AVX2+FMA path: 8 floats or 4 doubles per cycle
} else if (has_sse2()) {
    // SSE2 path: 4 floats or 2 doubles per cycle
} else {
    // Scalar fallback: portable, no SIMD
}
```

CPUID results are cached in static booleans (one-time cost per process). The detection overhead is negligible (~1 ns per cached query).

## Fallback Strategy

Every SIMD function has a complete scalar fallback that produces identical results. The fallback chain is:

```
AVX2+FMA (if available)
  |
  v
SSE2 (guaranteed on x86-64)
  |
  v
Scalar (portable, always available)
```

Compile-time macros (`TML_SSE2`, `TML_AVX2`, etc.) guard intrinsic includes. Runtime checks (`has_avx2()`) gate the fast paths.

## Functions by Phase

### Phase 1: Infrastructure

| File | Contents |
|------|----------|
| `compiler/include/simd/simd_detect.hpp` | Runtime CPUID detection (SSE2, SSE4.2, AVX2, AVX-512, AES-NI, FMA, BMI1/2) |
| `compiler/include/simd/simd_utils.h` | Compile-time ISA macros, alignment helpers, `SIMD_INLINE`, aligned alloc/free |
| `compiler/include/simd/simd_charclass.h` | 256-byte lookup tables for character classification (whitespace, alpha, digit, hex, identifier) |
| `compiler/tests/bench/bench_simd.cpp` | Comprehensive benchmark harness with JSON output |

### Phase 2: Vector Distance Functions

**File**: `compiler/src/search/simd_distance.cpp`
**Header**: `compiler/include/search/simd_distance.hpp`

| Function | Description | AVX2 Throughput | SSE2 Throughput |
|----------|-------------|-----------------|-----------------|
| `dot_product_f32` | Inner product | 8 floats/cycle (FMA) | 4 floats/cycle |
| `cosine_similarity_f32` | Normalized dot product | 8 floats/cycle (3 accumulators) | 4 floats/cycle |
| `l2_distance_squared_f32` | Squared L2 distance | 8 floats/cycle (sub+FMA) | 4 floats/cycle |
| `euclidean_distance_f32` | L2 distance | 8 floats/cycle + sqrt | 4 floats/cycle + sqrt |
| `normalize_f32` | Unit length normalization | 8 floats/cycle | 4 floats/cycle |
| `norm_f32` | Vector magnitude | 8 floats/cycle + sqrt | 4 floats/cycle + sqrt |
| `dot_product_f64` | Double inner product | 4 doubles/cycle (FMA) | 2 doubles/cycle |
| `cosine_similarity_f64` | Double cosine similarity | 4 doubles/cycle | 2 doubles/cycle |
| `euclidean_distance_f64` | Double L2 distance | 4 doubles/cycle | 2 doubles/cycle |
| `normalize_f64` | Double normalization | 4 doubles/cycle | 2 doubles/cycle |
| `norm_f64` | Double magnitude | 4 doubles/cycle | 2 doubles/cycle |
| `batch_l2_squared_f32_x4` | 4 distances simultaneously | Interleaved AVX2 loads | N/A |

### Phase 3: String Operations

**File**: `compiler/src/simd/simd_string.cpp`
**Header**: `compiler/include/simd/simd_string.h`

| Function | Description | ISA | Technique |
|----------|-------------|-----|-----------|
| `simd_str_find` | Forward substring search | SSE4.2 / SSE2 | PCMPESTRI (needle <= 16B) or PCMPEQB first-byte filter |
| `simd_str_rfind` | Reverse substring search | SSE2 | PCMPEQB + BitScanReverse from end |
| `simd_str_contains` | Substring existence check | SSE4.2 / SSE2 | Delegates to `simd_str_find` |
| `simd_to_upper` | ASCII to uppercase | SSE2 | Range check [a-z] via unsigned compare trick, conditional SUB 32 |
| `simd_to_lower` | ASCII to lowercase | SSE2 | Range check [A-Z] via unsigned compare trick, conditional ADD 32 |
| `simd_trim_start` | Find first non-whitespace | SSE2 | PCMPEQB for 4 ws chars, OR masks, MOVMASK + tzcnt |
| `simd_trim_end` | Find last non-whitespace | SSE2 | PCMPEQB reverse scan, MOVMASK + BitScanReverse |
| `simd_str_hash` | String hashing | SSE4.2 | CRC32C via `_mm_crc32_u64` (8 bytes/cycle), FNV-1a fallback |
| `simd_str_eq` | String equality | libc | Length check + pointer identity + `memcmp` |

### Phase 4: Collection Operations

| Function | Description | Technique |
|----------|-------------|-----------|
| `buffer_compare` | Buffer comparison | Delegates to libc `memcmp` (SIMD internally) |
| `buffer_fill` | Buffer fill | Delegates to `memset` intrinsic |
| `buffer_copy` | Buffer copy | `copy_nonoverlapping` intrinsic |
| `buffer_index_of` | Byte search | Delegates to libc `memchr` (SIMD internally) |
| `buffer_last_index_of` | Reverse byte search | SSE2 reverse scan in `buffer_simd.c` |
| `buf_bswap16/32/64` | Byte swap | `__builtin_bswap` / `_byteswap` intrinsics |

### Phase 5: Lexer SIMD Acceleration

**File**: `compiler/src/lexer/lexer.cpp`

| Function | Description | Technique |
|----------|-------------|-----------|
| `skip_whitespace` | Skip whitespace runs | SSE2 PCMPEQB for ' ', '\t', '\r' |
| `skip_line_comment` | Skip `//` comments | SSE2 PCMPEQB for '\n' |
| `skip_block_comment` | Skip `/* */` comments | SSE2 PCMPEQB for '*' and '/', nesting detection |
| `lex_identifier` | Scan identifier chars | SSE2 range checks [a-z], [A-Z], [0-9], '_' |
| `lex_string` | Scan string body | SSE2 PCMPEQB for 5 sentinels, bulk append |
| `lex_raw_string` | Scan raw string body | SSE2 PCMPEQB for 2 sentinels |
| `lex_template_literal` | Scan template literal | SSE2 PCMPEQB for 5 sentinels |
| `lex_doc_comment` | Scan doc comment body | SSE2 PCMPEQB for '\n' |

### Phase 6: Math & Sort

**Math File**: `compiler/src/simd/simd_math.cpp`
**Math Header**: `compiler/include/simd/simd_math.h`

| Function | Description | AVX2 Throughput | SSE2 Throughput |
|----------|-------------|-----------------|-----------------|
| `simd_sum_i32` | Sum int32 array | 8 ints/cycle | 4 ints/cycle |
| `simd_sum_f64` | Sum double array | 4 doubles/cycle | 2 doubles/cycle |
| `simd_dot_f64` | Double dot product | Delegates to `dot_product_f64` | Same |

**Sort File**: `lib/core/src/slice/sort.tml`

| Improvement | Description |
|-------------|-------------|
| Median-of-three pivot | Selects median of {low, mid, high} to prevent O(n^2) on sorted inputs |
| Insertion sort fallback | Uses insertion sort for partitions < 16 elements |
| Introsort depth limit | Switches to heapsort at depth 2 * floor(log2(n)) for guaranteed O(n log n) |

## Running Benchmarks

Build and run the benchmark suite:

```bash
# Build with benchmarks
scripts\build.bat --bench

# Run all benchmarks
build/debug/bin/tml_bench.exe

# Run with custom JSON output path
build/debug/bin/tml_bench.exe --json path/to/output.json
```

Default JSON output is written to `build/bench/results.json`.

### JSON Output Format

Each benchmark result is a JSON object:

```json
{
  "function": "dot_product_f32_512",
  "category": "distance",
  "input_size": 512,
  "median_ns": 42.0,
  "p95_ns": 55.0,
  "min_ns": 38.0,
  "max_ns": 120.0,
  "stddev_ns": 8.5,
  "iterations": 50000
}
```

### Comparing Results

To compare two benchmark runs (e.g., before and after an optimization), save the JSON output from each run and compare median_ns values per function.

## Adding New SIMD Functions

1. Create implementation in `compiler/src/simd/` with scalar + SSE2 + AVX2 paths
2. Add header in `compiler/include/simd/` with `extern "C"` linkage
3. Add library target in `compiler/CMakeLists.txt` (see `tml_simd_math` as template)
4. Add benchmarks in `compiler/tests/bench/bench_simd.cpp`
5. Verify scalar fallback compiles without AVX2 (`#if TML_AVX2` guards)

## Architecture Notes

- All SIMD code is C++ compiled with appropriate ISA flags (`-mavx2 -mfma` or `/arch:AVX2`)
- Runtime dispatch uses `simd_detect.hpp` cached CPUID queries
- `extern "C"` linkage for functions called from TML via FFI
- The sort improvements (introsort) are in pure TML, not SIMD — they use algorithmic optimization rather than data parallelism
- HNSW search uses flat embedding storage with 32-byte aligned arrays for optimal AVX2 loads
