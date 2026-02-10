/**
 * @file mem.c
 * @brief TML Runtime - Memory Functions
 *
 * Implements memory management functions for the TML language. These provide
 * the runtime support for TML's low-level memory operations.
 *
 * ## Components
 *
 * - **Allocation**: `mem_alloc`, `mem_alloc_zeroed`, `mem_realloc`, `mem_free`
 * - **Operations**: `mem_copy`, `mem_move`, `mem_set`, `mem_zero`
 * - **Comparison**: `mem_compare`, `mem_eq`
 *
 * ## Usage in TML
 *
 * These functions are typically used through TML's `lowlevel` blocks or
 * by the compiler's generated code for heap allocation.
 *
 * ```tml
 * lowlevel {
 *     let ptr = mem_alloc(size)
 *     mem_zero(ptr, size)
 *     // ... use memory ...
 *     mem_free(ptr)
 * }
 * ```
 *
 * ## Thread Safety
 *
 * All functions are thread-safe as they wrap standard C library functions.
 *
 * ## Memory Tracking
 *
 * When TML_DEBUG_MEMORY is defined, all allocations are tracked and
 * memory leaks are reported at program exit.
 *
 * @see env_builtins_mem.cpp for compiler builtin registration
 * @see mem_track.h for memory tracking API
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef TML_DEBUG_MEMORY
#include "mem_track.h"
#endif

// ============================================================================
// Allocation Functions
// ============================================================================

/**
 * @brief Allocates uninitialized memory.
 *
 * Maps to TML's `mem_alloc(size: I64) -> *Unit` builtin.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to allocated memory, or NULL on failure.
 */
void* mem_alloc(int64_t size) {
#ifdef TML_DEBUG_MEMORY
    void* ptr = malloc((size_t)size);
    tml_mem_track_alloc(ptr, (size_t)size, "mem_alloc");
    return ptr;
#else
    return malloc((size_t)size);
#endif
}

/**
 * @brief Allocates zero-initialized memory.
 *
 * Maps to TML's `mem_alloc_zeroed(size: I64) -> *Unit` builtin.
 *
 * @param size Number of bytes to allocate.
 * @return Pointer to zero-initialized memory, or NULL on failure.
 */
void* mem_alloc_zeroed(int64_t size) {
#ifdef TML_DEBUG_MEMORY
    void* ptr = calloc(1, (size_t)size);
    tml_mem_track_alloc(ptr, (size_t)size, "mem_alloc_zeroed");
    return ptr;
#else
    return calloc(1, (size_t)size);
#endif
}

/**
 * @brief Reallocates memory to a new size.
 *
 * Maps to TML's `mem_realloc(ptr: *Unit, new_size: I64) -> *Unit` builtin.
 *
 * @param ptr Pointer to existing allocation (or NULL for new allocation).
 * @param new_size New size in bytes.
 * @return Pointer to reallocated memory, or NULL on failure.
 */
void* mem_realloc(void* ptr, int64_t new_size) {
#ifdef TML_DEBUG_MEMORY
    void* new_ptr = realloc(ptr, (size_t)new_size);
    tml_mem_track_realloc(ptr, new_ptr, (size_t)new_size);
    return new_ptr;
#else
    return realloc(ptr, (size_t)new_size);
#endif
}

/**
 * @brief Frees allocated memory.
 *
 * Maps to TML's `mem_free(ptr: *Unit) -> Unit` builtin.
 *
 * @param ptr Pointer to memory to free. NULL is safe.
 */
void mem_free(void* ptr) {
#ifdef TML_DEBUG_MEMORY
    tml_mem_track_free(ptr);
#endif
    free(ptr);
}

// ============================================================================
// Memory Operations
// ============================================================================

/**
 * @brief Copies memory (non-overlapping regions).
 *
 * Maps to TML's `mem_copy(dest: *Unit, src: *Unit, size: I64) -> Unit` builtin.
 * For overlapping regions, use `mem_move` instead.
 *
 * @param dest Destination pointer.
 * @param src Source pointer.
 * @param size Number of bytes to copy.
 */
void mem_copy(void* dest, const void* src, int64_t size) {
    memcpy(dest, src, (size_t)size);
}

/**
 * @brief Moves memory (handles overlapping regions).
 *
 * Maps to TML's `mem_move(dest: *Unit, src: *Unit, size: I64) -> Unit` builtin.
 * Safe for overlapping source and destination regions.
 *
 * @param dest Destination pointer.
 * @param src Source pointer.
 * @param size Number of bytes to move.
 */
void mem_move(void* dest, const void* src, int64_t size) {
    memmove(dest, src, (size_t)size);
}

/**
 * @brief Sets memory to a byte value.
 *
 * Maps to TML's `mem_set(ptr: *Unit, value: I32, size: I64) -> Unit` builtin.
 *
 * @param ptr Pointer to memory region.
 * @param value Value to set (truncated to unsigned char).
 * @param size Number of bytes to set.
 */
void mem_set(void* ptr, int32_t value, int64_t size) {
    memset(ptr, value, (size_t)size);
}

/**
 * @brief Zeros a memory region.
 *
 * Maps to TML's `mem_zero(ptr: *Unit, size: I64) -> Unit` builtin.
 * Equivalent to `mem_set(ptr, 0, size)`.
 *
 * @param ptr Pointer to memory region.
 * @param size Number of bytes to zero.
 */
void mem_zero(void* ptr, int64_t size) {
    memset(ptr, 0, (size_t)size);
}

// ============================================================================
// Memory Comparison
// ============================================================================

/**
 * @brief Compares two memory regions.
 *
 * Maps to TML's `mem_compare(a: *Unit, b: *Unit, size: I64) -> I32` builtin.
 *
 * @param a First memory region.
 * @param b Second memory region.
 * @param size Number of bytes to compare.
 * @return <0 if a<b, 0 if equal, >0 if a>b.
 */
int32_t mem_compare(const void* a, const void* b, int64_t size) {
    return memcmp(a, b, (size_t)size);
}

/**
 * @brief Checks if two memory regions are equal.
 *
 * Maps to TML's `mem_eq(a: *Unit, b: *Unit, size: I64) -> Bool` builtin.
 *
 * @param a First memory region.
 * @param b Second memory region.
 * @param size Number of bytes to compare.
 * @return 1 if equal, 0 if not equal.
 */
int32_t mem_eq(const void* a, const void* b, int64_t size) {
    return memcmp(a, b, (size_t)size) == 0 ? 1 : 0;
}
