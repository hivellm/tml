# Proposal: Generic SIMD ISA Support for TML

## Status: PROPOSED

## Why

TML's current SIMD implementation has a **critical gap**: it only supports 128-bit vector types with generic LLVM IR operations (add/sub/mul/div) and exactly **2 native x86 intrinsics** (PCMPEQB, PMOVMSKB). This means:

1. **No SSE4.2 support** — No `PCMPISTRI`/`PCMPISTRM` for string search, no CRC32C for hashing, no POPCNT
2. **No AVX2 support** — No 256-bit vectors, no FMA (fused multiply-add), no gather/scatter
3. **No OSXSAVE/XGETBV** — No runtime detection of whether the OS has enabled AVX state saving (required before using ANY AVX instruction)
4. **No ARM NEON support** — Zero ARM intrinsics, blocking ARM64 targets (macOS Apple Silicon, Linux aarch64, Android)
5. **No CPU feature detection** — No CPUID-based runtime dispatch to select the best code path
6. **Only 128-bit types** — `I32x4`, `F32x4`, etc. exist but no `I32x8`, `F32x8` (AVX), or `I32x16`, `F32x16` (AVX-512)

The existing `simd-optimization` task focuses on **C++ runtime** SIMD (distance functions, strings, lexer). This task focuses on the **TML language level** — adding the compiler intrinsics, vector types, CPUID detection, and library APIs that let TML programs use SIMD natively.

### Current State

| Feature | Status | Gap |
|---------|--------|-----|
| 128-bit vector types (I32x4, F32x4, etc.) | 6 types | Only basic ops |
| Generic LLVM ops (add/sub/mul/div) | Working | No ISA-specific ops |
| SSE2 intrinsics | 2 (PCMPEQB, PMOVMSKB) | Need ~20 more |
| SSE4.2 intrinsics | 0 | Need CRC32, PCMPISTRI, POPCNT |
| AVX2 256-bit types | 0 | Need I32x8, F32x8, F64x4, I8x32 |
| AVX2 intrinsics | 0 | Need FMA, gather, hadd, permute |
| ARM NEON types | 0 | Need equivalent 128-bit types |
| ARM NEON intrinsics | 0 | Need vaddq, vmulq, vcmpq, etc. |
| CPUID / Feature detection | 0 | Need runtime dispatch |
| OSXSAVE / XGETBV | 0 | Required for safe AVX usage |

### Why This Matters

Without ISA-specific intrinsics at the TML level, users writing performance-critical code (search engines, parsers, crypto, compression, image processing) are forced to either:
- Use `@extern("c")` FFI to C libraries (defeats self-hosting goal)
- Rely on LLVM auto-vectorization (unreliable, no guarantees)
- Write scalar code and accept 4-16x performance penalty

## What Changes

### Phase 1: CPU Feature Detection Infrastructure

Runtime CPUID detection accessible from TML code.

**Compiler side** (`compiler/src/codegen/llvm/builtins/intrinsics.cpp`):
- `cpuid` intrinsic — Execute x86 CPUID instruction, return (eax, ebx, ecx, edx)
- `xgetbv` intrinsic — Read XCR0 register (needed for OSXSAVE check)

**Library side** (`lib/core/src/simd/detect.tml`):
- `has_sse2() -> Bool` — Always true on x86-64
- `has_sse42() -> Bool` — CPUID.1:ECX bit 20
- `has_popcnt() -> Bool` — CPUID.1:ECX bit 23
- `has_osxsave() -> Bool` — CPUID.1:ECX bit 27
- `has_avx() -> Bool` — OSXSAVE + XGETBV(0) bits 1:2 + CPUID.1:ECX bit 28
- `has_avx2() -> Bool` — has_avx() + CPUID.7.0:EBX bit 5
- `has_fma() -> Bool` — has_avx() + CPUID.1:ECX bit 12
- `has_neon() -> Bool` — Always true on AArch64 (compile-time)

