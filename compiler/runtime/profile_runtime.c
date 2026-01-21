/**
 * @file profile_runtime.c
 * @brief Minimal runtime functions for profiling TML overhead
 *
 * These functions do minimal work to help isolate where
 * performance overhead comes from in TML.
 */

#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

/**
 * @brief Noop function - measures pure FFI call overhead
 */
TML_EXPORT int32_t profile_noop(void) {
    return 0;
}

/**
 * @brief Echo i32 - measures i32 parameter passing
 */
TML_EXPORT int32_t profile_echo_i32(int32_t x) {
    return x;
}

/**
 * @brief Echo i64 - measures i64 parameter passing
 */
TML_EXPORT int64_t profile_echo_i64(int64_t x) {
    return x;
}

/**
 * @brief Echo string - measures string parameter overhead
 * Returns length of string
 */
TML_EXPORT int32_t profile_echo_str(const char* s) {
    if (!s)
        return 0;
    return (int32_t)strlen(s);
}

/**
 * @brief Add two integers - minimal compute
 */
TML_EXPORT int32_t profile_add(int32_t a, int32_t b) {
    return a + b;
}
