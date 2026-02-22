/**
 * @file backtrace.c
 * @brief TML Runtime - Stack Backtrace Implementation
 *
 * Cross-platform stack trace capture and symbol resolution.
 * Windows implementation uses RtlCaptureStackBackTrace + DbgHelp.
 */

#include "backtrace.h"

#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Use mem_alloc/mem_free so the memory tracker can track these allocations
extern void* mem_alloc(int64_t);
extern void mem_free(void*);

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// NOTE: dbghelp.h intentionally NOT included. DbgHelp APIs (SymFromAddr,
// SymGetLineFromAddr64, SymInitialize) can hang indefinitely in processes
// that load/unload many DLLs (e.g., test suites). We use only
// RtlCaptureStackBackTrace for frame capture and format addresses as hex.

// Defined in essential.c - suppresses VEH interception so our SEH
// __try/__except around RtlCaptureStackBackTrace works correctly.
extern volatile int32_t tml_veh_suppressed;
#else
#include <dlfcn.h>
#include <execinfo.h>
#endif

// ============================================================================
// Static State
// ============================================================================

static int32_t g_initialized = 0;
#ifdef _WIN32
static HANDLE g_process = NULL;
// NOTE: DbgHelp symbol resolution (SymFromAddr etc.) has been disabled because
// it hangs in test suites with many DLL load/unload cycles. We only use
// RtlCaptureStackBackTrace for frame capture and format addresses as hex.
#endif

// ============================================================================
// Global Symbol Cache
// ============================================================================

/** Cache entry mapping instruction pointer to resolved symbol */
typedef struct SymbolCacheEntry {
    void* ip;                      /** Instruction pointer (key) */
    BacktraceSymbol symbol;        /** Resolved symbol data */
    struct SymbolCacheEntry* next; /** Chaining for collisions */
} SymbolCacheEntry;

/** Simple hash table for symbol caching */
#define SYMBOL_CACHE_SIZE 256
static SymbolCacheEntry* g_symbol_cache[SYMBOL_CACHE_SIZE] = {NULL};
static int32_t g_cache_enabled = 1; /** Enable/disable caching */

/** Hash function for instruction pointers */
static uint32_t symbol_cache_hash(void* ip) {
    uintptr_t addr = (uintptr_t)ip;
    // Simple hash: mix upper and lower bits
    return (uint32_t)((addr >> 8) ^ addr) % SYMBOL_CACHE_SIZE;
}

/** Lookup symbol in cache, returns NULL if not found */
static BacktraceSymbol* symbol_cache_lookup(void* ip) {
    if (!g_cache_enabled || !ip) {
        return NULL;
    }

    uint32_t hash = symbol_cache_hash(ip);
    SymbolCacheEntry* entry = g_symbol_cache[hash];

    while (entry) {
        if (entry->ip == ip) {
            return &entry->symbol;
        }
        entry = entry->next;
    }

    return NULL;
}

/** Insert symbol into cache (makes copies of strings) */
static void symbol_cache_insert(void* ip, const BacktraceSymbol* symbol) {
    if (!g_cache_enabled || !ip || !symbol) {
        return;
    }

    uint32_t hash = symbol_cache_hash(ip);

    // Check if already exists
    SymbolCacheEntry* existing = g_symbol_cache[hash];
    while (existing) {
        if (existing->ip == ip) {
            return; // Already cached
        }
        existing = existing->next;
    }

    // Allocate new entry
    SymbolCacheEntry* entry = (SymbolCacheEntry*)malloc(sizeof(SymbolCacheEntry));
    if (!entry) {
        return; // Cache insertion failure is non-fatal
    }

    entry->ip = ip;
    entry->symbol.name = symbol->name ? _strdup(symbol->name) : NULL;
    entry->symbol.filename = symbol->filename ? _strdup(symbol->filename) : NULL;
    entry->symbol.lineno = symbol->lineno;
    entry->symbol.colno = symbol->colno;
    entry->symbol.symbol_address = symbol->symbol_address;
    entry->symbol.offset = symbol->offset;

    // Insert at head of chain
    entry->next = g_symbol_cache[hash];
    g_symbol_cache[hash] = entry;
}

