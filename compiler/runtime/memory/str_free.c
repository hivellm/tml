/**
 * @file str_free.c
 * @brief Safe string deallocation with PE image range checking.
 *
 * Validates whether a pointer is a heap allocation or a string constant
 * (.rdata) before freeing. Uses binary search over cached PE image ranges
 * on Windows, malloc_usable_size on Linux, malloc_size on macOS.
 *
 * Extracted from essential.c — private statics, no linkage changes.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

/**
 * @brief Safely frees a Str pointer if it is heap-allocated.
 *
 * TML Str values are raw `ptr` (char*). Some point to global string
 * constants (.rdata section), others to heap-allocated buffers from
 * str_concat_opt, interpolation, etc. This function validates the
 * pointer is a valid heap allocation before calling free().
 *
 * On Windows: uses PE image range check (~1ns) instead of HeapValidate (~100ns).
 * String constants live in the PE image's .rdata section. Any pointer within
 * the image range is a constant, any pointer outside is a heap allocation.
 *
 * On POSIX: uses malloc_usable_size() which returns 0 for non-heap pointers.
 *
 * @param ptr Pointer to potentially heap-allocated string.
 */

#ifdef _WIN32
#include <psapi.h>
#pragma comment(lib, "psapi.lib")

// Cached image ranges for all loaded modules (exe + DLLs).
// String constants (.rdata) live within these ranges; heap allocations do not.
// Sorted by base address for binary search.
typedef struct {
    uintptr_t base;
    uintptr_t end;
} ImageRange;

#define MAX_IMAGE_RANGES 128
static ImageRange tml_image_ranges[MAX_IMAGE_RANGES];
static int tml_image_range_count = 0;
static volatile int tml_image_ranges_initialized = 0;

// Comparison for qsort
static int image_range_cmp(const void* a, const void* b) {
    const ImageRange* ra = (const ImageRange*)a;
    const ImageRange* rb = (const ImageRange*)b;
    if (ra->base < rb->base)
        return -1;
    if (ra->base > rb->base)
        return 1;
    return 0;
}

static void tml_str_free_init_image_ranges(void) {
    HMODULE modules[MAX_IMAGE_RANGES];
    DWORD needed = 0;
    HANDLE proc = GetCurrentProcess();

    if (EnumProcessModules(proc, modules, sizeof(modules), &needed)) {
        int count = (int)(needed / sizeof(HMODULE));
        if (count > MAX_IMAGE_RANGES)
            count = MAX_IMAGE_RANGES;

        for (int i = 0; i < count; i++) {
            MODULEINFO mi;
            if (GetModuleInformation(proc, modules[i], &mi, sizeof(mi))) {
                tml_image_ranges[tml_image_range_count].base = (uintptr_t)mi.lpBaseOfDll;
                tml_image_ranges[tml_image_range_count].end =
                    (uintptr_t)mi.lpBaseOfDll + mi.SizeOfImage;
                tml_image_range_count++;
            }
        }
        // Sort for binary search
        qsort(tml_image_ranges, tml_image_range_count, sizeof(ImageRange), image_range_cmp);
    }
    tml_image_ranges_initialized = 1;
}

// Register a new module (called when test DLLs are loaded after init).
TML_EXPORT void tml_str_free_register_module(void* module_handle) {
    if (!module_handle || tml_image_range_count >= MAX_IMAGE_RANGES)
        return;
    MODULEINFO mi;
    if (GetModuleInformation(GetCurrentProcess(), (HMODULE)module_handle, &mi, sizeof(mi))) {
        tml_image_ranges[tml_image_range_count].base = (uintptr_t)mi.lpBaseOfDll;
        tml_image_ranges[tml_image_range_count].end = (uintptr_t)mi.lpBaseOfDll + mi.SizeOfImage;
        tml_image_range_count++;
        // Re-sort after insertion
        qsort(tml_image_ranges, tml_image_range_count, sizeof(ImageRange), image_range_cmp);
    }
}

// Binary search: is this address within any loaded module's image?
static inline int tml_is_image_ptr(uintptr_t addr) {
    int lo = 0, hi = tml_image_range_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (addr < tml_image_ranges[mid].base) {
            hi = mid - 1;
        } else if (addr >= tml_image_ranges[mid].end) {
            lo = mid + 1;
        } else {
            return 1; // Within this module's image
        }
    }
    return 0;
}
#endif

// Phase 1k: heap Str buffers are allocated with a 24-byte header
// ([magic | cap | len]) at `ptr[-24..0)`. `tml_str_free` detects the
// convention via `tml_str_cap(ptr) > 0` (which internally checks the
// magic, protecting us from false-positive frees). Legacy raw-malloc
// heap strings (cap == 0) are freed at `ptr` directly.
#define TML_STR_HEADER_BYTES 24
extern int64_t tml_str_cap(void* ptr);

TML_EXPORT void tml_str_free(void* ptr) {
    if (!ptr)
        return;
    // Use mem_free (declared in mem.c, linked into same binary) instead of
    // raw free() so that the memory tracker deregisters this allocation.
    // Without this, tml_str_free would bypass tracking and cause false-positive
    // leak reports in coverage/debug mode.
    extern void mem_free(void*);
#ifdef _WIN32
    if (!tml_image_ranges_initialized) {
        tml_str_free_init_image_ranges();
    }
    if (tml_is_image_ptr((uintptr_t)ptr)) {
        return; // String constant in .rdata — do not free
    }
    int64_t cap = tml_str_cap(ptr);
    if (cap > 0) {
        mem_free((char*)ptr - TML_STR_HEADER_BYTES);
    } else {
        mem_free(ptr);
    }
#elif defined(__GLIBC__) || defined(__linux__)
    extern size_t malloc_usable_size(void*);
    if (malloc_usable_size(ptr) > 0) {
        int64_t cap = tml_str_cap(ptr);
        if (cap > 0) {
            mem_free((char*)ptr - TML_STR_HEADER_BYTES);
        } else {
            mem_free(ptr);
        }
    }
#elif defined(__APPLE__)
    extern size_t malloc_size(const void*);
    if (malloc_size(ptr) > 0) {
        int64_t cap = tml_str_cap(ptr);
        if (cap > 0) {
            mem_free((char*)ptr - TML_STR_HEADER_BYTES);
        } else {
            mem_free(ptr);
        }
    }
#else
    (void)ptr;
#endif
}
