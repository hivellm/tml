# Tasks: SIMD Optimization Across TML Runtime and Compiler

**Status**: Planning (0%)
**Priority**: High

## Phase 1: SIMD Infrastructure

> **Priority**: Critical | **Dir**: `compiler/include/simd/`

- [ ] 1.1 Create `simd_detect.hpp` — runtime CPUID detection for SSE2, SSE4.2, AVX2, AVX-512, AES-NI, POPCNT
- [ ] 1.2 Create `simd_utils.h` — portable macros (`TML_SSE2`, `TML_AVX2`, `TML_NEON`), alignment helpers, `SIMD_INLINE` attribute
- [ ] 1.3 Create `simd_charclass.h` — 256-byte lookup tables for whitespace, alpha, digit, identifier, hex character classification
- [ ] 1.4 Create benchmark harness `compiler/tests/bench/bench_simd.cpp` — timing framework with `rdtsc`/`QueryPerformanceCounter`, warm-up, statistical reporting (median, p95, stddev)
- [ ] 1.5 Add `--bench` flag to build scripts for benchmark compilation with appropriate optimization flags

## Phase 2: Vector Distance Functions (HNSW Search)

> **Priority**: Critical | **Files**: `compiler/src/search/simd_distance.cpp`, `compiler/runtime/search/search.c`

### 2.1 Float (f32) Distance — `simd_distance.cpp`

- [ ] 2.1.1 `dot_product_f32` (line 13) — AVX2 `_mm256_fmadd_ps`, 8 floats/cycle, horizontal sum via `_mm256_hadd_ps` + extract
- [ ] 2.1.2 `cosine_similarity_f32` (line 21) — three AVX2 FMA accumulators (`dot`, `norm_a`, `norm_b`) in single loop pass
- [ ] 2.1.3 `l2_distance_squared_f32` (line 43) — AVX2 `_mm256_sub_ps` + `_mm256_fmadd_ps`
- [ ] 2.1.4 `normalize_f32` (line 52) — AVX2 `_mm256_mul_ps` bulk scale with broadcast inverse
- [ ] 2.1.5 `norm_f32` (line 63) — AVX2 self-dot-product via `_mm256_fmadd_ps`
- [ ] 2.1.6 Add SSE2 fallback path for all f32 functions (4 floats/cycle)
- [ ] 2.1.7 Add `__restrict` qualifiers to all pointer parameters to enable auto-vectorization as secondary path

### 2.2 Double (f64) Distance — `search.c`

- [ ] 2.2.1 `search_dot_product` (line 34) — AVX2 `_mm256_fmadd_pd`, 4 doubles/cycle
- [ ] 2.2.2 `search_cosine_similarity` (line 46) — three AVX2 FMA accumulators for f64
- [ ] 2.2.3 `search_euclidean_distance` (line 67) — AVX2 `_mm256_sub_pd` + `_mm256_fmadd_pd`
- [ ] 2.2.4 `search_normalize` (line 90) — AVX2 `_mm256_mul_pd` bulk scale
- [ ] 2.2.5 `search_norm` (line 79) — AVX2 self-dot f64

### 2.3 HNSW Structural Optimizations — `hnsw_index.cpp`

- [ ] 2.3.1 Batch distance: compute distances to 4 neighbors simultaneously by interleaving vector loads
- [ ] 2.3.2 Embedding storage: replace per-node `std::vector<float>` (line 69 of `hnsw_index.hpp`) with flat `float*` array + stride indexing for cache locality
- [ ] 2.3.3 Ensure 32-byte alignment for embedding storage (`alignas(32)` or `_mm_malloc`)

### 2.4 Benchmarks — Distance Functions

