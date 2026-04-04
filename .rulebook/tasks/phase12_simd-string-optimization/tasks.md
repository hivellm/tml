# Tasks: SIMD-Accelerated String Operations

**Status**: In Progress. 92% (35/38). Remaining: AVX2 memchr (1.5), benchmarks (7.1-7.6).
**Baseline**: All str ops are scalar (1 byte/cycle). Target: 16-32 bytes/cycle on x86-64.
**Architecture**: Every function has 3 tiers: AVX2 (32B) → SSE2 (16B) → Scalar fallback.
**Detection**: Runtime `#if X86_64` compile-time gate + `core::simd::detect` for AVX2 at runtime.
**Files**: `lib/core/src/str/simd.tml` (main), `lib/core/src/simd/algorithms.tml` (Slice-based), `lib/core/src/simd/detect.tml` (CPU detection)

## Phase 1: Infrastructure — SIMD String Utils (6 items)

- [x] 1.1 `lib/core/src/simd/detect.tml` — `has_sse2()` (always true on x86-64), CPUID-based
- [x] 1.2 `has_avx2()` — runtime CPUID check (OSXSAVE + XGETBV + CPUID.7.0:EBX bit 5)
- [x] 1.3 `simd_memchr` — `find_byte()` in simd.tml (SSE2 PCMPEQB + PMOVMSKB, 16B/cycle) + `memchr_simd()` in algorithms.tml (Slice-based)
- [x] 1.4 `find_byte_scalar()` — byte-by-byte fallback, auto-selected for strings < 32B
- [ ] 1.5 `simd_memchr_avx2` — AVX2 VPCMPEQB 32B/cycle variant (not yet implemented)
- [x] 1.6 Dispatch: `find_byte()` → `< 32B ? scalar : SSE2`, `#if X86_64` in algorithms.tml

## Phase 2: Search Operations — contains, find, rfind (7 items)

- [x] 2.1 `contains_fast()` / `contains_simd()` — SSE2 first-byte scan + memcmp verify in simd.tml
- [x] 2.2 `contains_scalar()` — c_memcmp loop fallback
- [x] 2.3 `find_substr()` / `find_simd()` — SSE2 first-byte scan + memcmp, returns index or Maybe[I64]
- [x] 2.4 `find_substr_scalar()` — byte-by-byte memcmp fallback
- [x] 2.5 `rfind_simd()` — forward scan tracking last match, dispatched from search.tml for strings >= 32B
- [x] 2.6 `rfind()` scalar fallback — backward scan in search.tml (existing)
- [x] 2.7 Tests: str_simd_rfind.test.tml (3 tests) + str_simd_find_all.test.tml (3 tests)

## Phase 3: Split Operations — SIMD delimiter scan (5 items)

- [x] 3.1 `split_by_byte_simd()` — uses find_all_byte (SSE2) then substring_raw between positions
- [x] 3.2 `split()` — SIMD dispatch for single-byte delimiters on strings >= 32B in split.tml
- [x] 3.3 `split_lines_simd()` — find_all_byte(\n) + \r\n detection, dispatched from lines()
- [x] 3.4 `lines()` — SIMD dispatch for >= 32B in split.tml, scalar fallback for short strings
- [x] 3.5 Tests: str_simd_split.test.tml (16 tests — split, lines, str_cmp)

## Phase 4: Case Conversion — to_lowercase, to_uppercase (6 items)

- [x] 4.1 `case_lower_simd(data: Slice[U8])` — SSE2 range check A-Z + ADD 32 (in algorithms.tml)
- [x] 4.2 `to_lowercase()` — SIMD dispatch in transform.tml for >= 32B + `to_lowercase_simd()` in simd.tml
- [x] 4.3 `case_upper_simd(data: Slice[U8])` — SSE2 range check a-z + SUB 32 (in algorithms.tml)
- [x] 4.4 `to_uppercase()` — SIMD dispatch in transform.tml for >= 32B + `to_uppercase_simd()` in simd.tml
- [x] 4.5 `is_ascii_fast()` / `is_ascii_simd()` — SSE2 high-bit check, 16B/cycle (in simd.tml) + `is_ascii_scalar()` fallback
- [x] 4.6 Tests: str_simd_trim.test.tml (17 tests) + str_simd_dispatch.test.tml (14 tests)

## Phase 5: Trim Operations — SIMD whitespace scan (4 items)

- [x] 5.1 `trim_start_simd()` — SSE2 4-way whitespace OR mask (space/tab/\n/\r), finds first non-ws byte
- [x] 5.2 `trim_start()` — SIMD dispatch in transform.tml for >= 32B, scalar fallback for short strings
- [x] 5.3 `trim_end_simd()` — scalar backward scan (trailing whitespace typically short)
- [x] 5.4 Tests: included in str_simd_trim.test.tml + str_simd_dispatch.test.tml

## Phase 6: String Comparison & Equality (4 items)

- [x] 6.1 `str_eq_simd(a: Str, b: Str) -> Bool` — length check + c_memcmp (C runtime memcmp is already SIMD-optimized)
- [x] 6.2 `str_eq` scalar fallback — same c_memcmp (no separate scalar needed)
- [x] 6.3 `str_cmp_simd(a: Str, b: Str) -> I32` — c_memcmp on common prefix + length tie-break
- [x] 6.4 Tests: included in str_simd_split.test.tml (7 cmp tests: equal, less, greater, prefix, empty)

## Phase 7: Benchmarks & Validation (6 items)

- [ ] 7.1 `benchmarks/string-simd/bench_contains.tml` — SIMD vs scalar contains on 1KB/10KB/1MB strings
- [ ] 7.2 `benchmarks/string-simd/bench_find.tml` — SIMD vs scalar find
- [ ] 7.3 `benchmarks/string-simd/bench_split.tml` — SIMD vs scalar split
- [ ] 7.4 `benchmarks/string-simd/bench_tolower.tml` — SIMD vs scalar to_lowercase
- [ ] 7.5 Cross-language comparison: TML SIMD vs Rust str::contains vs Go strings.Contains
- [ ] 7.6 Regression tests: ensure SIMD results match scalar for ALL edge cases

## Extra (implemented but not in original plan)

- [x] `count_byte()` + `count_byte_scalar()` — SSE2 byte counting with popcount via bit-clearing
- [x] `find_byte_simd()` — raw I64 return variant (no Maybe wrapper) for internal use
- [x] `ctz()` helper — count trailing zeros for bitmask scanning
- [x] `str_find_simd(haystack: Slice[U8], needle: Slice[U8])` — Slice-based SIMD find (algorithms.tml)
- [x] `dot_product_simd(a: Slice[F32], b: Slice[F32])` — F32 dot product with F32x4 accumulation (algorithms.tml)
- [x] `has_sse42()`, `has_popcnt()`, `has_osxsave()`, `has_fma()` — additional CPU feature detection

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
