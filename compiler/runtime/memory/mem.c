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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <malloc.h>
#include <crtdbg.h>
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
#ifdef _WIN32
// Silent invalid-parameter handler for `_msize`. MSVC's default handler
// calls `abort()` in the debug CRT when passed a pointer that isn't
// owned by the heap; we install this no-op handler only when explicitly
// probing and then restore.
#include <stdlib.h>
static void tml_mem_silent_invalid_param(const wchar_t* expr, const wchar_t* fn,
                                          const wchar_t* file, unsigned line,
                                          uintptr_t reserved) {
    (void)expr; (void)fn; (void)file; (void)line; (void)reserved;
}

// Safe `_msize` wrapper — returns (size_t)-1 for any pointer that isn't
// owned by the CRT heap, without aborting or crashing.
//
// The robust pre-check is `HeapValidate(GetProcessHeap(), 0, ptr)`. It
// does NOT access memory outside the heap manager's internal tables,
// returns `FALSE` for any non-owned pointer, and does not raise
// exceptions. We still install the silent invalid_parameter_handler for
// the final `_msize` call because debug CRTs sometimes re-validate and
// invoke the handler even when the pointer is heap-valid (e.g. when the
// app links multiple CRT versions).
//
// `_set_thread_local_invalid_parameter_handler` alone was insufficient
// in phase 1k testing: on certain pointer values `_msize`'s internal
// heap-walk could still trigger silent memory probes that the debug CRT
// detected later, producing the `STATUS_HEAP_CORRUPTION` we observed.
static inline size_t tml_safe_msize(void* ptr) {
    if (!ptr) return (size_t)-1;
    if (!HeapValidate(GetProcessHeap(), 0, ptr)) return (size_t)-1;
    _invalid_parameter_handler old = _set_thread_local_invalid_parameter_handler(
        tml_mem_silent_invalid_param);
    size_t sz = _msize(ptr);
    _set_thread_local_invalid_parameter_handler(old);
    return sz;
}
#endif

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
// Length-prefixed String Buffers (phase1k)
// ============================================================================
//
// TML's `Str` is a null-terminated `ptr`. Literals live in `.rdata`; heap
// strings used to live in raw `malloc` blocks, so every `Str.len()` and
// every `str_append` had to `strlen(buf)` — O(|buf|) — making
// `s = s + "x"` loops O(n²) in total time (even after phase1i's amortized
// growth). Phase 1k caches the length in a 24-byte header:
//
//   [magic:i64 | cap:i64 | len:i64 | data[cap] | NUL]
//
// The pointer handed to user code points at `data`. The header lives at
// `ptr[-24..0)`. The underlying `malloc` pointer is `ptr - 24`.
// NUL-termination at `data[len]` is preserved so the buffer remains a
// valid C string — printf, strlen, strcmp all still work.
//
// **Discriminator (the crucial part).** TML code calls `str_append` with
// mixed-provenance pointers: literals in `.rdata`, buffers we allocated
// via `tml_str_alloc`, and legacy heap buffers from raw `mem_alloc` /
// `malloc` that predate this ABI. Reading `ptr - 24` blindly would be
// wrong for literals (different section) and dangerous for legacy heap
// buffers (could land on another allocation's user data and pass
// plausibility checks by accident — we observed STATUS_HEAP_CORRUPTION
// in practice before adding the magic).
//
// We solve this with a three-step check:
//   1. Image-range guard — literals in `.rdata`/`.text` return fast.
//   2. `_msize(ptr - 24)` — MSVC's `_msize` calls `HeapSize(heap, 0, p)`
//      which returns `(SIZE_T)-1` on any pointer not owned by the CRT
//      heap. This rejects arbitrary addresses without UB.
//   3. Magic sentinel — we write `TML_STR_MAGIC` at the start of every
//      allocation. If the first 8 bytes of the candidate header don't
//      equal the magic, the buffer wasn't ours; fall back to `strlen`.

#define TML_STR_HEADER_BYTES 24  // 8 magic + 8 cap + 8 len

// Arbitrary but distinctive 64-bit value — ASCII "TMLstrK1" (phase1K).
// Chance of a false positive on an 8-byte slice of random heap data is
// ~1/2^64, and the bytes are non-printable-mix enough to never appear in
// a source literal by accident.
#define TML_STR_MAGIC 0x314B7274735F4C4DULL  // "ML_strK1" little-endian

typedef struct {
    uint64_t magic;
    int64_t cap;
    int64_t len;
} TmlStrHeader;