**ARM detection** (`#if ARM64`):
- ARM NEON is mandatory on AArch64, so detection is compile-time only
- SVE detection via `mrs` instruction (future phase)

### Phase 2: SSE2 Complete Intrinsic Set

Expand from 2 intrinsics to full SSE2 coverage needed by the standard library.

**New intrinsics** (in `core::intrinsics`, guarded by `#if X86_64`):
- `sse2_cmpgt_epi8` — PCMPGTB (byte greater-than, critical for range checks)
- `sse2_cmplt_epi8` — via PCMPGTB with swapped args
- `sse2_cmpeq_epi16` — PCMPEQW
- `sse2_cmpeq_epi32` — PCMPEQD
- `sse2_cmpgt_epi16` — PCMPGTW
- `sse2_cmpgt_epi32` — PCMPGTD
- `sse2_and_si128` — PAND (bitwise AND, 128-bit)
- `sse2_or_si128` — POR (bitwise OR, 128-bit)
- `sse2_andnot_si128` — PANDN (bitwise AND-NOT)
- `sse2_min_epu8` — PMINUB (unsigned byte min)
- `sse2_max_epu8` — PMAXUB (unsigned byte max)
- `sse2_movemask_ps` — MOVMSKPS (float movemask, 4 bits)
- `sse2_movemask_pd` — MOVMSKPD (double movemask, 2 bits)
- `sse2_set1_epi8` — Broadcast byte (alias for simd_splat)
- `sse2_setzero_si128` — Zero vector
- `sse2_loadu_si128` — Unaligned 128-bit load (alias for simd_load_ptr)
- `sse2_storeu_si128` — Unaligned 128-bit store
- `sse2_packs_epi16` — PACKSSWB (pack i16 to i8 with saturation)
- `sse2_packus_epi16` — PACKUSWB (pack i16 to u8 with unsigned saturation)

### Phase 3: SSE4.2 Intrinsics

String comparison and CRC32 instructions.

**New intrinsics**:
- `sse42_cmpistrm` — PCMPISTRM (implicit-length string compare, return mask)
- `sse42_cmpistri` — PCMPISTRI (implicit-length string compare, return index)
- `sse42_cmpestrm` — PCMPESTRM (explicit-length string compare, return mask)
- `sse42_cmpestri` — PCMPESTRI (explicit-length string compare, return index)
- `sse42_crc32_u8` — CRC32C of byte
- `sse42_crc32_u16` — CRC32C of 16-bit
- `sse42_crc32_u32` — CRC32C of 32-bit
- `sse42_crc32_u64` — CRC32C of 64-bit

**Library wrappers** (`lib/core/src/simd/sse42.tml`):
- `str_find_sse42(haystack, needle) -> Maybe[I64]` — Use PCMPISTRI for substring search
- `crc32c(data: Slice[U8]) -> U32` — Optimized CRC32C hash

**POPCNT** (technically SSE4.2 era):
- `popcnt_u32(val: U32) -> U32` — Population count 32-bit
- `popcnt_u64(val: U64) -> U64` — Population count 64-bit

### Phase 4: AVX2 256-bit Types and Intrinsics

**New 256-bit vector types** (`lib/core/src/simd/`):
- `I8x32` — 32-lane I8 (256 bits)
- `U8x32` — 32-lane U8 (256 bits)
- `I16x16` — 16-lane I16 (256 bits)
- `I32x8` — 8-lane I32 (256 bits)
- `I64x4` — 4-lane I64 (256 bits)
- `F32x8` — 8-lane F32 (256 bits)
- `F64x4` — 4-lane F64 (256 bits)
- `Mask8` — 8-lane mask
- `Mask16` already exists, add `Mask32` for I8x32

