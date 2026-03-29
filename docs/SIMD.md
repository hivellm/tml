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
| ARM NEON | v0-v31 | 128-bit | Portable stubs available (actual NEON on ARM64 target) |

## TML SIMD Intrinsics (Phase 5-02)

The TML standard library exposes SIMD intrinsics directly in `core::simd` and `core::runtime::intrinsics`.

### CPUID Detection API (`core::simd::detect`)

| Function | Description |
|----------|-------------|
| `has_sse2() -> Bool` | Always true on x86-64 |
| `has_sse42() -> Bool` | SSE4.2 support |
| `has_popcnt() -> Bool` | POPCNT instruction |
| `has_avx() -> Bool` | AVX support (OSXSAVE + XGETBV check) |
| `has_avx2() -> Bool` | AVX2 support |
| `has_fma() -> Bool` | FMA3 support |
| `has_neon() -> Bool` | ARM NEON (compile-time true on ARM64) |

### SSE2 Intrinsics (30 functions in `core::runtime::intrinsics`)

| Category | Functions | Count |
|----------|-----------|-------|
| Comparison | `sse2_cmpeq_epi8/16/32`, `sse2_cmpgt_epi8/16/32`, `sse2_cmplt_epi8` | 7 |
| Bitwise | `sse2_and/or/xor/andnot_si128` | 4 |
| Min/Max | `sse2_min/max_epu8`, `sse2_min/max_epi16` | 4 |
| Movemask | `sse2_movemask_ps/pd`, `sse2_movemask_epi8` | 3 |
| Pack/Unpack | `sse2_packs/packus_epi16`, `sse2_packs_epi32`, `sse2_unpacklo/hi_epi8` | 5 |
| Shift | `sse2_slli/srli/srai_epi16/32/64` | 5 |
| Memory | `sse2_storeu/store_si128` | 2 |

### SSE4.2 Intrinsics (10 functions)

| Category | Functions | Count |
|----------|-----------|-------|
| String comparison | `sse42_cmpistrm/cmpistri/cmpestrm/cmpestri` | 4 |
| CRC32 | `sse42_crc32_u8/u16/u32/u64` | 4 |
| POPCNT | `popcnt_u32/u64` | 2 |

### AVX2 Intrinsics (13+ functions)

| Category | Functions | Count |
|----------|-----------|-------|
| Comparison | `avx2_cmpeq/cmpgt_epi8/16/32` | 6 |
| Bitwise | `avx2_and/or/xor_si256` | 3 |
| Movemask | `avx2_movemask_epi8` | 1 |
| Shuffle | `avx2_shuffle_epi8`, `avx2_permute4x64_epi64`, `avx2_permute2x128_si256` | 3 |

### 256-bit Vector Types

| Type | Lanes | Element | Module |
|------|-------|---------|--------|
| `I32x8` | 8 | I32 | `core::simd::i32x8` |
| `F32x8` | 8 | F32 | `core::simd::f32x8` |
| `I64x4` | 4 | I64 | `core::simd::i64x4` |
| `F64x4` | 4 | F64 | `core::simd::f64x4` |
| `I8x32` | 32 | I8 | `core::simd::i8x32` |

All types support: `new`, `splat`, `zero`, `get`, `set`, `add`, `sub`, `mul`, `sum`, `hmin`, `hmax`, `to_string`, `debug_string`.
Integer types also support: `band`, `bor`, `bxor`, `shift_left`, `shift_right`, `min`, `max`, `product`.
Cross-type conversions: `I32x8.to_f32x8()`, `F32x8.to_i32x8()`, `I64x4.to_f64x4()`, `F64x4.to_i64x4()`.

### ARM NEON Stubs (`core::simd::neon`)

Portable NEON-like API for cross-platform code. On x86-64, delegates to 128-bit SSE operations.