// Fetch header pointer if `ptr` is one of our length-prefixed buffers.
// Returns NULL for literals, non-heap pointers, heap pointers not owned
// by us, or corrupted headers. Zero-UB on arbitrary input.
//
// On Windows we call `HeapSize(GetProcessHeap(), 0, raw)` directly
// instead of `_msize(raw)`. `_msize` invokes MSVC's
// `invalid_parameter_handler` when the pointer is not owned by the CRT
// heap — in the debug CRT that handler calls `abort()`, which is why
// phase1k's first iteration crashed with STATUS_HEAP_CORRUPTION the
// instant `str_append` received a legacy (non-prefixed) heap pointer.
// `HeapSize` has the same semantics but returns `(SIZE_T)-1` on invalid
// pointers without raising or aborting.
static inline TmlStrHeader* tml_str_header(void* ptr) {
    if (!ptr) return NULL;
#ifdef _WIN32
    if (!g_mem_image_ranges_initialized) mem_init_image_ranges();
    if (mem_is_image_ptr((uintptr_t)ptr)) return NULL;
    void* raw = (char*)ptr - TML_STR_HEADER_BYTES;
    // Fast-path magic check: the 8 bytes immediately before `ptr` are
    // always inside the CRT heap arena if `ptr` itself came from
    // `malloc` — the allocator packs a per-block header there (~16 B on
    // MSVC). So reading `*(uint64_t*)raw` is UB-free for any heap ptr,
    // and mismatched magic lets us reject 99% of the (common) non-
    // prefixed heap pointers in ~2 ns instead of paying for the
    // `HeapValidate` call (~100 ns). Only if the magic *happens* to
    // match do we validate via `tml_safe_msize` to defend against the
    // ~1/2^64 false-positive case where raw heap metadata resembles
    // our sentinel.
    TmlStrHeader* hdr = (TmlStrHeader*)raw;
    if (hdr->magic != TML_STR_MAGIC) return NULL;
    size_t block = tml_safe_msize(raw);
    if (block == (size_t)-1 || block < (size_t)TML_STR_HEADER_BYTES + 1) {
        return NULL;
    }
    if (hdr->cap < 0 || hdr->len < 0 || hdr->len > hdr->cap) return NULL;
    if ((size_t)hdr->cap + TML_STR_HEADER_BYTES + 1 > block) return NULL;
    return hdr;
#else
    void* raw = (char*)ptr - TML_STR_HEADER_BYTES;
#if defined(__APPLE__)
    size_t block = malloc_size(raw);
#elif defined(__GLIBC__) || defined(__linux__)
    size_t block = malloc_usable_size(raw);
#else
    size_t block = 0;
#endif
    if (block < (size_t)TML_STR_HEADER_BYTES + 1) return NULL;
    TmlStrHeader* hdr = (TmlStrHeader*)raw;
    if (hdr->magic != TML_STR_MAGIC) return NULL;
    if (hdr->cap < 0 || hdr->len < 0 || hdr->len > hdr->cap) return NULL;
    if ((size_t)hdr->cap + TML_STR_HEADER_BYTES + 1 > block) return NULL;
    return hdr;
#endif
}

/**
 * @brief Allocates a length-prefixed Str buffer.
 *
 * Layout: `[magic | cap | len | data... | NUL]`. Returns a pointer to
 * `data`. The buffer is sized for `cap` data bytes plus the trailing NUL;
 * the caller is expected to write at most `cap` bytes and then call
 * `tml_str_set_len` to record the written length. The initial len is 0
 * and `data[0]` is written as NUL so the buffer is already a valid empty
 * C string.
 */
TML_EXPORT void* tml_str_alloc(int64_t cap) {
    if (cap < 0) cap = 0;
    size_t total = (size_t)cap + TML_STR_HEADER_BYTES + 1;
    void* raw = mem_alloc((int64_t)total);
    if (!raw) return NULL;
    TmlStrHeader* hdr = (TmlStrHeader*)raw;
    hdr->magic = TML_STR_MAGIC;
    hdr->cap = cap;
    hdr->len = 0;
    char* data = (char*)raw + TML_STR_HEADER_BYTES;
    data[0] = '\0';
    return data;
}

/**
 * @brief Allocates with a pre-filled length (producers that know the exact
 * size and will fill the buffer before returning). `cap == len` on return.
 */