**AVX2 intrinsics**:
- `avx2_add_epi8/16/32/64` — 256-bit integer add
- `avx2_sub_epi8/16/32/64` — 256-bit integer sub
- `avx2_mullo_epi16/32` — 256-bit integer low multiply
- `avx2_cmpeq_epi8/16/32` — 256-bit byte/word/dword compare
- `avx2_cmpgt_epi8/16/32` — 256-bit signed compare
- `avx2_and_si256` — 256-bit AND
- `avx2_or_si256` — 256-bit OR
- `avx2_xor_si256` — 256-bit XOR
- `avx2_movemask_epi8` — VPMOVMSKB (32-bit mask from 256-bit)
- `avx2_shuffle_epi8` — VPSHUFB (in-lane byte shuffle)
- `avx2_permute4x64_epi64` — VPERMQ (cross-lane 64-bit permute)
- `avx2_hadd_epi16/32` — VPHADDSW/VPHADDD
- `avx2_packs_epi16/32` — VPACKSSWB/VPACKSSDW
- `avx2_gather_epi32` — VPGATHERDD (gathered loads)
- `avx2_gather_epi64` — VPGATHERDQ
- `avx2_sllv_epi32/64` — Variable shift left
- `avx2_srlv_epi32/64` — Variable shift right

**FMA intrinsics** (requires FMA + AVX):
- `fma_fmadd_ps` — `_mm256_fmadd_ps` (a*b+c, 8 floats)
- `fma_fmadd_pd` — `_mm256_fmadd_pd` (a*b+c, 4 doubles)
- `fma_fmsub_ps/pd` — `_mm256_fmsub_ps/pd` (a*b-c)
- `fma_fnmadd_ps/pd` — `_mm256_fnmadd_ps/pd` (-a*b+c)

### Phase 5: ARM NEON Intrinsics

**Conditional compilation** (`#if ARM64`):

**Intrinsics** (matching SSE2 functionality for portable code):
- `neon_add_i8/16/32/64` — VADD (integer add)
- `neon_add_f32/f64` — FADD (float add)
- `neon_sub_i8/16/32/64` — VSUB
- `neon_mul_i8/16/32` — VMUL
- `neon_fmla_f32/f64` — VFMLA (fused multiply-add)
- `neon_ceq_i8/16/32` — VCEQ (compare equal)
- `neon_cgt_i8/16/32` — VCGT (compare greater than)
- `neon_and_v128` — VAND
- `neon_or_v128` — VORR
- `neon_bsl_v128` — VBSL (bitwise select)
- `neon_tbl1_i8` — VTBL1 (table lookup, like PSHUFB)
- `neon_cnt_i8` — VCNT (popcount per byte)
- `neon_addv_i8/16/32` — VADDV (horizontal sum)
- `neon_min_i8/16/32` — VMIN (lane-wise min)
- `neon_max_i8/16/32` — VMAX (lane-wise max)
- `neon_ld1_i8/16/32/64` — VLD1 (unaligned load)
- `neon_st1_i8/16/32/64` — VST1 (unaligned store)

### Phase 6: Portable SIMD Abstraction Layer

High-level portable API that dispatches to the best ISA at compile time.

**Module**: `lib/core/src/simd/portable.tml`

```tml
/// Portable SIMD operations that compile to the best available ISA.
/// On x86-64: SSE2/AVX2 depending on target features.
/// On ARM64: NEON.

pub behavior SimdVector[Self, Elem, const LANES: I32] {
    func splat(val: Elem) -> Self
    func load(ptr: ref Elem) -> Self
    func store(this, ptr: mut ref Elem)
    func add(this, other: Self) -> Self
    func sub(this, other: Self) -> Self
    func mul(this, other: Self) -> Self
    func eq(this, other: Self) -> Self
    func gt(this, other: Self) -> Self
    func and(this, other: Self) -> Self
    func or(this, other: Self) -> Self
    func movemask(this) -> I32
    func hsum(this) -> Elem
}
```

Implement this behavior for all SIMD types (I32x4, I32x8, F32x4, F32x8, etc.) with ISA-specific lowering.

### Phase 7: Library Algorithms Using SIMD

Use the new intrinsics to implement key algorithms in pure TML:

