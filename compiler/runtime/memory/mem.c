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

#ifdef _WIN32
#include <malloc.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#elif defined(__APPLE__)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

// Export symbols for JIT discovery (DynamicLibrarySearchGenerator)
#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

#ifdef TML_DEBUG_MEMORY
#include "mem_track.h"
#endif

// ============================================================================
// Heap Pointer Detection
// ============================================================================
//
// On Windows, calling `_msize` on a non-heap pointer (e.g. a `.rdata` string
// literal) is undefined behavior and typically crashes. To make
// `mem_usable_size` safe for arbitrary pointers, we first check whether the
// pointer is inside a loaded module's image (i.e., a literal) — if yes,
// return 0 immediately. Otherwise it's a heap allocation and `_msize` is
// safe to call.
//
// This reuses the same image-range-check technique as `tml_str_free`.

#ifdef _WIN32
#define MAX_MEM_IMAGE_RANGES 128

typedef struct {
    uintptr_t base;
    uintptr_t end;
} MemImageRange;

static MemImageRange g_mem_image_ranges[MAX_MEM_IMAGE_RANGES];
static int g_mem_image_range_count = 0;
static volatile int g_mem_image_ranges_initialized = 0;

static int mem_image_range_cmp(const void* a, const void* b) {
    const MemImageRange* ra = (const MemImageRange*)a;
    const MemImageRange* rb = (const MemImageRange*)b;
    if (ra->base < rb->base) return -1;
    if (ra->base > rb->base) return 1;
    return 0;
}

static void mem_init_image_ranges(void) {
    HMODULE modules[MAX_MEM_IMAGE_RANGES];
    DWORD needed = 0;
    HANDLE proc = GetCurrentProcess();

    if (EnumProcessModules(proc, modules, sizeof(modules), &needed)) {
        int count = (int)(needed / sizeof(HMODULE));
        if (count > MAX_MEM_IMAGE_RANGES) count = MAX_MEM_IMAGE_RANGES;

        for (int i = 0; i < count; i++) {
            MODULEINFO mi;
            if (GetModuleInformation(proc, modules[i], &mi, sizeof(mi))) {
                g_mem_image_ranges[g_mem_image_range_count].base =
                    (uintptr_t)mi.lpBaseOfDll;
                g_mem_image_ranges[g_mem_image_range_count].end =
                    (uintptr_t)mi.lpBaseOfDll + mi.SizeOfImage;
                g_mem_image_range_count++;
            }
        }
        qsort(g_mem_image_ranges, g_mem_image_range_count,
              sizeof(MemImageRange), mem_image_range_cmp);
    }
    g_mem_image_ranges_initialized = 1;
}

// Returns 1 if `addr` is inside any loaded module's image (.rdata/.text/.data),
// 0 otherwise. Binary-search, O(log n) where n = loaded module count.
static inline int mem_is_image_ptr(uintptr_t addr) {
    int lo = 0, hi = g_mem_image_range_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (addr < g_mem_image_ranges[mid].base) {
            hi = mid - 1;
        } else if (addr >= g_mem_image_ranges[mid].end) {
            lo = mid + 1;
        } else {
            return 1;
        }
    }
    return 0;
}
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
TML_EXPORT void* mem_alloc(int64_t size) {
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
TML_EXPORT void* mem_alloc_zeroed(int64_t size) {
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
TML_EXPORT void* mem_realloc(void* ptr, int64_t new_size) {
#ifdef TML_DEBUG_MEMORY
    void* new_ptr = realloc(ptr, (size_t)new_size);
    tml_mem_track_realloc(ptr, new_ptr, (size_t)new_size);
    return new_ptr;
#else
    return realloc(ptr, (size_t)new_size);
#endif
}

/**
 * @brief Returns the actual allocated block size for a heap pointer, 0 for
 * non-heap (literal) pointers.
 *
 * Safe to call on ANY valid pointer, including string literals in `.rdata`.
 * Used by `str_append` to decide whether the current buffer has enough slack
 * to append in place (avoiding a malloc+memcpy on every concat and turning
 * `s = s + x` loops from O(n²) into amortized O(1)).
 */
TML_EXPORT int64_t mem_usable_size(void* ptr) {
    if (!ptr) return 0;
#ifdef _WIN32
    // Image-range guard: literals live in `.rdata` / `.text` of a loaded
    // module. Calling `_msize` on them is undefined behavior.
    if (!g_mem_image_ranges_initialized) {
        mem_init_image_ranges();
    }
    if (mem_is_image_ptr((uintptr_t)ptr)) {
        return 0;
    }
    return (int64_t)_msize(ptr);
#elif defined(__APPLE__)
    return (int64_t)malloc_size(ptr);
#elif defined(__GLIBC__) || defined(__linux__)
    return (int64_t)malloc_usable_size(ptr);
#else
    (void)ptr;
    return 0;
#endif
}

/**
 * @brief Frees allocated memory.
 *
 * Maps to TML's `mem_free(ptr: *Unit) -> Unit` builtin.
 *
 * @param ptr Pointer to memory to free. NULL is safe.
 */
TML_EXPORT void mem_free(void* ptr) {
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
TML_EXPORT void mem_copy(void* dest, const void* src, int64_t size) {
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
TML_EXPORT void mem_move(void* dest, const void* src, int64_t size) {
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
TML_EXPORT void mem_set(void* ptr, int32_t value, int64_t size) {
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
TML_EXPORT void mem_zero(void* ptr, int64_t size) {
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
TML_EXPORT int32_t mem_compare(const void* a, const void* b, int64_t size) {
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
TML_EXPORT int32_t mem_eq(const void* a, const void* b, int64_t size) {
    return memcmp(a, b, (size_t)size) == 0 ? 1 : 0;
}
