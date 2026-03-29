# Tasks: Generic SIMD ISA Support for TML

**Status**: Phase 1 Complete (Phase 1: 100%)
**Priority**: High

## Phase 1: CPU Feature Detection Infrastructure

> **Priority**: Critical | **Files**: `compiler/runtime/core/essential.c`, `lib/core/src/runtime/intrinsics.tml`, `lib/core/src/simd/detect.tml`

- [x] 1.1 Add CPUID C runtime helpers — `tml_cpuid_eax/ebx/ecx/edx(leaf, subleaf) -> I32` in `essential.c`
- [x] 1.2 Add XGETBV C runtime helper — `tml_xgetbv(xcr_index) -> I64` in `essential.c`
- [x] 1.3 Declare `tml_cpuid_*` and `tml_xgetbv` as `@extern("c")` in `lib/core/src/runtime/intrinsics.tml` (guarded by `#if X86_64`)
- [x] 1.4 Create `lib/core/src/simd/detect.tml` with feature detection functions:
  - [x] 1.4.1 `has_sse2() -> Bool` (always true on x86-64)
  - [x] 1.4.2 `has_sse42() -> Bool` (CPUID.1:ECX bit 20)
  - [x] 1.4.3 `has_popcnt() -> Bool` (CPUID.1:ECX bit 23)
  - [x] 1.4.4 `has_osxsave() -> Bool` (CPUID.1:ECX bit 27)
  - [x] 1.4.5 `has_avx() -> Bool` (OSXSAVE + XGETBV(0) bits 1:2 + CPUID.1:ECX bit 28)
  - [x] 1.4.6 `has_avx2() -> Bool` (has_avx() + CPUID.7.0:EBX bit 5)
  - [x] 1.4.7 `has_fma() -> Bool` (has_avx() + CPUID.1:ECX bit 12)
  - [x] 1.4.8 `has_neon() -> Bool` (compile-time true on ARM64)
- [x] 1.5 Add `detect` to `lib/core/src/simd/mod.tml` module exports
- [x] 1.6 Write `lib/core/tests/simd/detect.test.tml` — 10 tests (all pass)

> **Implementation note**: Used `@extern("c")` FFI to C runtime helpers instead of compiler intrinsics.
> Per-register functions (`tml_cpuid_eax`, `tml_cpuid_ebx`, etc.) avoid pointer-passing complexity.
> Functions added to `essential.c` (not separate file) because the test runtime archive
> (`tml_test_runtime.lib`) only includes essential.c objects via WHOLEARCHIVE.

## Phase 2: SSE2 Complete Intrinsic Set

> **Priority**: Critical | **Files**: `intrinsics_slice_simd.cpp`, `intrinsics.cpp`, `lib/core/src/runtime/intrinsics.tml`

### 2.1 Comparison Intrinsics
- [x] 2.1.1 `sse2_cmpgt_epi8` — PCMPGTB (byte signed greater-than) — icmp sgt + sext
- [x] 2.1.2 `sse2_cmpeq_epi16` — PCMPEQW (word equal) — icmp eq + sext
- [x] 2.1.3 `sse2_cmpeq_epi32` — PCMPEQD (dword equal) — icmp eq + sext
- [x] 2.1.4 `sse2_cmpgt_epi16` — PCMPGTW (word signed greater-than) — icmp sgt + sext
- [x] 2.1.5 `sse2_cmpgt_epi32` — PCMPGTD (dword signed greater-than) — icmp sgt + sext
- [x] 2.1.6 `sse2_cmplt_epi8` — via icmp slt + sext

### 2.2 Bitwise Intrinsics
- [x] 2.2.1 `sse2_and_si128` — PAND (LLVM `and` instruction)
- [x] 2.2.2 `sse2_or_si128` — POR (LLVM `or` instruction)
- [x] 2.2.3 `sse2_xor_si128` — PXOR (LLVM `xor` instruction)
- [x] 2.2.4 `sse2_andnot_si128` — PANDN (xor -1 + and)

### 2.3 Min/Max Intrinsics
- [x] 2.3.1 `sse2_min_epu8` — PMINUB (icmp ult + select)
- [x] 2.3.2 `sse2_max_epu8` — PMAXUB (icmp ugt + select)
- [x] 2.3.3 `sse2_min_epi16` — PMINSW (icmp slt + select)
- [x] 2.3.4 `sse2_max_epi16` — PMAXSW (icmp sgt + select)

### 2.4 Movemask Intrinsics
- [x] 2.4.1 `sse2_movemask_ps` — @llvm.x86.sse.movmsk.ps
- [x] 2.4.2 `sse2_movemask_pd` — @llvm.x86.sse2.movmsk.pd