- [ ] 2.4.1 Benchmark `dot_product_f32`: scalar vs SSE2 vs AVX2, dims = {64, 128, 256, 512, 1024}
- [ ] 2.4.2 Benchmark `cosine_similarity_f32`: same dimension sweep
- [ ] 2.4.3 Benchmark `l2_distance_squared_f32`: same dimension sweep
- [ ] 2.4.4 Benchmark `search_dot_product` (f64): same dimension sweep
- [ ] 2.4.5 Benchmark HNSW end-to-end query latency: 1K, 10K, 100K document index, top-10 search
- [ ] 2.4.6 Record baseline results in `build/bench/distance_baseline.json`

## Phase 3: String Operations

> **Priority**: High | **Files**: `compiler/runtime/text/string.c`, `compiler/runtime/text/text.c`

### 3.1 String Search (Highest Impact)

- [ ] 3.1.1 `str_find` (string.c:821) — SSE4.2 `PCMPISTRI` substring search with `_SIDD_CMP_EQUAL_ORDERED`, SSE2 fallback via `PCMPEQB` + first-byte filter
- [ ] 3.1.2 `str_contains` (string.c:400) — delegate to SIMD `str_find`, return bool
- [ ] 3.1.3 `str_rfind` (string.c:831) — SSE2 reverse scan: load 16 bytes from end, `PCMPEQB` first byte of needle, `MOVMASK` + `bsr` for last match position
- [ ] 3.1.4 `tml_text_index_of` (text.c:599) — replace naive O(n*m) `memcmp` loop with SSE4.2 `PCMPISTRI`, SSE2 fallback
- [ ] 3.1.5 `tml_text_last_index_of` (text.c:620) — SSE2 reverse scan matching `str_rfind` approach
- [ ] 3.1.6 `tml_text_contains` (text.c:664) — delegate to SIMD `tml_text_index_of`

### 3.2 Case Conversion

- [ ] 3.2.1 `str_to_upper` (string.c:426) — SSE2: load 16 bytes, `PCMPGTB`/`PCMPLTB` for range `['a','z']`, conditional `SUB 32` via `PAND` mask, store
- [ ] 3.2.2 `str_to_lower` (string.c:440) — SSE2: same approach with range `['A','Z']`, conditional `ADD 32`
- [ ] 3.2.3 `tml_text_to_upper` (text.c:672) — SSE2 bulk conversion, eliminate per-byte `tml_text_push` overhead
- [ ] 3.2.4 `tml_text_to_lower` (text.c:686) — SSE2 bulk conversion

### 3.3 Trimming & Whitespace

- [ ] 3.3.1 `str_trim` (string.c:454) — SSE2 multi-char whitespace test: `PCMPEQB` for `' '`, `'\t'`, `'\r'`, `'\n'`, `OR` all masks, `MOVMASK` + `tzcnt`/`lzcnt` to find first/last non-whitespace
- [ ] 3.3.2 `str_trim_start` (string.c:849) — SSE2 forward scan for first non-whitespace
- [ ] 3.3.3 `str_trim_end` (string.c:863) — SSE2 reverse scan for last non-whitespace
- [ ] 3.3.4 `tml_text_trim` (text.c:700) — SSE2 bidirectional whitespace scan
- [ ] 3.3.5 `tml_text_trim_start` (text.c:720) — SSE2 forward scan
- [ ] 3.3.6 `tml_text_trim_end` (text.c:737) — SSE2 reverse scan
- [ ] 3.3.7 `str_split_whitespace` (string.c:1002) — SSE2 bulk whitespace detection for boundary identification

### 3.4 String Hashing

- [ ] 3.4.1 `str_hash` DJB2 (string.c:336) — replace with CRC32C intrinsic (`_mm_crc32_u64`) processing 8 bytes/cycle, or AES-NI-based hash (`_mm_aesenc_si128`) for 16 bytes/cycle
- [ ] 3.4.2 Fallback: keep DJB2 for non-SSE4.2 platforms

### 3.5 String Comparison

- [ ] 3.5.1 `str_eq` (string.c:327) — add length-first check before `strcmp` to short-circuit length mismatches
- [ ] 3.5.2 `tml_text_equals` (text.c:939) — already has length check + `memcmp`; verify `memcmp` uses SIMD on target platforms

