# Proposal: SIMD Optimization Across TML Runtime and Compiler

## Status: PROPOSED

## Why

The TML project currently has **1 file with explicit SIMD** (JSON parser, SSE2), **3 files with auto-vectorization-dependent loops** (distance functions, math), and **~8 files with pure scalar code** in performance-critical hot paths (strings, text, collections, hashing, lexer). Meanwhile, all external library calls (OpenSSL, BCrypt, Brotli, Zstd) already leverage hardware SIMD internally via FFI.

The gap is in **TML's own code**. Specifically:

1. **String search** (`str_find`, `tml_text_index_of`) uses naive O(n*m) scalar `memcmp` loops — SSE4.2 `PCMPISTRI` can achieve 10-100x speedup on long strings.
2. **Vector distance functions** (dot product, cosine similarity, L2 distance) are the hottest functions in HNSW search, called O(k * ef * neighbors) per query on 512-dim vectors — explicit AVX2 FMA would give 4-8x over auto-vectorized scalar loops.
3. **Case conversion** (`str_to_upper`, `str_to_lower`) processes one byte at a time via libc `toupper`/`tolower` — SSE2 range-mask arithmetic processes 16 bytes per cycle.
4. **Buffer operations** (`buffer_compare`, `buffer_fill`, `buffer_copy`) use hand-written scalar byte loops instead of `memcmp`/`memset`/`memcpy` or SIMD equivalents.
5. **Hash functions** (DJB2 for strings, FNV-1a for HashMap keys) are scalar byte-by-byte loops — AES-NI or CRC32C hashing achieves 4-8x throughput.
6. **Lexer hot paths** (whitespace skipping, identifier scanning, string body scanning) process one character at a time — SSE2 bulk character classification achieves 4-16x speedup on typical source files.

Every operation above runs on every TML program. String operations run on every string manipulation. Distance functions run on every doc search query. Lexer loops run on every compilation. The cumulative impact is significant.

### Current SIMD State

| Category | Files | Status |
|----------|-------|--------|
| Explicit SIMD (SSE2 intrinsics) | `json_fast_parser.hpp` | 1 file, production |
| Auto-vectorization (no guarantee) | `simd_distance.cpp`, `search.c`, `math.c` | 3 files, compiler-dependent |
| Indirect SIMD (via FFI libraries) | OpenSSL, BCrypt, Brotli, Zstd | ~15 files, hardware-accelerated |
| **No SIMD (scalar)** | `string.c`, `text.c`, `collections.c`, lexer, sort | **~8 files, zero acceleration** |

### Design Principles

1. **Portable first**: SSE2 baseline (guaranteed on all x86-64), AVX2 fast paths behind runtime CPUID detection
2. **Fallback always**: Every SIMD path has a scalar fallback via `#ifdef` or runtime dispatch
3. **Measure everything**: Each optimization includes a before/after benchmark comparing scalar, auto-vectorized, SSE2, and AVX2 implementations
4. **Minimal surface**: Use a thin `simd_utils.h` abstraction (not a heavy library), following the JSON parser pattern already in the codebase

## What Changes

### Phase 1: SIMD Infrastructure

Create the shared SIMD utilities and CPUID runtime detection used by all subsequent phases.

- `compiler/include/simd/simd_detect.hpp` — Runtime CPU feature detection (SSE2, SSE4.2, AVX2, AVX-512, AES-NI)
- `compiler/include/simd/simd_utils.h` — Portable macros and inline helpers for SSE2/AVX2/NEON intrinsics
- `compiler/include/simd/simd_charclass.h` — SIMD character classification lookup tables (whitespace, alpha, digit, identifier)
- Benchmark harness: `compiler/tests/bench/bench_simd.cpp` — Microbenchmark framework for before/after comparisons

### Phase 2: Vector Distance Functions (HNSW Search)

Replace auto-vectorization-dependent loops with explicit AVX2/FMA intrinsics.