### 2.5 Pack/Unpack Intrinsics
- [x] 2.5.1 `sse2_packs_epi16` — @llvm.x86.sse2.packsswb.128
- [x] 2.5.2 `sse2_packus_epi16` — @llvm.x86.sse2.packuswb.128
- [x] 2.5.3 `sse2_packs_epi32` — @llvm.x86.sse2.packssdw.128
- [x] 2.5.4 `sse2_unpacklo_epi8` — shufflevector (low interleave)
- [x] 2.5.5 `sse2_unpackhi_epi8` — shufflevector (high interleave)

### 2.6 Shift Intrinsics
- [x] 2.6.1 `sse2_slli_epi16/32/64` — shl with splatted shift amount
- [x] 2.6.2 `sse2_srli_epi16/32/64` — lshr with splatted shift amount
- [x] 2.6.3 `sse2_srai_epi16/32` — ashr with splatted shift amount

### 2.7 Memory Intrinsics
- [x] 2.7.1 `sse2_storeu_si128` — store align 1
- [x] 2.7.2 `sse2_store_si128` — store align 16

### 2.8 Tests
- [x] 2.8.1 Write `lib/core/tests/simd/sse2_intrinsics.test.tml` — 6 comparison tests (all pass)
- [x] 2.8.2 Write `lib/core/tests/simd/sse2_bitwise.test.tml` — 5 bitwise tests (all pass)
- [x] 2.8.3 Write `lib/core/tests/simd/sse2_pack_shift.test.tml` — 5 shift tests (all pass)

> **Implementation note**: All intrinsics use pure LLVM IR (icmp+sext for comparisons, and/or/xor for
> bitwise, icmp+select for min/max, shl/lshr/ashr for shifts). LLVM's backend lowers these to the
> correct SSE2 instructions. Pack intrinsics use LLVM x86 target intrinsics (@llvm.x86.sse2.*).
> Tests verified both via `tml.exe test` (direct) and `tml.exe run` (sandbox).

## Phase 3: SSE4.2 Intrinsics

> **Priority**: High | **Files**: `intrinsics_slice_simd.cpp`, `intrinsics.cpp`, `lib/core/src/runtime/intrinsics.tml`, `lib/core/src/simd/sse42.tml`

### 3.1 String Comparison Intrinsics
- [x] 3.1.1 `sse42_cmpistrm` — @llvm.x86.sse42.pcmpistrm128
- [x] 3.1.2 `sse42_cmpistri` — @llvm.x86.sse42.pcmpistri128
- [x] 3.1.3 `sse42_cmpestrm` — @llvm.x86.sse42.pcmpestrm128
- [x] 3.1.4 `sse42_cmpestri` — @llvm.x86.sse42.pcmpestri128

### 3.2 CRC32 Intrinsics
- [x] 3.2.1 `sse42_crc32_u8` — @llvm.x86.sse42.crc32.32.8
- [x] 3.2.2 `sse42_crc32_u16` — @llvm.x86.sse42.crc32.32.16
- [x] 3.2.3 `sse42_crc32_u32` — @llvm.x86.sse42.crc32.32.32
- [x] 3.2.4 `sse42_crc32_u64` — @llvm.x86.sse42.crc32.64.64

### 3.3 POPCNT
- [x] 3.3.1 `popcnt_u32` — @llvm.ctpop.i32
- [x] 3.3.2 `popcnt_u64` — @llvm.ctpop.i64

### 3.4 Library Wrappers
- [x] 3.4.1 Create `lib/core/src/simd/sse42.tml` with `crc32c` high-level wrapper
- [ ] 3.4.2 `str_find_sse42(haystack: Slice[U8], needle: Slice[U8]) -> Maybe[I64]` — deferred (needs Slice iteration support in lowlevel blocks)
- [x] 3.4.3 `crc32c(data: Slice[U8]) -> U32` — processes 8 bytes at a time via CRC32Q

### 3.5 Tests
- [x] 3.5.1 Write `lib/core/tests/simd/sse42_intrinsics.test.tml` — 8 tests (POPCNT + CRC32)
- [x] 3.5.2 Write `lib/core/tests/simd/crc32c.test.tml` — 4 CRC32C accumulation tests

> **Implementation note**: All intrinsics use LLVM target intrinsics (@llvm.x86.sse42.*) for
> CRC32/string comparison, and @llvm.ctpop for POPCNT. String comparison intrinsics (PCMPISTRI/M,
> PCMPESTRI/M) are exposed as raw intrinsics — the imm8 control byte determines comparison mode.
> The `crc32c` high-level wrapper processes 8 bytes at a time for throughput.
> Note: SIMD test suite has a pre-existing 100ms per-suite execution timeout issue;
> tests verified via `tml run` (all pass) and individually via `tml test --path` (crc32c passes).

