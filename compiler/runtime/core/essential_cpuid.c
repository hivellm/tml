/**
 * @file essential_cpuid.c
 * @brief TML Runtime - CPUID / XGETBV CPU feature detection helpers
 *
 * Provides wrappers around the CPUID instruction and XGETBV instruction
 * for detecting CPU features at runtime (SSE, AVX, AVX-512, etc.).
 *
 * Extracted from essential.c (Phase 38 split).
 */

#include <stdint.h>

// Export macro for DLL visibility
#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

// ============================================================================
// CPUID / XGETBV — CPU feature detection helpers
// ============================================================================

#if defined(_MSC_VER)
#include <intrin.h>
#endif

static void do_cpuid_(int32_t leaf, int32_t subleaf, int32_t regs[4]) {
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

TML_EXPORT int32_t tml_cpuid_eax(int32_t leaf, int32_t subleaf) {
    int32_t regs[4];
    do_cpuid_(leaf, subleaf, regs);
    return regs[0];
}
TML_EXPORT int32_t tml_cpuid_ebx(int32_t leaf, int32_t subleaf) {
    int32_t regs[4];
    do_cpuid_(leaf, subleaf, regs);
    return regs[1];
}
TML_EXPORT int32_t tml_cpuid_ecx(int32_t leaf, int32_t subleaf) {
    int32_t regs[4];
    do_cpuid_(leaf, subleaf, regs);
    return regs[2];
}
TML_EXPORT int32_t tml_cpuid_edx(int32_t leaf, int32_t subleaf) {
    int32_t regs[4];
    do_cpuid_(leaf, subleaf, regs);
    return regs[3];
}

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
