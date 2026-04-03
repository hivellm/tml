# Tasks: SIMD-Accelerated String Operations

**Status**: Planning. 0% (0/38).
**Baseline**: All str ops are scalar (1 byte/cycle). Target: 16-32 bytes/cycle on x86-64.
**Architecture**: Every function has 3 tiers: AVX2 (32B) → SSE2 (16B) → Scalar fallback.
**Detection**: Runtime `#if X86_64` compile-time gate + `core::simd::detect` for AVX2 at runtime.

## Phase 1: Infrastructure — SIMD String Utils (6 items)

- [ ] 1.1 `lib/core/src/str/simd_utils.tml` — `is_sse2_available() -> Bool` (always true on x86-64)
- [ ] 1.2 `is_avx2_available() -> Bool` — runtime check via `core::simd::detect::has_avx2()`
- [ ] 1.3 `simd_memchr(haystack: Str, byte: U8) -> Maybe[I64]` — SSE2 PCMPEQB scan, 16 bytes/cycle
- [ ] 1.4 `simd_memchr` scalar fallback — byte-by-byte loop, used on ARM/WASM/non-x86
- [ ] 1.5 `simd_memchr_avx2(haystack: Str, byte: U8) -> Maybe[I64]` — AVX2 VPCMPEQB, 32 bytes/cycle
- [ ] 1.6 `simd_memchr_dispatch` — auto-selects AVX2 → SSE2 → scalar based on CPU

## Phase 2: Search Operations — contains, find, rfind (7 items)

- [ ] 2.1 `str/search.tml` — Rewrite `contains()` with SIMD first-byte scan + memcmp verify
- [ ] 2.2 `contains()` scalar fallback — current c_memcmp loop (keep as-is for fallback)
- [ ] 2.3 `find()` — SIMD scan for first byte of pattern, then memcmp verify at each hit
- [ ] 2.4 `find()` scalar fallback — current byte-by-byte implementation
- [ ] 2.5 `rfind()` — reverse SIMD scan (process from end, 16 bytes at a time)
- [ ] 2.6 `rfind()` scalar fallback
- [ ] 2.7 Tests: contains/find/rfind with strings of 0, 1, 15, 16, 17, 31, 32, 33, 100, 10000 bytes

## Phase 3: Split Operations — SIMD delimiter scan (5 items)

- [ ] 3.1 `str/split.tml` — SIMD single-byte delimiter scan (split by char)
- [ ] 3.2 `split()` scalar fallback
- [ ] 3.3 `split_lines()` — SIMD newline scan (\n and \r\n detection in 16-byte chunks)
- [ ] 3.4 `split_lines()` scalar fallback
- [ ] 3.5 Tests: split on various delimiters, edge cases (empty, trailing, consecutive)

## Phase 4: Case Conversion — to_lowercase, to_uppercase (6 items)

- [ ] 4.1 `str/transform.tml` — `to_lowercase_simd()` — SSE2 range check A-Z + ADD 32
- [ ] 4.2 `to_lowercase()` scalar fallback — current byte-by-byte
- [ ] 4.3 `to_uppercase_simd()` — SSE2 range check a-z + SUB 32
- [ ] 4.4 `to_uppercase()` scalar fallback
- [ ] 4.5 Handle ASCII-only fast path (check all bytes < 128 with SIMD, fallback to scalar for UTF-8)
- [ ] 4.6 Tests: ASCII strings, mixed case, empty, already lowercase/uppercase

## Phase 5: Trim Operations — SIMD whitespace scan (4 items)

- [ ] 5.1 `str/transform.tml` — `trim_start_simd()` — SSE2 whitespace mask (space/tab/\n/\r)
- [ ] 5.2 `trim_start()` scalar fallback
- [ ] 5.3 `trim_end_simd()` — reverse scan for last non-whitespace
- [ ] 5.4 Tests: trim with various whitespace patterns, all-whitespace, no-whitespace

## Phase 6: String Comparison & Equality (4 items)

- [ ] 6.1 `str_eq_simd(a: Str, b: Str) -> Bool` — length check + SIMD memcmp (16 bytes/cycle)
- [ ] 6.2 `str_eq` scalar fallback — c_memcmp
- [ ] 6.3 `str_cmp_simd(a: Str, b: Str) -> I32` — lexicographic compare with SIMD prefix scan
- [ ] 6.4 Tests: equal strings, different lengths, differ at byte 0/15/16/17/31/32

## Phase 7: Benchmarks & Validation (6 items)

- [ ] 7.1 `benchmarks/string-simd/bench_contains.tml` — SIMD vs scalar contains on 1KB/10KB/1MB strings
- [ ] 7.2 `benchmarks/string-simd/bench_find.tml` — SIMD vs scalar find
- [ ] 7.3 `benchmarks/string-simd/bench_split.tml` — SIMD vs scalar split
- [ ] 7.4 `benchmarks/string-simd/bench_tolower.tml` — SIMD vs scalar to_lowercase
- [ ] 7.5 Cross-language comparison: TML SIMD vs Rust str::contains vs Go strings.Contains
- [ ] 7.6 Regression tests: ensure SIMD results match scalar for ALL edge cases

## Architecture Notes

### Dispatch Pattern (MANDATORY for all functions)
```tml
pub func contains(s: Str, pattern: Str) -> Bool {
    #if X86_64
    // Compile-time: x86-64 gets SIMD path
    return contains_simd(s, pattern)
    #else
    // Fallback: ARM, WASM, other architectures get scalar
    return contains_scalar(s, pattern)
    #endif
}

func contains_simd(s: Str, pattern: Str) -> Bool {
    let s_len = len(s)
    let p_len = len(pattern)
    // Short strings: scalar is faster (no SIMD setup cost)
    if s_len < 32 { return contains_scalar(s, pattern) }
    // SIMD: scan for first byte of pattern using PCMPEQB
    // On each hit, verify with memcmp
    ...
}

func contains_scalar(s: Str, pattern: Str) -> Bool {
    // Current implementation — always works on any CPU
    ...
}
```

### Runtime AVX2 Detection
```tml
use core::simd::detect

func contains_simd(s: Str, pattern: Str) -> Bool {
    if s_len >= 64 and detect::has_avx2() {
        return contains_avx2(s, pattern)  // 32 bytes/cycle
    }
    return contains_sse2(s, pattern)      // 16 bytes/cycle
}
```

### Short-String Threshold
- Strings < 32 bytes: scalar is faster (SIMD setup > savings)
- Strings 32-127 bytes: SSE2 (16B/cycle) wins
- Strings ≥ 128 bytes: AVX2 (32B/cycle) wins, if available
- These thresholds should be tuned via benchmarks (Phase 7)

### Mandatory Scalar Fallback Rules
1. EVERY SIMD function MUST have a `_scalar` variant
2. The public API function uses `#if X86_64` to select SIMD vs scalar at compile time
3. The SIMD variant MUST fallback to scalar for strings shorter than 32 bytes
4. ARM NEON variants are out of scope for now (future task)
5. WASM SIMD is out of scope for now
6. ALL scalar fallbacks must produce IDENTICAL results to SIMD variants
7. Phase 7.6 regression tests verify SIMD == scalar for every edge case
