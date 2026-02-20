/**
 * @file time.c
 * @brief TML Runtime - Time Functions
 *
 * Implements time-related functions for the TML language. Provides cross-platform
 * time reading, sleeping, and duration measurement.
 *
 * ## Components
 *
 * - **Time reading**: `time_ns`
 * - **Sleep**: `sleep_ms`
 * - **Instant API**: Rust-style `instant_now`, `instant_elapsed`
 *
 * ## Platform Support
 *
 * - **Windows**: Uses `QueryPerformanceCounter`, `Sleep`
 * - **POSIX**: Uses `clock_gettime`, `nanosleep`
 */

#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <unistd.h>
#endif

// ============================================================================
// Time Reading Functions
// ============================================================================

// time_ns() -> I64
#ifdef _WIN32
int64_t time_ns(void) {
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (int64_t)(count.QuadPart * 1000000000 / freq.QuadPart);
}
#else
int64_t time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)(ts.tv_sec * 1000000000LL + ts.tv_nsec);
}
#endif

// ============================================================================
// Sleep
// ============================================================================

// sleep_ms(ms: I32) -> Unit
#ifdef _WIN32
void sleep_ms(int32_t ms) {
    Sleep((DWORD)ms);
}
#else
void sleep_ms(int32_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}
#endif

// ============================================================================
// Instant API (like Rust's std::time::Instant)
// ============================================================================

// instant_now() -> I64 - Get current instant in nanoseconds
int64_t instant_now(void) {
    return time_ns();
}

// instant_elapsed(start: I64) -> I64 - Get elapsed duration in nanoseconds
int64_t instant_elapsed(int64_t start) {
    return time_ns() - start;
}