/** Clear entire symbol cache */
static void symbol_cache_clear(void) {
    for (int i = 0; i < SYMBOL_CACHE_SIZE; i++) {
        SymbolCacheEntry* entry = g_symbol_cache[i];
        while (entry) {
            SymbolCacheEntry* next = entry->next;
            if (entry->symbol.name) {
                free(entry->symbol.name);
            }
            if (entry->symbol.filename) {
                free(entry->symbol.filename);
            }
            free(entry);
            entry = next;
        }
        g_symbol_cache[i] = NULL;
    }
}

// ============================================================================
// Initialization
// ============================================================================

int32_t backtrace_init(void) {
    if (g_initialized) {
        return 0;
    }

#ifdef _WIN32
    g_process = GetCurrentProcess();

    // NOTE: We intentionally do NOT call SymInitialize/SymSetOptions here.
    // DbgHelp symbol resolution (SymFromAddr, SymGetLineFromAddr64) has been
    // disabled because it can hang indefinitely in test suites that load/unload
    // hundreds of DLLs. Stack capture via RtlCaptureStackBackTrace works without
    // DbgHelp initialization.
#endif

    g_initialized = 1;
    return 0;
}

void backtrace_cleanup(void) {
    if (!g_initialized) {
        return;
    }

#ifdef _WIN32
    g_process = NULL;
    // No SymCleanup needed since we don't call SymInitialize
#endif

    // Clear global symbol cache
    symbol_cache_clear();

    g_initialized = 0;
}

// ============================================================================
// Capture Functions
// ============================================================================

int32_t backtrace_capture(void** frames, int32_t max_frames, int32_t skip) {
    if (!frames || max_frames <= 0) {
        return -1;
    }

#ifdef _WIN32
    // Wrap RtlCaptureStackBackTrace in SEH (__try/__except) because it can
    // cause ACCESS_VIOLATION if the stack is corrupted, frame pointers are
    // invalid, or guard pages are hit. This is especially common in DLL-based
    // test execution where stack state may be unusual.
    //
    // Suppress VEH interception so the exception is handled by our SEH
    // rather than being caught by the VEH handler in essential.c.
    tml_veh_suppressed = 1;
    __try {
        // First, capture all frames to know the total
        void* temp_frames[128];
        USHORT total_frames = RtlCaptureStackBackTrace(0, 128, temp_frames, NULL);

        // Skip this function plus user-specified skip
        int32_t total_skip = skip + 1;

        // Ensure we don't skip more frames than available
        if (total_skip >= (int32_t)total_frames) {
            total_skip = total_frames > 0 ? total_frames - 1 : 0;
        }

        USHORT captured =
            RtlCaptureStackBackTrace((ULONG)total_skip, (ULONG)max_frames, frames, NULL);
        tml_veh_suppressed = 0;
        return (int32_t)captured;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Stack walking failed - return empty backtrace instead of crashing
        tml_veh_suppressed = 0;
        return 0;
    }
#else
    // Unix: use backtrace() from execinfo.h
    int captured = backtrace(frames, max_frames);
    if (captured <= 0) {
        return -1;
    }

    // Handle skip by shifting the array
    if (skip > 0 && skip < captured) {
        int remaining = captured - skip - 1; // -1 for this function
        if (remaining > 0) {
            memmove(frames, frames + skip + 1, remaining * sizeof(void*));
            return remaining;
        }
        return 0;
    }

    return captured - 1; // Skip this function
#endif
}

Backtrace* backtrace_capture_full(int32_t skip) {
    // Auto-initialize on first use
    if (!g_initialized) {
        if (backtrace_init() != 0) {
            return NULL;
        }
    }

    // Temporary array for raw addresses
    // Skip internal C frames (backtrace_capture_full, ffi_backtrace_capture)
    void* raw_frames[BACKTRACE_MAX_FRAMES];
    int32_t count = backtrace_capture(raw_frames, BACKTRACE_MAX_FRAMES, skip);

    if (count <= 0) {
        // Return an empty but valid Backtrace instead of NULL when count is 0.
        // This prevents callers from getting NULL which could cascade errors.
        Backtrace* empty = (Backtrace*)calloc(1, sizeof(Backtrace));
        if (!empty)
            return NULL;
        empty->capacity = 0;
        empty->frame_count = 0;
        empty->frames = NULL;
        return empty;
    }

    Backtrace* bt = (Backtrace*)calloc(1, sizeof(Backtrace));
    if (!bt) {
        return NULL;
    }

    // Lazy allocation: only allocate the exact number of frames needed
    // This saves memory compared to always allocating BACKTRACE_MAX_FRAMES (128)
    bt->capacity = count;
    bt->frame_count = count;
    bt->frames = (BacktraceFrame*)calloc(count, sizeof(BacktraceFrame));
    if (!bt->frames) {
        free(bt);
        return NULL;
    }

    for (int32_t i = 0; i < count; i++) {
        bt->frames[i].ip = raw_frames[i];
        bt->frames[i].sp = NULL;
        bt->frames[i].resolved = 0;
        memset(&bt->frames[i].symbol, 0, sizeof(BacktraceSymbol));
    }

    return bt;
}