- `dot_product_f32` — AVX2 `_mm256_fmadd_ps` with horizontal sum, 8 floats/cycle
- `cosine_similarity_f32` — Three AVX2 FMA accumulators in single loop
- `l2_distance_squared_f32` — AVX2 `_mm256_sub_ps` + `_mm256_fmadd_ps`
- `normalize_f32` / `norm_f32` — AVX2 `_mm256_mul_ps` bulk scaling
- `search_dot_product` (f64) — AVX2 `_mm256_fmadd_pd`, 4 doubles/cycle
- `search_cosine_similarity` (f64) — Three AVX2 FMA accumulators
- `search_euclidean_distance` (f64) — AVX2 sub + FMA
- Batch distance: compute distances to 4 neighbors simultaneously by interleaving vectors
- Embedding storage: flat array with stride-based indexing for cache locality

### Phase 3: String Operations

SIMD-accelerate the most frequent string operations in `string.c` and `text.c`.

- **String search** (`str_find`, `str_contains`, `tml_text_index_of`, `tml_text_contains`) — SSE4.2 `PCMPISTRI` for substring search, SSE2 `PCMPEQB` fallback
- **Reverse search** (`str_rfind`, `tml_text_last_index_of`) — SSE2 reverse scan with `PCMPEQB` + bitmask
- **Case conversion** (`str_to_upper/lower`, `tml_text_to_upper/lower`) — SSE2 range comparison (`cmpgt`/`cmplt` for 'a'-'z') + conditional add/sub 32
- **Trimming** (`str_trim`, `str_trim_start/end`, `tml_text_trim*`) — SSE2 multi-char whitespace comparison (`PCMPEQB` for space/tab/cr/lf, `OR` combined)
- **Character classification** (`char_is_alphabetic/numeric/whitespace/alphanumeric`) — SIMD lookup table for bulk classification (16 chars at a time)
- **String hashing** (`str_hash` DJB2) — Replace with CRC32C intrinsic (`_mm_crc32_u64`) or AES-NI-based hash for 8-16x throughput

### Phase 4: Collection Operations

Replace scalar byte loops in `collections.c` with SIMD or at minimum libc equivalents.

- **`buffer_compare`** — Replace scalar byte loop with `memcmp` + SSE2 `PCMPEQB` for equality testing
- **`buffer_fill`** — Replace scalar byte loop with `memset` + SSE2 `_mm_store_si128` for large fills
- **`buffer_copy`** — Replace scalar byte loop with `memcpy` + SSE2 bulk copy
- **`buffer_index_of`** — SSE2 `PCMPEQB` + `PMOVMSKB` for byte search (like `memchr`)
- **`buffer_last_index_of`** — SSE2 reverse scan
- **`buffer_swap16/32/64`** — SSSE3 `PSHUFB` for byte-order swapping, or `_byteswap_ulong`/`__builtin_bswap32` intrinsics
- **`buffer_concat`** — Replace scalar nested loop with `memcpy` per buffer
- **`hash_key`** (FNV-1a for HashMap) — Replace with AES-NI-based hash or CRC32C for better distribution and speed

### Phase 5: Lexer SIMD Acceleration

Apply SIMD to the compiler's lexer character-processing loops.

- **`skip_whitespace`** — SSE2 16-byte scan for non-whitespace (space/tab/cr), using `PCMPEQB` + `OR` + `MOVMASK`, with AVX2 32-byte fast path
- **`skip_line_comment`** — SSE2 scan for `'\n'` sentinel byte in 16-byte chunks
- **`skip_block_comment`** — SSE2 scan for `'*'` or `'/'` characters, scalar fallback at hit positions for `*/`/`/*` pair detection
- **`lex_identifier`** — SSE2 ASCII range checks (`[a-zA-Z0-9_]`) on 16 bytes, find first non-identifier byte
- **`lex_string` body scan** — SSE2 scan for sentinel set `{'"', '\n', '{', '}', '\\'}` in 16-byte chunks, bulk append non-special runs
- **`lex_raw_string` body scan** — SSE2 scan for `'"'` and `'\n'` only (2 sentinels)
- **`lex_template_literal` body scan** — SSE2 scan for sentinel set `` {'`', '{', '}', '\\'} ``
- **`lex_doc_comment` content** — SSE2 scan for `'\n'`, bulk `string::append` of entire run

