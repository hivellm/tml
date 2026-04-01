/**
 * @file essential_ffi.c
 * @brief FFI utility functions for TML <-> C interop.
 *
 * Simple wrappers:
 * - tml_str_from_cstr: converts a C string pointer to a TML Str (identity op)
 * - tml_free: frees heap memory allocated by FFI-returning functions
 *
 * Extracted from essential.c — no statics, no linkage changes.
 */

#include <stdlib.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

/**
 * @brief Converts a C string to a TML Str.
 *
 * In TML, Str is represented as a pointer to a null-terminated string.
 * This function simply returns the pointer unchanged.
 *
 * @param cstr Pointer to a null-terminated C string.
 * @return The same pointer, suitable for use as TML Str.
 */
TML_EXPORT const char* tml_str_from_cstr(const char* cstr) {
    return cstr;
}

/**
 * @brief Frees memory allocated by FFI functions.
 *
 * This is a wrapper around free() for use from TML code when FFI
 * functions return heap-allocated memory.
 *
 * @param ptr Pointer to memory to free.
 */
TML_EXPORT void tml_free(void* ptr) {
    free(ptr);
}