| Function | NEON Equivalent |
|----------|----------------|
| `neon_add/sub/mul_i32` | VADD/VSUB/VMUL.4S |
| `neon_add/sub/mul_f32` | FADD/FSUB/FMUL.4S |
| `neon_addv/maxv/minv_i32` | VADDV/VMAXV/VMINV |
| `neon_and/or/xor_i32` | VAND/VORR/VEOR |
| `neon_ceq/cgt_i32` | VCEQ/VCGT |

### Usage Example

```tml
use core::simd::detect::has_avx2
use core::simd::i32x8::I32x8
use core::simd::i32x4::I32x4

func sum_array(data: I32, count: I32) -> I32 {
    if has_avx2() {
        // Process 8 elements at a time with AVX2
        let acc = I32x8::splat(0)
        // ... vectorized loop ...
        return acc.sum()
    }
    // Fallback: 4 elements at a time with SSE2
    let acc = I32x4::splat(0)
    // ... vectorized loop ...
    return acc.sum()
}
```

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

## Phase 4.6-4.9: Advanced AVX2 + FMA Intrinsics

### Horizontal & Pack
| Intrinsic | LLVM | Description |
|-----------|------|-------------|
| `avx2_hadd_epi16/32` | `@llvm.x86.avx2.phadd.w/d` | Horizontal add adjacent pairs |
| `avx2_packs_epi16/32` | `@llvm.x86.avx2.packsswb/packssdw` | Pack with signed saturation |
| `avx2_packus_epi16/32` | `@llvm.x86.avx2.packuswb/packusdw` | Pack with unsigned saturation |

### Gather (via `llvm.masked.gather`)
| Intrinsic | LLVM | Description |
|-----------|------|-------------|
| `avx2_gather_epi32` | `@llvm.masked.gather.v8i32` | Gather 8 x I32 by index |
| `avx2_gather_epi64` | `@llvm.masked.gather.v4i64` | Gather 4 x I64 by index |
| `avx2_gather_ps` | `@llvm.masked.gather.v8f32` | Gather 8 x F32 by index |

### Variable Shift
| Intrinsic | LLVM | Description |
|-----------|------|-------------|
| `avx2_sllv_epi32/64` | `shl <N x iM>` | Per-lane variable shift left |
| `avx2_srlv_epi32/64` | `lshr <N x iM>` | Per-lane variable shift right |

### FMA (Fused Multiply-Add)
| Intrinsic | LLVM | Description |
|-----------|------|-------------|
| `fma_fmadd_ps/pd` | `@llvm.fma.v8f32/v4f64` | a*b + c |
| `fma_fmsub_ps/pd` | `fneg + @llvm.fma` | a*b - c |
| `fma_fnmadd_ps/pd` | `fneg + @llvm.fma` | -a*b + c |
| `fma_fmadd_ss/sd` | `@llvm.fma.f32/f64` | Scalar FMA |

## Library Algorithms (`core::simd::algorithms`)

| Function | Description |
|----------|-------------|
| `memchr_simd(haystack, byte)` | SSE2 byte search (PCMPEQB + PMOVMSKB) |
| `str_find_simd(haystack, needle)` | SIMD substring search |
| `case_upper_simd(data)` | ASCII lowercase to uppercase |
| `case_lower_simd(data)` | ASCII uppercase to lowercase |
| `crc32c_simd(data)` | Hardware CRC32C (delegates to sse42::crc32c) |
| `dot_product_simd(a, b)` | F32 dot product with F32x4 accumulation |

## Architecture Notes

- All SIMD code is C++ compiled with appropriate ISA flags (`-mavx2 -mfma` or `/arch:AVX2`)
- Runtime dispatch uses `simd_detect.hpp` cached CPUID queries
- `extern "C"` linkage for functions called from TML via FFI
- The sort improvements (introsort) are in pure TML, not SIMD — they use algorithmic optimization rather than data parallelism
- HNSW search uses flat embedding storage with 32-byte aligned arrays for optimal AVX2 loads
- Function attributes include `"target-features"="+sse2,+sse4.2,+avx,+avx2,+fma"` for LLVM ISA intrinsic selection
- LLVM 23 removed old `@llvm.x86.avx2.gather.*` intrinsics — gather uses `@llvm.masked.gather` instead