### Phase 6: Math & Sort

SIMD for numeric array operations and sorting.

- **`simd_sum_i32`** — AVX2 `_mm256_add_epi32` with horizontal reduction
- **`simd_sum_f64`** — AVX2 `_mm256_add_pd` with horizontal reduction
- **`simd_dot_f64`** — AVX2 `_mm256_fmadd_pd` (duplicate of search, consolidate)
- **Sorting networks** — SIMD sorting networks for small arrays (4-16 elements) using `_mm_min_epi32`/`_mm_max_epi32`
- **Partition** — SIMD comparison + compress-store for quicksort partition step

### Phase 7: Benchmarks

Comprehensive benchmark suite comparing all implementations.

- Microbenchmarks per function: scalar vs SSE2 vs AVX2, across input sizes (16B, 256B, 4KB, 64KB, 1MB)
- End-to-end benchmarks: compilation speed (lexer), search latency (HNSW query), string-heavy program runtime
- Regression tracking: store benchmark baselines in `build/bench/` for CI comparison
- Platform matrix: Windows (MSVC), Linux (GCC/Clang), macOS (Apple Clang)

## Impact

- Affected specs: None (internal implementation, no language-level changes)
- Affected code: `compiler/runtime/text/`, `compiler/runtime/collections/`, `compiler/runtime/search/`, `compiler/runtime/math/`, `compiler/src/search/`, `compiler/src/lexer/`, `compiler/include/simd/` (new)
- Breaking change: NO (same C ABI, same function signatures, SIMD is internal)
- User benefit: Faster string operations (10-100x for search), faster doc search (4-8x), faster compilation (4-16x lexer), faster collections

## Dependencies

- LLVM headers for intrinsics (`immintrin.h`, `nmmintrin.h`) — already available via LLVM build
- MSVC intrinsics support — available on all supported MSVC versions
- No external libraries required — all intrinsics are compiler-provided

## Success Criteria

1. All SIMD paths have scalar fallbacks and compile on x86-64 (SSE2 baseline), ARM64 (NEON future), and WASM (scalar)
2. Runtime CPUID detection selects optimal code path without requiring recompilation
3. `dot_product_f32` achieves >=4x speedup over scalar on 512-dim vectors (AVX2)
4. `str_find` / `tml_text_index_of` achieves >=10x speedup over scalar on 4KB+ strings (SSE4.2)
5. `str_to_upper` / `str_to_lower` achieves >=8x speedup over scalar on 1KB+ strings (SSE2)
6. `buffer_compare` matches or exceeds libc `memcmp` performance
7. Lexer `skip_whitespace` achieves >=4x speedup on files with >30% whitespace (SSE2)
8. All benchmarks pass on Windows (MSVC x64), Linux (GCC x64), and macOS (Clang x64/ARM64)
9. No performance regressions for small inputs (<16 bytes) — scalar fallback must be cost-free
10. Benchmark results stored as baselines for CI regression detection

## Out of Scope

- ARM NEON implementations (future phase, after x86 SIMD is proven)
- WASM SIMD128 (requires WebAssembly target support in compiler)
- GPU compute / CUDA (out of scope for a compiler runtime)
- Auto-vectorization improvements in the TML compiler's MIR vectorization pass (separate task)
- Crypto SIMD (already handled by OpenSSL/BCrypt via FFI)
- Compression SIMD (already handled by Brotli/Zstd via FFI)

## References

- Intel Intrinsics Guide: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/
- simdjson SIMD patterns: https://github.com/simdjson/simdjson
- Rust `memchr` crate SIMD implementation
- TML JSON fast parser (`compiler/include/json/json_fast_parser.hpp`) — existing SSE2 reference in codebase