### 3.6 Benchmarks — String Operations

- [ ] 3.6.1 Benchmark `str_find`: scalar vs SSE2 vs SSE4.2, haystack = {64B, 1KB, 64KB, 1MB}, needle = {1, 4, 16, 64 bytes}
- [ ] 3.6.2 Benchmark `tml_text_index_of`: same matrix as `str_find`
- [ ] 3.6.3 Benchmark `str_to_upper`/`str_to_lower`: scalar vs SSE2, input = {16B, 256B, 4KB, 64KB}
- [ ] 3.6.4 Benchmark `str_trim`: scalar vs SSE2, input with 0%, 10%, 50%, 90% leading/trailing whitespace
- [ ] 3.6.5 Benchmark `str_hash`: DJB2 vs CRC32C vs AES-NI, key length = {4, 16, 64, 256, 1024 bytes}
- [ ] 3.6.6 Benchmark `str_split_whitespace`: scalar vs SSE2, input = {256B, 4KB, 64KB} text with varying word density
- [ ] 3.6.7 Record baseline results in `build/bench/string_baseline.json`

## Phase 4: Collection Operations

> **Priority**: High | **File**: `compiler/runtime/collections/collections.c`

### 4.1 Buffer Operations

- [ ] 4.1.1 `buffer_compare` (line 1099) — replace scalar byte loop with `memcmp`; add SSE2 `PCMPEQB` + `MOVMASK` fast path for equality testing
- [ ] 4.1.2 `buffer_fill` (line 1021) — replace scalar byte loop with `memset`; add SSE2 `_mm_store_si128` for large fills (>64 bytes)
- [ ] 4.1.3 `buffer_copy` (line 1036) — replace scalar byte loop with `memcpy`; add SSE2 `_mm_loadu_si128`/`_mm_storeu_si128` for non-overlapping copies
- [ ] 4.1.4 `buffer_index_of` (line 1133) — SSE2 `PCMPEQB` + `PMOVMSKB` + `tzcnt` for byte search (inline `memchr`)
- [ ] 4.1.5 `buffer_last_index_of` (line 1149) — SSE2 reverse scan with `PCMPEQB` + `PMOVMSKB` + `lzcnt`
- [ ] 4.1.6 `buffer_concat` (line 1327) — replace scalar nested loop with `memcpy` per source buffer

### 4.2 Byte Swap Operations

- [ ] 4.2.1 `buffer_swap16` (line 1175) — SSSE3 `PSHUFB` with 16-byte swap mask, or `_byteswap_ushort`/`__builtin_bswap16` per element
- [ ] 4.2.2 `buffer_swap32` (line 1188) — SSSE3 `PSHUFB` with 32-bit reversal mask, or `_byteswap_ulong`/`__builtin_bswap32`
- [ ] 4.2.3 `buffer_swap64` (line 1204) — SSSE3 `PSHUFB` or `_byteswap_uint64`/`__builtin_bswap64`, replace 4-iteration inner loop

### 4.3 HashMap Hashing

- [ ] 4.3.1 `hash_key` FNV-1a (line 216) — evaluate replacement with CRC32C for integer keys, or wyhash for better distribution
- [ ] 4.3.2 Benchmark hash distribution quality (collision rate) before and after hash function change

### 4.4 Benchmarks — Collections