## Phase 4: AVX2 256-bit Types and Intrinsics

> **Priority**: High | **Files**: `lib/core/src/simd/`, `intrinsics_slice_simd.cpp`, `intrinsics.cpp`, `lib/core/src/runtime/intrinsics.tml`

### 4.1 New 256-bit Vector Types
- [x] 4.1.1 `I8x32` — 32-lane I8 (`<32 x i8>`) — `lib/core/src/simd/i8x32.tml`
- [ ] 4.1.2 `U8x32` — 32-lane U8 (`<32 x i8>`) — deferred (same bit pattern as I8x32)
- [ ] 4.1.3 `I16x16` — 16-lane I16 (`<16 x i16>`) — deferred
- [x] 4.1.4 `I32x8` — 8-lane I32 (`<8 x i32>`) — `lib/core/src/simd/i32x8.tml`
- [x] 4.1.5 `I64x4` — 4-lane I64 (`<4 x i64>`) — `lib/core/src/simd/i64x4.tml`
- [x] 4.1.6 `F32x8` — 8-lane F32 (`<8 x float>`) — `lib/core/src/simd/f32x8.tml`
- [x] 4.1.7 `F64x4` — 4-lane F64 (`<4 x double>`) — `lib/core/src/simd/f64x4.tml`
- [ ] 4.1.8 `Mask8` — 8-lane boolean mask — deferred
- [ ] 4.1.9 `Mask32` — 32-lane boolean mask — deferred
- [x] 4.1.10 Register all 256-bit types in compiler `simd_types_` map — auto via @simd annotation

### 4.2 AVX2 Arithmetic Intrinsics
- [x] 4.2.1 `avx2_add_epi8/16/32/64` — via llvm_add on @simd types (I8x32.add, I32x8.add, etc.)
- [x] 4.2.2 `avx2_sub_epi8/16/32/64` — via llvm_sub on @simd types
- [x] 4.2.3 `avx2_mullo_epi16/32` — via llvm_mul on @simd types (I32x8.mul)

### 4.3 AVX2 Comparison Intrinsics
- [x] 4.3.1 `avx2_cmpeq_epi8/16/32` — icmp eq + sext (6 variants)
- [x] 4.3.2 `avx2_cmpgt_epi8/16/32` — icmp sgt + sext (6 variants)

### 4.4 AVX2 Bitwise & Movemask
- [x] 4.4.1 `avx2_and_si256` — LLVM `and` instruction
- [x] 4.4.2 `avx2_or_si256` — LLVM `or` instruction
- [x] 4.4.3 `avx2_xor_si256` — LLVM `xor` instruction
- [x] 4.4.4 `avx2_movemask_epi8` — @llvm.x86.avx2.pmovmskb

### 4.5 AVX2 Shuffle & Permute
- [x] 4.5.1 `avx2_shuffle_epi8` — @llvm.x86.avx2.pshuf.b (VPSHUFB)
- [x] 4.5.2 `avx2_permute4x64_epi64` — via @llvm.x86.avx2.permd (VPERMD)
- [x] 4.5.3 `avx2_permute2x128_si256` — via shufflevector (VPERM2I128)

### 4.6 AVX2 Horizontal & Pack
- [ ] 4.6.1 `avx2_hadd_epi16/32` — VPHADD (horizontal add)
- [ ] 4.6.2 `avx2_packs_epi16/32` — VPACKSS (pack with saturation)
- [ ] 4.6.3 `avx2_packus_epi16/32` — VPACKUS (pack unsigned saturation)

### 4.7 AVX2 Gather
- [ ] 4.7.1 `avx2_gather_epi32` — VPGATHERDD (indexed 32-bit loads)
- [ ] 4.7.2 `avx2_gather_epi64` — VPGATHERDQ (indexed 64-bit loads)
- [ ] 4.7.3 `avx2_gather_ps` — VGATHERDPS (indexed float loads)

### 4.8 AVX2 Variable Shift
- [ ] 4.8.1 `avx2_sllv_epi32/64` — VPSLLVD/Q (per-lane variable shift left)
- [ ] 4.8.2 `avx2_srlv_epi32/64` — VPSRLVD/Q (per-lane variable shift right)

