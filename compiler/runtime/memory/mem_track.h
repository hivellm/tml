/**
 * @file mem_track.h
 * @brief TML Runtime - Memory Tracking for Debug Builds
 *
 * Provides memory leak detection by tracking all allocations and deallocations.
 * When enabled (TML_DEBUG_MEMORY), all mem_alloc/mem_free calls are tracked
 * and unfreed allocations are reported at program exit.
 *
 * ## Usage
 *
 * Build with -DTML_DEBUG_MEMORY to enable tracking. At program exit,
 * `tml_mem_check_leaks()` is called automatically to report any leaks.
 *
 * ## Thread Safety
 *
 * All tracking operations are protected by a mutex for thread safety.
 */

#ifndef TML_MEM_TRACK_H
#define TML_MEM_TRACK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Memory Tracking Configuration
// ============================================================================

/** Maximum number of allocations to track (can be increased) */
#ifndef TML_MEM_TRACK_MAX
#define TML_MEM_TRACK_MAX 65536
#endif

/** Maximum stack depth for allocation call stacks */
#ifndef TML_MEM_TRACK_STACK_DEPTH
#define TML_MEM_TRACK_STACK_DEPTH 8
#endif

// ============================================================================
// Allocation Record
// ============================================================================

/**
 * @brief Record of a single allocation.
 */
typedef struct TmlAllocRecord {
    void* ptr;             /**< Allocated pointer */
    size_t size;           /**< Allocation size in bytes */
    uint64_t alloc_id;     /**< Sequential allocation ID */
    uint64_t timestamp_ns; /**< Allocation timestamp (nanoseconds) */
    const char* tag;       /**< Optional allocation tag/label */
} TmlAllocRecord;

// ============================================================================
// Tracking Statistics
// ============================================================================

/**
 * @brief Memory tracking statistics.
 */
typedef struct TmlMemStats {
    uint64_t total_allocations;     /**< Total number of allocations */
    uint64_t total_deallocations;   /**< Total number of deallocations */
    uint64_t current_allocations;   /**< Current active allocations */
    uint64_t peak_allocations;      /**< Peak number of active allocations */
    uint64_t total_bytes_allocated; /**< Total bytes ever allocated */
    uint64_t current_bytes;         /**< Current bytes in use */
    uint64_t peak_bytes;            /**< Peak bytes in use */
    uint64_t double_frees;          /**< Number of double-free attempts */
    uint64_t invalid_frees;         /**< Number of invalid free attempts */
} TmlMemStats;

// ============================================================================
// Tracking API
// ============================================================================

/**
 * @brief Initializes the memory tracking system.
 *
 * Called automatically on first allocation if not explicitly initialized.
 * Can be called manually to reset tracking state.
 */
void tml_mem_track_init(void);

/**
 * @brief Shuts down the memory tracking system.
 *
 * Reports any unfreed allocations and cleans up tracking data.
 * Called automatically at program exit via atexit().
 */
void tml_mem_track_shutdown(void);

/**
 * @brief Records an allocation.
 *
 * @param ptr The allocated pointer.
 * @param size The allocation size.
 * @param tag Optional tag for the allocation (can be NULL).
 */
void tml_mem_track_alloc(void* ptr, size_t size, const char* tag);

/**
 * @brief Records a deallocation.
 *
 * @param ptr The pointer being freed.
 * @return 1 if the pointer was tracked, 0 if it was unknown/double-free.
 */
int32_t tml_mem_track_free(void* ptr);

/**
 * @brief Records a reallocation.
 *
 * @param old_ptr The original pointer.
 * @param new_ptr The new pointer (may be same as old).
 * @param new_size The new allocation size.
 */
void tml_mem_track_realloc(void* old_ptr, void* new_ptr, size_t new_size);

/**
 * @brief Checks for memory leaks and reports them.
 *
 * @return Number of leaked allocations (0 = no leaks).
 */
int32_t tml_mem_check_leaks(void);

/**
 * @brief Gets current memory statistics.
 *
 * @param stats Pointer to stats structure to fill.
 */
void tml_mem_get_stats(TmlMemStats* stats);

/**
 * @brief Prints memory statistics to stderr.
 */
void tml_mem_print_stats(void);

/**
 * @brief Enables or disables leak checking at exit.
 *
 * @param enable 1 to enable, 0 to disable.
 */
void tml_mem_set_check_at_exit(int32_t enable);

/**
 * @brief Sets the output stream for leak reports.
 *
 * @param fp FILE pointer (defaults to stderr).
 */
void tml_mem_set_output(void* fp);

// ============================================================================
// Tagged Allocation Macros
// ============================================================================

#ifdef TML_DEBUG_MEMORY

/**
 * @brief Tracked allocation with automatic tag.
 */
#define TML_ALLOC(size) tml_mem_alloc_tracked((size), __FILE__ ":" TML_STRINGIFY(__LINE__))
#define TML_ALLOC_ZEROED(size)                                                                     \
    tml_mem_alloc_zeroed_tracked((size), __FILE__ ":" TML_STRINGIFY(__LINE__))
#define TML_REALLOC(ptr, size)                                                                     \
    tml_mem_realloc_tracked((ptr), (size), __FILE__ ":" TML_STRINGIFY(__LINE__))
#define TML_FREE(ptr) tml_mem_free_tracked(ptr)

#define TML_STRINGIFY_HELPER(x) #x
#define TML_STRINGIFY(x) TML_STRINGIFY_HELPER(x)

#else

#define TML_ALLOC(size) mem_alloc(size)
#define TML_ALLOC_ZEROED(size) mem_alloc_zeroed(size)
#define TML_REALLOC(ptr, size) mem_realloc((ptr), (size))
#define TML_FREE(ptr) mem_free(ptr)

#endif

// ============================================================================
// Tracked Allocation Functions
// ============================================================================

/**
 * @brief Allocates memory with tracking.
 */
void* tml_mem_alloc_tracked(int64_t size, const char* tag);

/**
 * @brief Allocates zeroed memory with tracking.
 */
void* tml_mem_alloc_zeroed_tracked(int64_t size, const char* tag);

/**
 * @brief Reallocates memory with tracking.
 */
void* tml_mem_realloc_tracked(void* ptr, int64_t new_size, const char* tag);

/**
 * @brief Frees memory with tracking.
 */
void tml_mem_free_tracked(void* ptr);

#ifdef __cplusplus
}
#endif

#endif // TML_MEM_TRACK_H