// ============================================================================
// Resolution Functions
// ============================================================================

int32_t backtrace_resolve(void* addr, BacktraceSymbol* out) {
    if (!addr || !out) {
        return -1;
    }

    // Auto-initialize
    if (!g_initialized) {
        if (backtrace_init() != 0) {
            return -1;
        }
    }

    memset(out, 0, sizeof(BacktraceSymbol));

    // Check global cache first (major performance optimization)
    BacktraceSymbol* cached = symbol_cache_lookup(addr);
    if (cached) {
        // Copy cached data (caller expects to own the strings)
        out->name = cached->name ? _strdup(cached->name) : NULL;
        out->filename = cached->filename ? _strdup(cached->filename) : NULL;
        out->lineno = cached->lineno;
        out->colno = cached->colno;
        out->symbol_address = cached->symbol_address;
        out->offset = cached->offset;
        return (out->name != NULL) ? 0 : -1;
    }

#ifdef _WIN32
    // IMPORTANT: We intentionally skip DbgHelp symbol resolution (SymFromAddr,
    // SymGetLineFromAddr64) entirely. These APIs can hang indefinitely when
    // the process has loaded/unloaded many DLLs (e.g., test suite running
    // 300+ test DLLs). Even with SYMOPT_DEFERRED_LOADS, SymFromAddr triggers
    // lazy PDB loading which can block on disk I/O or symbol server access.
    //
    // Instead, we provide address-only information. The backtrace still captures
    // all stack frames via RtlCaptureStackBackTrace (which is fast and safe),
    // and formatting shows hex addresses instead of function names.
    {
        char addr_buf[32];
        snprintf(addr_buf, sizeof(addr_buf), "0x%llX", (unsigned long long)(uintptr_t)addr);
        out->name = _strdup(addr_buf);
        out->symbol_address = addr;
        out->offset = 0;
    }

    // Cache the result
    if (out->name) {
        symbol_cache_insert(addr, out);
    }

    return 0;
#else
    // Unix: use dladdr for basic symbol info
    Dl_info info;
    if (dladdr(addr, &info)) {
        if (info.dli_sname) {
            out->name = strdup(info.dli_sname);
        }
        if (info.dli_fname) {
            out->filename = strdup(info.dli_fname);
        }
        out->symbol_address = info.dli_saddr;
        if (info.dli_saddr) {
            out->offset = (uint64_t)addr - (uint64_t)info.dli_saddr;
        }

        // Cache the result for future lookups
        if (out->name) {
            symbol_cache_insert(addr, out);
        }

        return (out->name != NULL) ? 0 : -1;
    }
    return -1;
#endif
}

int32_t backtrace_resolve_all(Backtrace* bt) {
    if (!bt || !bt->frames) {
        return 0;
    }

    int32_t resolved_count = 0;
    for (int32_t i = 0; i < bt->frame_count; i++) {
        if (!bt->frames[i].resolved) {
            if (backtrace_resolve(bt->frames[i].ip, &bt->frames[i].symbol) == 0) {
                bt->frames[i].resolved = 1;
                resolved_count++;
            }
        } else {
            resolved_count++;
        }
    }

    bt->fully_resolved = (resolved_count == bt->frame_count);
    return resolved_count;
}

// ============================================================================
// Internal Frame Detection
// ============================================================================

/**
 * @brief Check if a frame should be filtered out as internal/runtime frame.
 *
 * Filters out:
 * - Runtime panic/assert functions (panic, assert_tml, assert_tml_loc)
 * - Test framework internals (tml_run_test_with_catch, tml_test_)
 * - Backtrace capture internals (backtrace_capture, backtrace_format)
 * - System functions (longjmp, setjmp, RaiseException)
 *
 * @param frame The frame to check.
 * @return 1 if frame should be filtered, 0 otherwise.
 */