### 4.9 FMA Intrinsics (FMA3)
- [ ] 4.9.1 `fma_fmadd_ps` — VFMADDPS (a*b+c, 8 floats)
- [ ] 4.9.2 `fma_fmadd_pd` — VFMADDPD (a*b+c, 4 doubles)
- [ ] 4.9.3 `fma_fmsub_ps/pd` — VFMSUBPS/PD (a*b-c)
- [ ] 4.9.4 `fma_fnmadd_ps/pd` — VFNMADDPS/PD (-a*b+c)
- [ ] 4.9.5 `fma_fmadd_ss/sd` — Scalar FMA (single float/double)

### 4.10 Tests
- [x] 4.10.1 Write `lib/core/tests/simd/avx2_basic.test.tml` — 7 tests (I32x8 + F32x8 types + arithmetic)
- [x] 4.10.2 Write `lib/core/tests/simd/avx2_compare.test.tml` — 6 tests (cmpeq/cmpgt/bitwise/movemask)
- [ ] 4.10.3 Write `lib/core/tests/simd/avx2_shuffle.test.tml` — shuffle/permute
- [ ] 4.10.4 Write `lib/core/tests/simd/avx2_fma.test.tml` — FMA operations
- [ ] 4.10.5 Write `lib/core/tests/simd/avx2_gather.test.tml` — gathered loads

> **Implementation note**: 256-bit types use @simd annotation — the compiler auto-detects and
> generates `<N x elemtype>` LLVM vector types. Arithmetic (add/sub/mul) uses existing llvm_add/
> llvm_sub/llvm_mul intrinsics which work on any vector width. AVX2 comparison/bitwise/movemask
> intrinsics use the same pattern as SSE2 (icmp+sext, and/or/xor, LLVM target intrinsics).
> All tests verified via `tml run` (9/9 pass). Items 4.6-4.9 (horizontal, gather, variable shift,
> FMA) not yet requested.
>
> **Utility methods added**: I32x8 now has full parity with I32x4 (div, neg, set, band/bor/bxor,
> shift_left/shift_right, product, hmin/hmax, min/max). F32x8 has neg (via sub), set, hmin/hmax,
> min/max, to_string/debug_string. I8x32 has band/bor/bxor, sum(->I32), hmin/hmax, to_string.
> I64x4 has mul, band/bor/bxor, shift_left/shift_right, hmin/hmax.
> Note: F32x8::neg uses `zero().sub(this)` because `simd_splat[F32x8, F32](0.0)` has a
> pre-existing F32/F64 literal codegen issue (0.0 emits as double instead of float).

## Phase 5: ARM NEON Intrinsics

> **Priority**: Medium | **Files**: `intrinsics.cpp`, `lib/core/src/simd/neon.tml`

### 5.1 NEON Arithmetic
- [ ] 5.1.1 `neon_add_i8/16/32/64` — VADD
- [ ] 5.1.2 `neon_add_f32/f64` — FADD
- [ ] 5.1.3 `neon_sub_i8/16/32/64` — VSUB
- [ ] 5.1.4 `neon_sub_f32/f64` — FSUB
- [ ] 5.1.5 `neon_mul_i8/16/32` — VMUL
- [ ] 5.1.6 `neon_mul_f32/f64` — FMUL

### 5.2 NEON FMA
- [ ] 5.2.1 `neon_fmla_f32` — VFMLA.F32 (fused multiply-add, 4 floats)
- [ ] 5.2.2 `neon_fmla_f64` — VFMLA.F64 (fused multiply-add, 2 doubles)
- [ ] 5.2.3 `neon_fmls_f32/f64` — VFMLS (fused multiply-subtract)

### 5.3 NEON Comparison
- [ ] 5.3.1 `neon_ceq_i8/16/32` — VCEQ (equal)
- [ ] 5.3.2 `neon_cgt_i8/16/32` — VCGT (signed greater-than)
- [ ] 5.3.3 `neon_cge_i8/16/32` — VCGE (signed greater-equal)
- [ ] 5.3.4 `neon_ceq_f32/f64` — FCMEQ (float equal)
- [ ] 5.3.5 `neon_cgt_f32/f64` — FCMGT (float greater-than)

### 5.4 NEON Bitwise
- [ ] 5.4.1 `neon_and_v128` — VAND
- [ ] 5.4.2 `neon_or_v128` — VORR
- [ ] 5.4.3 `neon_xor_v128` — VEOR
- [ ] 5.4.4 `neon_bsl_v128` — VBSL (bitwise select: mask ? a : b)
- [ ] 5.4.5 `neon_not_v128` — VMVN