- `core::simd::str_search` — SIMD substring search (SSE4.2 PCMPISTRI with SSE2 fallback)
- `core::simd::memchr` — SIMD byte search (PCMPEQB + PMOVMSKB)
- `core::simd::case_convert` — SIMD case conversion (range check + conditional add)
- `core::simd::hash::crc32c` — Hardware CRC32C hashing
- `core::simd::distance` — SIMD dot product / cosine similarity (FMA)

### Phase 8: Tests

One test file per ISA tier:
- `lib/core/tests/simd/detect.test.tml` — Feature detection
- `lib/core/tests/simd/sse2_intrinsics.test.tml` — SSE2 ops
- `lib/core/tests/simd/sse42_intrinsics.test.tml` — SSE4.2 ops
- `lib/core/tests/simd/avx2_basic.test.tml` — AVX2 types + ops
- `lib/core/tests/simd/avx2_fma.test.tml` — FMA operations
- `lib/core/tests/simd/portable.test.tml` — Portable abstraction
- `lib/core/tests/simd/str_search.test.tml` — SIMD string search
- `lib/core/tests/simd/memchr.test.tml` — SIMD byte search

## Impact

- **Affected specs**: `docs/13-BUILTINS.md` (new intrinsics), `docs/04-TYPES.md` (new SIMD types)
- **Affected code**:
  - `compiler/src/codegen/llvm/builtins/intrinsics.cpp` — New intrinsic handlers (~1500 lines)
  - `lib/core/src/intrinsics.tml` — New intrinsic declarations (~200 lines)
  - `lib/core/src/simd/` — New type files + detect + portable layer (~3000 lines)
  - `lib/core/tests/simd/` — New test files (~1000 lines)
- **Breaking change**: NO (additive only — new types, new intrinsics)
- **User benefit**: TML programs can use hardware SIMD directly with 4-16x performance gains for string search, hashing, vector math, and data processing. Enables self-hosting of performance-critical code without C FFI.

## Dependencies

- LLVM 18+ (already available — provides all x86 and AArch64 intrinsics)
- Existing `@simd` annotation system (working)
- Existing `@intrinsic` system for registering new builtins (working)
- Conditional compilation (`#if X86_64`, `#if ARM64`) (working)

## Success Criteria

1. CPUID detection works and correctly identifies SSE4.2/AVX2/NEON on the host
2. OSXSAVE+XGETBV check prevents using AVX on systems where OS hasn't enabled YMM state
3. All 128-bit SSE2 intrinsics produce correct LLVM IR (verified via `--emit-ir`)
4. SSE4.2 PCMPISTRI-based string search matches scalar results on all test inputs
5. AVX2 256-bit types (I32x8, F32x8, etc.) produce `<8 x i32>`, `<8 x float>` LLVM IR
6. FMA intrinsics produce `llvm.fma.v8f32` / `llvm.fma.v4f64`
7. ARM NEON intrinsics compile to correct AArch64 instructions (cross-compile check)
8. Portable abstraction compiles to ISA-specific code on both x86-64 and ARM64
9. All test files pass on Windows x86-64 (primary platform)
10. No regressions in existing SIMD tests (9 files in lib/core/tests/simd/)

## Out of Scope

- AVX-512 (future phase — limited hardware support)
- ARM SVE/SVE2 (future phase — variable-length vectors)
- WASM SIMD128 (future phase — requires WebAssembly target)
- GPU/CUDA (out of scope entirely)
- Auto-vectorization improvements in TML's MIR pass (separate task)
- C++ runtime SIMD optimization (handled by `simd-optimization` task)

## References

- Intel Intrinsics Guide: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/
- ARM NEON Intrinsics Reference: https://developer.arm.com/architectures/instruction-sets/intrinsics/
- LLVM Language Reference — Vector Operations: https://llvm.org/docs/LangRef.html#vector-operations
- Existing TML SIMD code: `lib/core/src/simd/`, `lib/core/src/intrinsics.tml`
- Existing SSE2 intrinsic handlers: `compiler/src/codegen/llvm/builtins/intrinsics.cpp` (lines 1298-1540)