static int32_t is_internal_frame(const BacktraceFrame* frame) {
    if (!frame || !frame->resolved || !frame->symbol.name) {
        // Don't filter unknown frames - they might be user code without symbols
        return 0;
    }

    const char* name = frame->symbol.name;

    // Runtime panic/assert functions
    if (strstr(name, "panic") != NULL)
        return 1;
    if (strstr(name, "assert_tml") != NULL)
        return 1;

    // Test framework internals
    if (strstr(name, "tml_run_test") != NULL)
        return 1;
    if (strncmp(name, "tml_test_", 9) == 0)
        return 1;

    // Backtrace internals
    if (strstr(name, "backtrace_capture") != NULL)
        return 1;
    if (strstr(name, "backtrace_resolve") != NULL)
        return 1;
    if (strstr(name, "backtrace_format") != NULL)
        return 1;

    // System/runtime functions
    if (strstr(name, "longjmp") != NULL)
        return 1;
    if (strstr(name, "setjmp") != NULL)
        return 1;
    if (strstr(name, "_setjmpex") != NULL)
        return 1;
#ifdef _WIN32
    if (strstr(name, "RaiseException") != NULL)
        return 1;
    if (strstr(name, "RtlRaiseException") != NULL)
        return 1;
    if (strstr(name, "RtlCaptureStackBackTrace") != NULL)
        return 1;
#endif

    // CRT startup/main wrappers
    if (strcmp(name, "__scrt_common_main_seh") == 0)
        return 1;
    if (strcmp(name, "invoke_main") == 0)
        return 1;
    if (strcmp(name, "__libc_start_main") == 0)
        return 1;
    if (strcmp(name, "_start") == 0)
        return 1;

    return 0;
}

// ============================================================================
// Formatting Functions
// ============================================================================

int32_t backtrace_frame_format(const BacktraceFrame* frame, int32_t index, char* buffer,
                               int32_t buffer_size) {
    if (!frame || !buffer || buffer_size <= 0) {
        return -1;
    }

    const char* name = frame->resolved && frame->symbol.name ? frame->symbol.name : "<unknown>";
    const char* filename =
        frame->resolved && frame->symbol.filename ? frame->symbol.filename : "<unknown>";
    uint32_t lineno = frame->resolved ? frame->symbol.lineno : 0;

    int written;
    if (lineno > 0) {
        written = snprintf(buffer, buffer_size, "  %2d: %s\n             at %s:%u", index, name,
                           filename, lineno);
    } else if (frame->symbol.filename) {
        written =
            snprintf(buffer, buffer_size, "  %2d: %s\n             at %s", index, name, filename);
    } else {
        written =
            snprintf(buffer, buffer_size, "  %2d: %s\n             at %p", index, name, frame->ip);
    }

    return (written >= 0 && written < buffer_size) ? written : -1;
}

char* backtrace_format(const Backtrace* bt) {
    if (!bt || bt->frame_count == 0) {
        return _strdup("  <empty backtrace>\n");
    }

    // Optimized: More accurate size estimation
    // Average frame: "  NN: function_name\n             at file:line\n" = ~150 bytes
    // Account for filtered frames by using actual count
    size_t buffer_size = (size_t)bt->frame_count * 180 + 128;
    char* result = (char*)mem_alloc((int64_t)buffer_size);
    if (!result) {
        return NULL;
    }

    char* ptr = result;
    size_t remaining = buffer_size;
    int32_t display_index = 0; // Separate counter for displayed frames

    for (int32_t i = 0; i < bt->frame_count; i++) {
        // Skip internal/runtime frames
        if (is_internal_frame(&bt->frames[i])) {
            continue;
        }

        // Optimized: Write directly to result buffer, eliminating intermediate copy
        const BacktraceFrame* frame = &bt->frames[i];
        const char* name = frame->resolved && frame->symbol.name ? frame->symbol.name : "<unknown>";
        const char* filename =
            frame->resolved && frame->symbol.filename ? frame->symbol.filename : "<unknown>";
        uint32_t lineno = frame->resolved ? frame->symbol.lineno : 0;

        int written;
        if (lineno > 0) {
            written = snprintf(ptr, remaining, "  %2d: %s\n             at %s:%u\n", display_index,
                               name, filename, lineno);
        } else if (frame->symbol.filename) {
            written = snprintf(ptr, remaining, "  %2d: %s\n             at %s\n", display_index,
                               name, filename);
        } else {
            written = snprintf(ptr, remaining, "  %2d: %s\n             at %p\n", display_index,
                               name, frame->ip);
        }

        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
            display_index++;
        }
    }

    // If all frames were filtered, show a message
    if (display_index == 0) {
        mem_free(result);
        return _strdup("  <all frames filtered as internal>\n");
    }

    return result;
}

