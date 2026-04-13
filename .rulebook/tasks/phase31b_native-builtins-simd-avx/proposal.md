# Proposal: phase31b_native-builtins-simd-avx

## Why
SIMD is the primary source of throughput in numeric, ML, image-processing, and
cryptographic TML code. Without native SIMD intrinsic emission, every `@simd`
annotated loop and every call into the tensor or math libraries falls back to
scalar code via the LLVM backend, defeating the purpose of the native path. SSE2
is baseline for all x86-64 targets; SSE4.2 adds accelerated string operations;
AVX2 doubles lane width to 256-bit and is available on all post-2013 Intel/AMD
CPUs targeted by TML. Without AVX2 support the native backend cannot match the
performance of simple C scalar code on any workload wider than 128 bits.

## What Changes
- `compiler-tml/src/native/x86/sse.tml` is extended to cover SSE2 128-bit integer
  ops (paddq, psubq, pmullw, pcmpeqd, pand, por, pxor, psllq, psrlq) and SSE2
  128-bit double ops (addpd, subpd, mulpd, divpd, sqrtpd, cmpltpd, minpd, maxpd).
- SSE4.2 string intrinsics: pcmpistri / pcmpistrm for fast substring search.
- A new `compiler-tml/src/native/x86/avx.tml` module covers AVX2 256-bit integer
  ops (vpaddq, vpsubq, vpmullw, vpcmpeqd, vpand, vpor, vpxor, vpsllq, vpsrlq)
  and AVX2 double ops (vaddpd, vsubpd, vmulpd, vdivpd, vsqrtpd, vcmpltpd).
- CPUID detection at codegen time: if the target CPU flags include `avx2` the
  AVX2 path is selected; otherwise the SSE2 path is used.

## Impact
- Affected specs: native-backend/simd
- Affected code: compiler-tml/src/native/x86/sse.tml (extended), compiler-tml/src/native/x86/avx.tml (new)
- Breaking change: NO
- User benefit: Numeric and tensor code compiles to full-width SIMD instructions natively, matching or exceeding LLVM output on vectorisable loops.