### 5.5 NEON Table Lookup & Horizontal
- [ ] 5.5.1 `neon_tbl1_i8` — VTBL1 (byte table lookup, like PSHUFB)
- [ ] 5.5.2 `neon_cnt_i8` — VCNT (popcount per byte)
- [ ] 5.5.3 `neon_addv_i8/16/32` — VADDV (horizontal sum, single result)
- [ ] 5.5.4 `neon_maxv_i8/16/32` — VMAXV (horizontal max)
- [ ] 5.5.5 `neon_minv_i8/16/32` — VMINV (horizontal min)

### 5.6 NEON Min/Max/Abs
- [ ] 5.6.1 `neon_min_i8/16/32` — VMIN (lane-wise signed min)
- [ ] 5.6.2 `neon_max_i8/16/32` — VMAX (lane-wise signed max)
- [ ] 5.6.3 `neon_min_f32/f64` — FMIN
- [ ] 5.6.4 `neon_max_f32/f64` — FMAX
- [ ] 5.6.5 `neon_abs_i8/16/32` — VABS (absolute value)

### 5.7 NEON Memory
- [ ] 5.7.1 `neon_ld1_i8/16/32/64` — VLD1 (unaligned vector load)
- [ ] 5.7.2 `neon_st1_i8/16/32/64` — VST1 (unaligned vector store)
- [ ] 5.7.3 `neon_ld2_i8/16/32` — VLD2 (interleaved load, 2 vectors)

### 5.8 Tests
- [ ] 5.8.1 Write `lib/core/tests/simd/neon_basic.test.tml` — arithmetic + compare
- [ ] 5.8.2 Write `lib/core/tests/simd/neon_bitwise.test.tml` — bitwise + select
- [ ] 5.8.3 Write `lib/core/tests/simd/neon_horizontal.test.tml` — reductions

## Phase 6: Portable SIMD Abstraction Layer

> **Priority**: Medium | **Files**: `lib/core/src/simd/portable.tml`

- [ ] 6.1 Define `SimdVector[Self, Elem, LANES]` behavior with portable ops
- [ ] 6.2 Implement `SimdVector` for I32x4 (SSE2 on x86, NEON on ARM)
- [ ] 6.3 Implement `SimdVector` for F32x4
- [ ] 6.4 Implement `SimdVector` for I8x16 / U8x16
- [ ] 6.5 Implement `SimdVector` for I32x8 / F32x8 (AVX2 on x86, 2x NEON on ARM)
- [ ] 6.6 Add `simd_select` portable function (SSE2 blend / NEON BSL)
- [ ] 6.7 Write `lib/core/tests/simd/portable.test.tml`

## Phase 7: Library Algorithms

> **Priority**: Medium | **Files**: `lib/core/src/simd/`

- [ ] 7.1 `memchr_simd(haystack: Slice[U8], byte: U8) -> Maybe[I64]` — PCMPEQB + PMOVMSKB
- [ ] 7.2 `str_find_simd(haystack: Slice[U8], needle: Slice[U8]) -> Maybe[I64]` — SSE4.2 / SSE2 fallback
- [ ] 7.3 `case_upper_simd(data: MutSlice[U8])` — range check + conditional sub
- [ ] 7.4 `case_lower_simd(data: MutSlice[U8])` — range check + conditional add
- [ ] 7.5 `crc32c_simd(data: Slice[U8]) -> U32` — hardware CRC32C
- [ ] 7.6 `dot_product_simd(a: Slice[F32], b: Slice[F32]) -> F32` — FMA accumulation
- [ ] 7.7 Write tests for each algorithm

## Phase 8: Documentation

> **Priority**: Low

- [ ] 8.1 Update `docs/13-BUILTINS.md` with new SIMD intrinsics
- [ ] 8.2 Update `docs/04-TYPES.md` with new 256-bit vector types
- [ ] 8.3 Update `lib/core/src/simd/mod.tml` doc comments
- [ ] 8.4 Add usage examples in doc comments for key functions

## Validation

- [ ] V.1 CPUID correctly detects SSE4.2/AVX2 on host (Windows x86-64)
- [ ] V.2 OSXSAVE+XGETBV prevents AVX on unsupported OS (test on VM if possible)
- [ ] V.3 All SSE2 intrinsics emit correct LLVM IR (verify with `--emit-ir`)
- [ ] V.4 SSE4.2 PCMPISTRI string search matches scalar on all edge cases
- [ ] V.5 256-bit types produce `<8 x i32>` / `<8 x float>` in LLVM IR
- [ ] V.6 FMA intrinsics produce `llvm.fma.v8f32` / `llvm.fma.v4f64`
- [ ] V.7 Existing 9 SIMD test files still pass (no regressions)
- [ ] V.8 New test files pass on Windows x86-64
- [ ] V.9 `--emit-ir` for ARM NEON intrinsics produces correct AArch64 IR (cross-compile check)