char* backtrace_format_json(const Backtrace* bt) {
    if (!bt || bt->frame_count == 0) {
        return _strdup("[]");
    }

    // Estimate buffer size: each JSON frame object ~256 bytes
    size_t buffer_size = (size_t)bt->frame_count * 300 + 64;
    char* result = (char*)mem_alloc((int64_t)buffer_size);
    if (!result) {
        return NULL;
    }

    char* ptr = result;
    size_t remaining = buffer_size;
    int written = snprintf(ptr, remaining, "[");
    if (written > 0) {
        ptr += written;
        remaining -= written;
    }

    int32_t display_index = 0;

    for (int32_t i = 0; i < bt->frame_count; i++) {
        if (is_internal_frame(&bt->frames[i])) {
            continue;
        }

        const BacktraceFrame* frame = &bt->frames[i];
        const char* name = frame->resolved && frame->symbol.name ? frame->symbol.name : "<unknown>";
        const char* filename =
            frame->resolved && frame->symbol.filename ? frame->symbol.filename : NULL;
        uint32_t lineno = frame->resolved ? frame->symbol.lineno : 0;

        // Add comma separator between frames
        if (display_index > 0) {
            written = snprintf(ptr, remaining, ",");
            if (written > 0) {
                ptr += written;
                remaining -= written;
            }
        }

        // Escape JSON strings (name, filename)
        // Simple approach: just escape backslashes and quotes
        char escaped_name[512];
        char escaped_file[512];
        {
            size_t j = 0;
            for (const char* s = name; *s && j < sizeof(escaped_name) - 2; s++) {
                if (*s == '"' || *s == '\\') {
                    escaped_name[j++] = '\\';
                }
                escaped_name[j++] = *s;
            }
            escaped_name[j] = '\0';
        }
        if (filename) {
            size_t j = 0;
            for (const char* s = filename; *s && j < sizeof(escaped_file) - 2; s++) {
                if (*s == '"' || *s == '\\') {
                    escaped_file[j++] = '\\';
                }
                escaped_file[j++] = *s;
            }
            escaped_file[j] = '\0';
        }

        if (filename && lineno > 0) {
            written = snprintf(
                ptr, remaining,
                "{\"index\":%d,\"name\":\"%s\",\"file\":\"%s\",\"line\":%u,\"addr\":\"%p\"}",
                display_index, escaped_name, escaped_file, lineno, frame->ip);
        } else if (filename) {
            written = snprintf(
                ptr, remaining,
                "{\"index\":%d,\"name\":\"%s\",\"file\":\"%s\",\"line\":0,\"addr\":\"%p\"}",
                display_index, escaped_name, escaped_file, frame->ip);
        } else {
            written =
                snprintf(ptr, remaining,
                         "{\"index\":%d,\"name\":\"%s\",\"file\":null,\"line\":0,\"addr\":\"%p\"}",
                         display_index, escaped_name, frame->ip);
        }

        if (written > 0 && (size_t)written < remaining) {
            ptr += written;
            remaining -= written;
            display_index++;
        }
    }

    written = snprintf(ptr, remaining, "]");
    if (written > 0) {
        ptr += written;
        remaining -= written;
    }

    if (display_index == 0) {
        mem_free(result);
        return _strdup("[]");
    }

    return result;
}

void backtrace_print(int32_t skip) {
    Backtrace* bt = backtrace_capture_full(skip + 1); // +1 for this function
    if (!bt) {
        RT_WARN("runtime", "Failed to capture backtrace");
        return;
    }

    backtrace_resolve_all(bt);

    char* formatted = backtrace_format(bt);
    if (formatted) {
        RT_ERROR("runtime", "%s", formatted);
        mem_free(formatted);
    }

    backtrace_free(bt);
}

// ============================================================================
// Memory Management
// ============================================================================

void backtrace_symbol_free(BacktraceSymbol* sym) {
    if (!sym) {
        return;
    }
    if (sym->name) {
        free(sym->name);
        sym->name = NULL;
    }
    if (sym->filename) {
        free(sym->filename);
        sym->filename = NULL;
    }
}

void backtrace_free(Backtrace* bt) {
    if (!bt) {
        return;
    }
    if (bt->frames) {
        for (int32_t i = 0; i < bt->frame_count; i++) {
            backtrace_symbol_free(&bt->frames[i].symbol);
        }
        free(bt->frames);
    }
    free(bt);
}