- [ ] 4.4.1 Benchmark `buffer_compare`: scalar byte loop vs `memcmp` vs SSE2, sizes = {16B, 256B, 4KB, 64KB, 1MB}
- [ ] 4.4.2 Benchmark `buffer_fill`: scalar vs `memset` vs SSE2, same sizes
- [ ] 4.4.3 Benchmark `buffer_copy`: scalar vs `memcpy` vs SSE2, same sizes
- [ ] 4.4.4 Benchmark `buffer_index_of`: scalar vs SSE2, sizes = {64B, 1KB, 64KB}, byte at position {0%, 25%, 50%, 75%, 100%}
- [ ] 4.4.5 Benchmark `buffer_swap32`: scalar vs `bswap` intrinsic vs SSSE3, sizes = {64B, 1KB, 64KB}
- [ ] 4.4.6 Benchmark `hash_key`: FNV-1a vs CRC32C vs wyhash, throughput (keys/sec) and collision rate on 10K/100K keys
- [ ] 4.4.7 Record baseline results in `build/bench/collection_baseline.json`

## Phase 5: Lexer SIMD Acceleration

> **Priority**: Medium | **Dir**: `compiler/src/lexer/`

### 5.1 Whitespace & Comment Scanning

- [ ] 5.1.1 `skip_whitespace` (lexer_core.cpp:206-235) — SSE2: `PCMPEQB` for `' '`, `'\t'`, `'\r'`, `OR` masks, `MOVMASK` + `tzcnt` to find first non-whitespace; AVX2 32-byte fast path
- [ ] 5.1.2 `skip_line_comment` (lexer_core.cpp:344-352) — SSE2: `PCMPEQB` for `'\n'` in 16-byte chunks, `MOVMASK` + `tzcnt`
- [ ] 5.1.3 `skip_block_comment` (lexer_core.cpp:354-377) — SSE2: scan for `'*'` or `'/'` via `PCMPEQB` + `OR`, scalar fallback at hit positions for `*/`/`/*` detection

### 5.2 Identifier & String Scanning

- [ ] 5.2.1 `lex_identifier` (lexer_ident.cpp:27-29) — SSE2 ASCII range checks: `[a-z]` via `PCMPGTB`/`PCMPLTB`, `[A-Z]`, `[0-9]`, `'_'` via `PCMPEQB`; `OR` all ranges, `MOVMASK` + `tzcnt` for first non-identifier byte
- [ ] 5.2.2 `lex_string` body scan (lexer_string.cpp:77-126) — SSE2: `PCMPEQB` for 5 sentinel bytes `{'"', '\n', '{', '}', '\\'}`, `OR` all masks, `MOVMASK`; bulk `string::append` of safe run before first sentinel
- [ ] 5.2.3 `lex_raw_string` body scan (lexer_string.cpp:251-256) — SSE2: 2 sentinels only (`'"'`, `'\n'`)
- [ ] 5.2.4 `lex_template_literal` body scan (lexer_string.cpp:407-458) — SSE2: 4 sentinels (`` '`' ``, `'{'`, `'}'`, `'\\'`)
- [ ] 5.2.5 `lex_doc_comment` content (lexer_core.cpp:277-279) — SSE2: scan for `'\n'`, bulk `string::append` of entire comment line

### 5.3 Benchmarks — Lexer

- [ ] 5.3.1 Benchmark `skip_whitespace`: scalar vs SSE2 vs AVX2, input = {indentation-heavy, minimal-whitespace, tab-heavy} TML files
- [ ] 5.3.2 Benchmark `lex_identifier`: scalar vs SSE2, identifiers of length {4, 16, 64, 128} characters
- [ ] 5.3.3 Benchmark `lex_string`: scalar vs SSE2, string literals of length {16, 256, 4KB, 64KB}
- [ ] 5.3.4 Benchmark full lexer throughput: scalar vs SIMD-enhanced, on 10 representative TML files from `lib/core/` and `lib/std/`
- [ ] 5.3.5 Benchmark end-to-end compilation time: lex+parse+typecheck+codegen on `lib/core/src/str.tml` (largest module)
- [ ] 5.3.6 Record baseline results in `build/bench/lexer_baseline.json`

## Phase 6: Math & Sort

> **Priority**: Low | **Files**: `compiler/runtime/math/math.c`, `lib/core/src/slice/sort.tml`

### 6.1 Math Array Operations

