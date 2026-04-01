/**
 * @file essential_random.c
 * @brief TML Runtime - Random Seed Generation (SplitMix64)
 *
 * Extracted from essential.c. All statics remain static (translation-unit scope).
 */

#include <stdint.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

// ============================================================================
// Random Seed Generation
// ============================================================================

/** @brief Global counter for generating unique seeds. */
static uint64_t tml_seed_counter = 0;

/**
 * @brief Gets a unique random seed value.
 *
 * Uses a combination of a monotonic counter and the process address space
 * to generate unique seeds across multiple calls.
 *
 * @return A 64-bit seed value.
 */
TML_EXPORT uint64_t tml_random_seed(void) {
    // Increment counter atomically (simple version - not thread-safe)
    uint64_t counter = ++tml_seed_counter;

    // Mix in the address of the counter variable for additional entropy
    uint64_t addr = (uint64_t)(uintptr_t)&tml_seed_counter;

    // Simple mixing function (similar to SplitMix64)
    uint64_t seed = counter ^ addr;
    seed = seed * 0x9E3779B97F4A7C15ULL;
    seed ^= seed >> 30;
    seed = seed * 0xBF58476D1CE4E5B9ULL;
    seed ^= seed >> 27;

    return seed;
}
