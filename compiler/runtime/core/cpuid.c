/**
 * @file cpuid.c
 * @brief TML Runtime - CPU Feature Detection Helpers
 *
 * Provides CPUID and XGETBV wrappers for runtime CPU feature detection.
 * Used by `core::simd::detect` to query ISA extensions (SSE4.2, AVX2, etc.).
 *
 * Each CPUID register has its own function to avoid pointer-passing complexity
 * in the TML FFI layer. The overhead of calling CPUID multiple times is
 * negligible for feature detection (done once at startup).
 */

#include <stdint.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#endif

/* ========================================================================== */
/* CPUID — per-register accessors                                             */
/* ========================================================================== */

static void do_cpuid(int32_t leaf, int32_t subleaf, int32_t regs[4]) {
#if defined(_MSC_VER)
    __cpuidex((int*)regs, leaf, subleaf);
#elif defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("cpuid"
                     : "=a"(regs[0]), "=b"(regs[1]), "=c"(regs[2]), "=d"(regs[3])
                     : "a"(leaf), "c"(subleaf));
#else
    regs[0] = regs[1] = regs[2] = regs[3] = 0;
#endif
}

/** CPUID(leaf, subleaf) -> EAX */
TML_EXPORT int32_t tml_cpuid_eax(int32_t leaf, int32_t subleaf) {
    int32_t regs[4];
    do_cpuid(leaf, subleaf, regs);
    return regs[0];
}

/** CPUID(leaf, subleaf) -> EBX */
TML_EXPORT int32_t tml_cpuid_ebx(int32_t leaf, int32_t subleaf) {
    int32_t regs[4];
    do_cpuid(leaf, subleaf, regs);
    return regs[1];
}

/** CPUID(leaf, subleaf) -> ECX */
TML_EXPORT int32_t tml_cpuid_ecx(int32_t leaf, int32_t subleaf) {
    int32_t regs[4];
    do_cpuid(leaf, subleaf, regs);
    return regs[2];
}

/** CPUID(leaf, subleaf) -> EDX */
TML_EXPORT int32_t tml_cpuid_edx(int32_t leaf, int32_t subleaf) {
    int32_t regs[4];
    do_cpuid(leaf, subleaf, regs);
    return regs[3];
}

/* ========================================================================== */
/* XGETBV                                                                     */
/* ========================================================================== */

/**
 * Read Extended Control Register (XCR).
 *
 * Used to check whether the OS has enabled AVX/AVX-512 state saving.
 * Only call after verifying OSXSAVE bit (CPUID.1:ECX bit 27).
 *
 * @param xcr_index  XCR register index (typically 0)
 * @return           64-bit XCR value (low 32 in EAX, high 32 in EDX)
 */
TML_EXPORT int64_t tml_xgetbv(int32_t xcr_index) {
#if defined(_MSC_VER)
    return (int64_t)_xgetbv((unsigned int)xcr_index);
#elif defined(__GNUC__) || defined(__clang__)
    uint32_t lo, hi;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(xcr_index));
    return ((int64_t)hi << 32) | (int64_t)lo;
#else
    (void)xcr_index;
    return 0;
#endif
}
