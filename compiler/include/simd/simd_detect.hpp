#pragma once

//! # SIMD Feature Detection
//!
//! Runtime CPUID detection for x86-64 SIMD instruction sets.
//! Results are cached in static bools — call once per process.
//!
//! Usage:
//!   if (tml::simd::has_avx2()) { /* AVX2 path */ }
//!   else if (tml::simd::has_sse2()) { /* SSE2 fallback */ }

#include <cstdint>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace tml::simd {

namespace detail {

struct CpuidResult {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
};

inline auto cpuid(uint32_t leaf, uint32_t subleaf = 0) -> CpuidResult {
    CpuidResult r{};
#ifdef _MSC_VER
    int regs[4];
    __cpuidex(regs, static_cast<int>(leaf), static_cast<int>(subleaf));
    r.eax = static_cast<uint32_t>(regs[0]);
    r.ebx = static_cast<uint32_t>(regs[1]);
    r.ecx = static_cast<uint32_t>(regs[2]);
    r.edx = static_cast<uint32_t>(regs[3]);
#else
    __cpuid_count(leaf, subleaf, r.eax, r.ebx, r.ecx, r.edx);
#endif
    return r;
}

// Bit 27 of ECX from CPUID leaf 1 — OSXSAVE (OS has enabled XSAVE for AVX state)
inline auto os_supports_avx() -> bool {
    auto leaf1 = cpuid(1);
    if (!(leaf1.ecx & (1u << 27)))
        return false; // OSXSAVE not set

    // Check XCR0 bits 1 (SSE state) and 2 (AVX state)
#ifdef _MSC_VER
    auto xcr0 = _xgetbv(0);
#else
    uint32_t lo, hi;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    uint64_t xcr0 = (static_cast<uint64_t>(hi) << 32) | lo;
#endif
    return (xcr0 & 0x6) == 0x6; // bits 1 and 2
}

inline auto os_supports_avx512() -> bool {
    if (!os_supports_avx())
        return false;
#ifdef _MSC_VER
    auto xcr0 = _xgetbv(0);
#else
    uint32_t lo, hi;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    uint64_t xcr0 = (static_cast<uint64_t>(hi) << 32) | lo;
#endif
    // bits 5, 6, 7 = opmask, ZMM_Hi256, Hi16_ZMM
    return (xcr0 & 0xE6) == 0xE6;
}

struct SimdFeatures {
    bool sse2 = false;
    bool sse42 = false;
    bool avx2 = false;
    bool avx512f = false;
    bool aesni = false;
    bool popcnt = false;
    bool fma = false;
    bool bmi1 = false;
    bool bmi2 = false;
};

inline auto detect() -> SimdFeatures {
    SimdFeatures f;

    // Leaf 0: max supported leaf
    auto leaf0 = cpuid(0);
    uint32_t max_leaf = leaf0.eax;

    if (max_leaf >= 1) {
        auto leaf1 = cpuid(1);

        // EDX bits (leaf 1)
        f.sse2 = (leaf1.edx & (1u << 26)) != 0;

        // ECX bits (leaf 1)
        f.sse42 = (leaf1.ecx & (1u << 20)) != 0;
        f.aesni = (leaf1.ecx & (1u << 25)) != 0;
        f.popcnt = (leaf1.ecx & (1u << 23)) != 0;
        f.fma = (leaf1.ecx & (1u << 12)) != 0;
    }

    if (max_leaf >= 7) {
        auto leaf7 = cpuid(7, 0);

        // EBX bits (leaf 7, subleaf 0)
        f.bmi1 = (leaf7.ebx & (1u << 3)) != 0;
        f.bmi2 = (leaf7.ebx & (1u << 8)) != 0;

        if (os_supports_avx()) {
            f.avx2 = (leaf7.ebx & (1u << 5)) != 0;
            f.fma = f.fma; // FMA requires AVX state saving
        } else {
            f.avx2 = false;
            f.fma = false;
        }

        if (os_supports_avx512()) {
            f.avx512f = (leaf7.ebx & (1u << 16)) != 0;
        } else {
            f.avx512f = false;
        }
    }

    return f;
}

// Singleton — initialized once on first call
inline auto features() -> const SimdFeatures& {
    static const SimdFeatures f = detect();
    return f;
}

} // namespace detail

// Public query functions — all inline, zero overhead after first call
inline auto has_sse2() -> bool {
    return detail::features().sse2;
}
inline auto has_sse42() -> bool {
    return detail::features().sse42;
}
inline auto has_avx2() -> bool {
    return detail::features().avx2;
}
inline auto has_avx512() -> bool {
    return detail::features().avx512f;
}
inline auto has_aesni() -> bool {
    return detail::features().aesni;
}
inline auto has_popcnt() -> bool {
    return detail::features().popcnt;
}
inline auto has_fma() -> bool {
    return detail::features().fma;
}
inline auto has_bmi1() -> bool {
    return detail::features().bmi1;
}
inline auto has_bmi2() -> bool {
    return detail::features().bmi2;
}

} // namespace tml::simd
