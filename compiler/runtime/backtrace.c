/**
 * @file backtrace.c
 * @brief TML Runtime - Stack Backtrace Implementation
 *
 * Cross-platform stack trace capture and symbol resolution.
 * Windows implementation uses RtlCaptureStackBackTrace + DbgHelp.
 */

#include "backtrace.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
// clang-format off
// IMPORTANT: windows.h must come before dbghelp.h for type definitions
#include <windows.h>
#include <dbghelp.h>
// clang-format on
#pragma comment(lib, "dbghelp.lib")
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
static CRITICAL_SECTION g_symbol_lock;
static int32_t g_lock_initialized = 0;
#endif

// ============================================================================
// Initialization
// ============================================================================

int32_t backtrace_init(void) {
    if (g_initialized) {
        return 0;
    }

#ifdef _WIN32
    g_process = GetCurrentProcess();

    // Initialize critical section for thread-safe symbol operations
    if (!g_lock_initialized) {
        InitializeCriticalSection(&g_symbol_lock);
        g_lock_initialized = 1;
    }

    // Initialize symbol handler
    // SYMOPT_LOAD_LINES enables line number information
    // SYMOPT_DEFERRED_LOADS improves startup time
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);

    if (!SymInitialize(g_process, NULL, TRUE)) {
        return -1;
    }
#endif

    g_initialized = 1;
    return 0;
}

void backtrace_cleanup(void) {
    if (!g_initialized) {
        return;
    }

#ifdef _WIN32
    SymCleanup(g_process);
    g_process = NULL;

    if (g_lock_initialized) {
        DeleteCriticalSection(&g_symbol_lock);
        g_lock_initialized = 0;
    }
#endif

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
    // First, capture all frames to know the total
    void* temp_frames[128];
    USHORT total_frames = RtlCaptureStackBackTrace(0, 128, temp_frames, NULL);

    // Skip this function plus user-specified skip
    int32_t total_skip = skip + 1;

    // Ensure we don't skip more frames than available
    if (total_skip >= (int32_t)total_frames) {
        total_skip = total_frames > 0 ? total_frames - 1 : 0;
    }

    USHORT captured = RtlCaptureStackBackTrace((ULONG)total_skip, (ULONG)max_frames, frames, NULL);
    return (int32_t)captured;
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

    Backtrace* bt = (Backtrace*)calloc(1, sizeof(Backtrace));
    if (!bt) {
        return NULL;
    }

    bt->capacity = BACKTRACE_MAX_FRAMES;
    bt->frames = (BacktraceFrame*)calloc(bt->capacity, sizeof(BacktraceFrame));
    if (!bt->frames) {
        free(bt);
        return NULL;
    }

    // Temporary array for raw addresses
    // Skip internal C frames (backtrace_capture_full, ffi_backtrace_capture)
    void* raw_frames[BACKTRACE_MAX_FRAMES];
    int32_t count = backtrace_capture(raw_frames, BACKTRACE_MAX_FRAMES, skip);

    if (count < 0) {
        free(bt->frames);
        free(bt);
        return NULL;
    }

    bt->frame_count = count;
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

#ifdef _WIN32
    EnterCriticalSection(&g_symbol_lock);

    // Allocate buffer for SYMBOL_INFO
    char symbol_buffer[sizeof(SYMBOL_INFO) + BACKTRACE_MAX_SYMBOL_NAME];
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbol_buffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = BACKTRACE_MAX_SYMBOL_NAME;

    DWORD64 displacement64 = 0;
    if (SymFromAddr(g_process, (DWORD64)addr, &displacement64, symbol)) {
        out->name = _strdup(symbol->Name);
        out->symbol_address = (void*)symbol->Address;
        out->offset = displacement64;
    }

    // Get line information
    IMAGEHLP_LINE64 line;
    memset(&line, 0, sizeof(line));
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    DWORD displacement32 = 0;
    if (SymGetLineFromAddr64(g_process, (DWORD64)addr, &displacement32, &line)) {
        out->filename = _strdup(line.FileName);
        out->lineno = line.LineNumber;
        out->colno = 0; // DbgHelp doesn't provide column info
    }

    LeaveCriticalSection(&g_symbol_lock);

    return (out->name != NULL) ? 0 : -1;
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

    // Estimate buffer size: ~200 bytes per frame
    size_t buffer_size = (size_t)bt->frame_count * 256 + 64;
    char* result = (char*)malloc(buffer_size);
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

        char frame_buf[512];
        int32_t len =
            backtrace_frame_format(&bt->frames[i], display_index, frame_buf, sizeof(frame_buf));
        if (len > 0) {
            int written = snprintf(ptr, remaining, "%s\n", frame_buf);
            if (written > 0 && (size_t)written < remaining) {
                ptr += written;
                remaining -= written;
                display_index++;
            }
        }
    }

    // If all frames were filtered, show a message
    if (display_index == 0) {
        free(result);
        return _strdup("  <all frames filtered as internal>\n");
    }

    return result;
}

void backtrace_print(int32_t skip) {
    Backtrace* bt = backtrace_capture_full(skip + 1); // +1 for this function
    if (!bt) {
        fprintf(stderr, "  <failed to capture backtrace>\n");
        return;
    }

    backtrace_resolve_all(bt);

    char* formatted = backtrace_format(bt);
    if (formatted) {
        fprintf(stderr, "%s", formatted);
        free(formatted);
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
#ifdef _WIN32
    // Re-initialize symbol handler to clear any cached data
    if (g_initialized) {
        backtrace_cleanup();
        backtrace_init();
    }
#endif
    // On Unix, dladdr doesn't have a cache to clear
}