TML_EXPORT void* tml_str_alloc_len(int64_t len) {
    if (len < 0) len = 0;
    size_t total = (size_t)len + TML_STR_HEADER_BYTES + 1;
    void* raw = mem_alloc((int64_t)total);
    if (!raw) return NULL;
    TmlStrHeader* hdr = (TmlStrHeader*)raw;
    hdr->magic = TML_STR_MAGIC;
    hdr->cap = len;
    hdr->len = len;
    char* data = (char*)raw + TML_STR_HEADER_BYTES;
    data[len] = '\0';
    {
        FILE* f = fopen("F:/Node/hivellm/tml/.sandbox/alloc_trace.log", "a");
        if (f) { fprintf(f, "[alloc_len] len=%lld data=%p raw=%p\n", (long long)len, data, raw); fclose(f); }
    }
    return data;
}

/**
 * @brief Allocates with explicit capacity and initial length (builder pattern).
 */
TML_EXPORT void* tml_str_alloc_with_cap(int64_t len, int64_t cap) {
    if (len < 0) len = 0;
    if (cap < len) cap = len;
    size_t total = (size_t)cap + TML_STR_HEADER_BYTES + 1;
    void* raw = mem_alloc((int64_t)total);
    if (!raw) return NULL;
    TmlStrHeader* hdr = (TmlStrHeader*)raw;
    hdr->magic = TML_STR_MAGIC;
    hdr->cap = cap;
    hdr->len = len;
    char* data = (char*)raw + TML_STR_HEADER_BYTES;
    data[len] = '\0';
    return data;
}

/**
 * @brief Reads the cached length of a Str in O(1) for our heap strings.
 *
 * Literals and legacy raw-heap strings (magic check fails) fall back to
 * `strlen`. The `tml_str_header` helper performs all the UB-safe gating.
 */
TML_EXPORT int64_t tml_str_len(void* ptr) {
    if (!ptr) return 0;
    TmlStrHeader* hdr = tml_str_header(ptr);
    if (hdr) return hdr->len;
    return (int64_t)strlen((const char*)ptr);
}

/**
 * @brief Updates the cached length after the caller wrote new data.
 * No-op for literals / legacy heap strings.
 */
TML_EXPORT void tml_str_set_len(void* ptr, int64_t new_len) {
    if (!ptr || new_len < 0) return;
    TmlStrHeader* hdr = tml_str_header(ptr);
    if (!hdr) return;
    if (new_len > hdr->cap) return;
    hdr->len = new_len;
}

/**
 * @brief Reads the capacity. Returns 0 for anything that isn't one of our
 * length-prefixed buffers — callers use `cap > 0` as a discriminator.
 */
TML_EXPORT int64_t tml_str_cap(void* ptr) {
    if (!ptr) return 0;
    TmlStrHeader* hdr = tml_str_header(ptr);
    if (!hdr) return 0;
    return hdr->cap;
}

/**
 * @brief Reallocates a length-prefixed Str to a larger capacity.
 *
 * For non-prefixed inputs, promotes them into a fresh prefixed buffer:
 * reads the current length via `strlen`, allocates a new prefixed
 * buffer, memcpys the data, and returns the new pointer. The old buffer
 * is NOT freed — the caller retains ownership of it (the common case
 * is `a` is a literal or a buffer we'll free separately).
 */
TML_EXPORT void* tml_str_realloc(void* ptr, int64_t new_cap) {
    if (!ptr) return tml_str_alloc(new_cap);
    if (new_cap < 0) new_cap = 0;

    TmlStrHeader* hdr = tml_str_header(ptr);
    if (!hdr) {
        // Non-prefixed: literal or legacy heap buffer. Promote.
        size_t cur_len = strlen((const char*)ptr);
        if (new_cap < (int64_t)cur_len) new_cap = (int64_t)cur_len;
        void* fresh = tml_str_alloc_with_cap((int64_t)cur_len, new_cap);
        if (!fresh) return NULL;
        if (cur_len > 0) memcpy(fresh, ptr, cur_len);
        ((char*)fresh)[cur_len] = '\0';
        return fresh;
    }

    int64_t cur_len = hdr->len;
    if (new_cap < cur_len) new_cap = cur_len;
    void* raw = (char*)ptr - TML_STR_HEADER_BYTES;
    size_t total = (size_t)new_cap + TML_STR_HEADER_BYTES + 1;
    void* new_raw = mem_realloc(raw, (int64_t)total);
    if (!new_raw) return NULL;
    TmlStrHeader* new_hdr = (TmlStrHeader*)new_raw;
    // magic is preserved by realloc's memcpy; just bump cap.
    new_hdr->cap = new_cap;
    // len preserved.
    char* data = (char*)new_raw + TML_STR_HEADER_BYTES;
    data[cur_len] = '\0';
    return data;
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