// ============================================================================
// FFI Exports for TML
// ============================================================================

TML_EXPORT void* ffi_backtrace_capture(int32_t skip) {
    // The TML caller already accounts for its own frame skip.
    // We just pass through to the C implementation which handles its own skipping.
    return (void*)backtrace_capture_full(skip);
}

TML_EXPORT int32_t ffi_backtrace_frame_count(void* bt_handle) {
    Backtrace* bt = (Backtrace*)bt_handle;
    return bt ? bt->frame_count : 0;
}

TML_EXPORT void* ffi_backtrace_frame_ip(void* bt_handle, int32_t index) {
    Backtrace* bt = (Backtrace*)bt_handle;
    if (!bt || index < 0 || index >= bt->frame_count) {
        return NULL;
    }
    return bt->frames[index].ip;
}

TML_EXPORT void ffi_backtrace_resolve(void* bt_handle) {
    Backtrace* bt = (Backtrace*)bt_handle;
    if (bt) {
        backtrace_resolve_all(bt);
    }
}

TML_EXPORT const char* ffi_backtrace_frame_name(void* bt_handle, int32_t index) {
    Backtrace* bt = (Backtrace*)bt_handle;
    if (!bt || index < 0 || index >= bt->frame_count) {
        return NULL;
    }
    if (!bt->frames[index].resolved) {
        return NULL;
    }
    return bt->frames[index].symbol.name;
}

TML_EXPORT const char* ffi_backtrace_frame_filename(void* bt_handle, int32_t index) {
    Backtrace* bt = (Backtrace*)bt_handle;
    if (!bt || index < 0 || index >= bt->frame_count) {
        return NULL;
    }
    if (!bt->frames[index].resolved) {
        return NULL;
    }
    return bt->frames[index].symbol.filename;
}

TML_EXPORT uint32_t ffi_backtrace_frame_lineno(void* bt_handle, int32_t index) {
    Backtrace* bt = (Backtrace*)bt_handle;
    if (!bt || index < 0 || index >= bt->frame_count) {
        return 0;
    }
    if (!bt->frames[index].resolved) {
        return 0;
    }
    return bt->frames[index].symbol.lineno;
}

TML_EXPORT char* ffi_backtrace_to_string(void* bt_handle) {
    Backtrace* bt = (Backtrace*)bt_handle;
    if (!bt) {
        return NULL;
    }
    if (!bt->fully_resolved) {
        backtrace_resolve_all(bt);
    }
    return backtrace_format(bt);
}

TML_EXPORT void ffi_backtrace_free(void* bt_handle) {
    backtrace_free((Backtrace*)bt_handle);
}

TML_EXPORT uint32_t ffi_backtrace_frame_colno(void* bt_handle, int32_t index) {
    Backtrace* bt = (Backtrace*)bt_handle;
    if (!bt || index < 0 || index >= bt->frame_count) {
        return 0;
    }
    if (!bt->frames[index].resolved) {
        return 0;
    }
    return bt->frames[index].symbol.colno;
}

TML_EXPORT void* ffi_backtrace_frame_symbol_address(void* bt_handle, int32_t index) {
    Backtrace* bt = (Backtrace*)bt_handle;
    if (!bt || index < 0 || index >= bt->frame_count) {
        return NULL;
    }
    if (!bt->frames[index].resolved) {
        return NULL;
    }
    return bt->frames[index].symbol.symbol_address;
}

TML_EXPORT uint64_t ffi_backtrace_frame_offset(void* bt_handle, int32_t index) {
    Backtrace* bt = (Backtrace*)bt_handle;
    if (!bt || index < 0 || index >= bt->frame_count) {
        return 0;
    }
    if (!bt->frames[index].resolved) {
        return 0;
    }
    return bt->frames[index].symbol.offset;
}

TML_EXPORT int32_t ffi_backtrace_is_resolved(void* bt_handle) {
    Backtrace* bt = (Backtrace*)bt_handle;
    if (!bt) {
        return 0;
    }
    return bt->fully_resolved;
}

TML_EXPORT void ffi_backtrace_clear_cache(void) {
    // Clear global symbol cache
    symbol_cache_clear();
    // On Windows, no DbgHelp state to reset (DbgHelp is disabled)
    // On Unix, dladdr doesn't have a cache to clear
}