- [ ] 6.1.1 `simd_sum_i32` (math.c:74) — AVX2 `_mm256_add_epi32` with horizontal reduction via `_mm256_hadd_epi32`
- [ ] 6.1.2 `simd_sum_f64` (math.c:83) — AVX2 `_mm256_add_pd` with horizontal reduction
- [ ] 6.1.3 `simd_dot_f64` (math.c:92) — consolidate with `search_dot_product`, use single AVX2 implementation
- [ ] 6.1.4 Deduplicate: remove `simd_dot_f64` in math.c, redirect to `search_dot_product` in search.c

### 6.2 Sorting (Pure TML — Future Consideration)

- [ ] 6.2.1 Investigate SIMD sorting networks for `MutSlice[I32].sort()` via compiler intrinsic or `lowlevel` block
- [ ] 6.2.2 Investigate median-of-three pivot selection to replace last-element pivot (sort.tml:125)
- [ ] 6.2.3 Investigate insertion sort fallback for partitions < 16 elements
- [ ] 6.2.4 Investigate introsort depth limit to prevent O(n^2) on sorted input

### 6.3 Benchmarks — Math & Sort

- [ ] 6.3.1 Benchmark `simd_sum_i32`: scalar vs AVX2, array size = {16, 256, 4K, 64K, 1M elements}
- [ ] 6.3.2 Benchmark `simd_sum_f64`: scalar vs AVX2, same sizes
- [ ] 6.3.3 Benchmark `sort` (TML quicksort): current Lomuto vs improved, array size = {100, 1K, 10K, 100K}, patterns = {random, sorted, reversed, few-unique}
- [ ] 6.3.4 Record baseline results in `build/bench/math_sort_baseline.json`

## Phase 7: Comprehensive Benchmark Suite & Regression Tracking

> **Priority**: Medium | **Dir**: `compiler/tests/bench/`

- [ ] 7.1 Create unified benchmark runner: `tml bench` command or `bench_all.cpp` that executes all phase benchmarks
- [ ] 7.2 Output format: JSON with `{function, variant, input_size, median_ns, p95_ns, speedup_vs_scalar}` per result
- [ ] 7.3 Comparison report: generate markdown table from two JSON baseline files (before vs after)
- [ ] 7.4 CI integration: run benchmarks on release builds, fail if any function regresses >10% vs baseline
- [ ] 7.5 Platform coverage: verify benchmarks run on Windows (MSVC x64), Linux (GCC x64), macOS (Clang ARM64)
- [ ] 7.6 Create `docs/SIMD.md` documenting supported ISA, fallback strategy, and benchmark results summary

## Validation

- [ ] V.1 All SIMD paths have scalar fallback and compile on x86-64 without AVX2 (`-mno-avx2`)
- [ ] V.2 `dot_product_f32` achieves >=4x speedup over scalar on 512-dim vectors (AVX2)
- [ ] V.3 `str_find` achieves >=10x speedup over scalar on 4KB+ haystack with 4-byte needle (SSE4.2)
- [ ] V.4 `tml_text_index_of` achieves >=10x speedup over current naive O(n*m) on 4KB+ text (SSE4.2)
- [ ] V.5 `str_to_upper` achieves >=8x speedup over scalar on 1KB+ input (SSE2)
- [ ] V.6 `buffer_compare` matches or exceeds libc `memcmp` performance on all tested sizes
- [ ] V.7 Lexer `skip_whitespace` achieves >=4x speedup on indentation-heavy files (SSE2)
- [ ] V.8 No performance regressions for small inputs (<16 bytes) — scalar path must be free
- [ ] V.9 Full test suite passes with SIMD-optimized code (`mcp__tml__test --no-cache`)
- [ ] V.10 Benchmark JSON baselines committed to `build/bench/` for CI tracking
- [ ] V.11 HNSW search latency improves >=3x on 10K document index with 512-dim embeddings
- [ ] V.12 End-to-end compilation time for `lib/core/` improves >=10% with lexer SIMD
