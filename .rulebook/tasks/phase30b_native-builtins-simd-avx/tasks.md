## 1. Implementation
- [ ] 1.1 SSE2 128-bit integer ops in sse.tml: emit paddq, psubq, pmullw, pcmpeqd, pand, por, pxor, psllq, psrlq using the existing x86 instruction encoder; wire into the SIMD intrinsic dispatch table with their TML builtin names
- [ ] 1.2 SSE2 128-bit double ops in sse.tml: emit addpd, subpd, mulpd, divpd, sqrtpd, cmpltpd (result mask), minpd, maxpd; expose as `@simd_f64x2_add` etc. in the dispatch table
- [ ] 1.3 SSE4.2 string ops in sse.tml: emit pcmpistri with EQUAL_ANY mode for fast null-terminated substring search; emit pcmpistrm for mask-based search; expose as `simd_str_find` intrinsic
- [ ] 1.4 AVX2 256-bit integer ops in new avx.tml: emit vpaddq, vpsubq, vpmullw, vpcmpeqd, vpand, vpor, vpxor, vpsllq, vpsrlq using VEX-prefixed encoding; wire into dispatch table
- [ ] 1.5 AVX2 256-bit double ops in avx.tml: emit vaddpd, vsubpd, vmulpd, vdivpd, vsqrtpd, vcmpltpd (imm8=1), vminpd, vmaxpd; expose as `@simd_f64x4_add` etc.
- [ ] 1.6 CPUID detection: read ECX/EBX feature bits at codegen time from the target CPU descriptor; if `avx2` flag is set select avx.tml path; else fall back to sse.tml path for all 256-bit intrinsics
- [ ] 1.7 Integration test: vectorised dot-product over two 8-element F64 arrays using AVX2 intrinsics; assert result matches scalar reference to within FP epsilon

## 2. Tail (mandatory — enforced by rulebook v5.3.0)
- [ ] 2.1 Update or create documentation covering the implementation
- [ ] 2.2 Write tests covering the new behavior
- [ ] 2.3 Run tests and confirm they pass
