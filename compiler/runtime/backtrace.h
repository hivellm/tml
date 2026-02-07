/**
 * @file backtrace.h
 * @brief TML Runtime - Stack Backtrace Support
 *
 * Provides cross-platform stack trace capture and symbol resolution.
 * Used by panic handlers and debugging utilities.
 *
 * ## Platform Support
 * - Windows: RtlCaptureStackBackTrace + DbgHelp (SymFromAddr, SymGetLineFromAddr64)
 * - Linux/macOS: _Unwind_Backtrace + dladdr (future)
 *
 * ## Usage
 * ```c
 * void* frames[64];
 * int count = backtrace_capture(frames, 64, 0);
 *
 * for (int i = 0; i < count; i++) {
 *     BacktraceSymbol sym;
 *     if (backtrace_resolve(frames[i], &sym) == 0) {
 *         printf("%d: %s at %s:%d\n", i, sym.name, sym.filename, sym.lineno);
 *         backtrace_symbol_free(&sym);
 *     }
 * }
 * ```
 */

#ifndef TML_BACKTRACE_H
#define TML_BACKTRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Configuration
// ============================================================================

/** Maximum number of frames to capture */
#define BACKTRACE_MAX_FRAMES 128

/** Maximum symbol name length */
#define BACKTRACE_MAX_SYMBOL_NAME 512

/** Maximum filename length */
#define BACKTRACE_MAX_FILENAME 1024

// ============================================================================
// Types
// ============================================================================

/**
 * @brief Resolved symbol information for a stack frame.
 *
 * Contains the demangled function name, source file path, and line/column
 * numbers if debug information is available.
 */
typedef struct BacktraceSymbol {
    /** Demangled function name (heap-allocated, call backtrace_symbol_free) */
    char* name;

    /** Source file path (heap-allocated, may be NULL) */
    char* filename;

    /** Line number in source file (0 if unknown) */
    uint32_t lineno;

    /** Column number in source file (0 if unknown) */
    uint32_t colno;

    /** Symbol address (start of function) */
    void* symbol_address;

    /** Offset from symbol start */
    uint64_t offset;
} BacktraceSymbol;

/**
 * @brief A captured stack frame.
 */
typedef struct BacktraceFrame {
    /** Instruction pointer (return address) */
    void* ip;

    /** Stack pointer (may be NULL on some platforms) */
    void* sp;

    /** Whether symbol has been resolved */
    int32_t resolved;

    /** Resolved symbol info (valid only if resolved == 1) */
    BacktraceSymbol symbol;
} BacktraceFrame;

/**
 * @brief A complete backtrace with multiple frames.
 */
typedef struct Backtrace {
    /** Array of frames */
    BacktraceFrame* frames;

    /** Number of captured frames */
    int32_t frame_count;

    /** Capacity of frames array */
    int32_t capacity;

    /** Whether all frames have been resolved */
    int32_t fully_resolved;
} Backtrace;

// ============================================================================
// Initialization
// ============================================================================

/**
 * @brief Initialize the backtrace subsystem.
 *
 * On Windows, this calls SymInitialize to load debug symbols.
 * Safe to call multiple times - subsequent calls are no-ops.
 *
 * @return 0 on success, -1 on failure
 */
int32_t backtrace_init(void);

/**
 * @brief Cleanup the backtrace subsystem.
 *
 * On Windows, this calls SymCleanup to release debug symbol resources.
 */
void backtrace_cleanup(void);

// ============================================================================
// Capture Functions
// ============================================================================

/**
 * @brief Capture raw stack frame addresses.
 *
 * @param frames Output array to store frame addresses
 * @param max_frames Maximum number of frames to capture
 * @param skip Number of initial frames to skip (0 = include this function)
 * @return Number of frames captured, or -1 on error
 */
int32_t backtrace_capture(void** frames, int32_t max_frames, int32_t skip);

/**
 * @brief Capture a complete backtrace structure.
 *
 * Allocates a Backtrace structure and captures the current stack.
 * The caller must free the result with backtrace_free().
 *
 * @param skip Number of initial frames to skip
 * @return Allocated Backtrace, or NULL on failure
 */
Backtrace* backtrace_capture_full(int32_t skip);

// ============================================================================
// Resolution Functions
// ============================================================================

/**
 * @brief Resolve a single address to symbol information.
 *
 * @param addr The instruction pointer address to resolve
 * @param out Output symbol structure (caller must call backtrace_symbol_free)
 * @return 0 on success, -1 if symbol not found
 */
int32_t backtrace_resolve(void* addr, BacktraceSymbol* out);

/**
 * @brief Resolve all frames in a backtrace.
 *
 * Lazily resolves symbol information for each frame.
 * Safe to call multiple times - already-resolved frames are skipped.
 *
 * @param bt The backtrace to resolve
 * @return Number of successfully resolved frames
 */
int32_t backtrace_resolve_all(Backtrace* bt);

// ============================================================================
// Formatting Functions
// ============================================================================

/**
 * @brief Format a single frame as a string.
 *
 * Format: "  N: function_name\n         at filename:line"
 *
 * @param frame The frame to format
 * @param index Frame index for numbering
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written, or -1 on error
 */
int32_t backtrace_frame_format(const BacktraceFrame* frame, int32_t index, char* buffer,
                               int32_t buffer_size);

/**
 * @brief Format a complete backtrace as a string.
 *
 * @param bt The backtrace to format
 * @return Heap-allocated string (caller must free), or NULL on failure
 */
char* backtrace_format(const Backtrace* bt);

/**
 * @brief Format a complete backtrace as a JSON string.
 *
 * Returns a JSON array of frame objects, each with:
 * - "index": frame index (int)
 * - "name": function/symbol name (string)
 * - "file": source file path (string or null)
 * - "line": line number (int or 0)
 * - "addr": instruction pointer as hex string
 *
 * @param bt The backtrace to format
 * @return Heap-allocated JSON string (caller must free), or NULL on failure
 */
char* backtrace_format_json(const Backtrace* bt);

/**
 * @brief Print a backtrace to stderr.
 *
 * Convenience function that captures, resolves, and prints a backtrace.
 *
 * @param skip Number of initial frames to skip
 */
void backtrace_print(int32_t skip);

// ============================================================================
// Memory Management
// ============================================================================

/**
 * @brief Free symbol resources.
 *
 * Frees the name and filename strings in a BacktraceSymbol.
 * Safe to call on zeroed or already-freed symbols.
 *
 * @param sym The symbol to free
 */
void backtrace_symbol_free(BacktraceSymbol* sym);

/**
 * @brief Free a complete backtrace.
 *
 * Frees all frames, symbols, and the backtrace structure itself.
 *
 * @param bt The backtrace to free (may be NULL)
 */
void backtrace_free(Backtrace* bt);

// ============================================================================
// FFI Exports for TML
// ============================================================================

#ifdef _WIN32
#define TML_EXPORT __declspec(dllexport)
#else
#define TML_EXPORT __attribute__((visibility("default")))
#endif

/** FFI: Capture backtrace and return handle */
TML_EXPORT void* ffi_backtrace_capture(int32_t skip);

/** FFI: Get frame count */
TML_EXPORT int32_t ffi_backtrace_frame_count(void* bt_handle);

/** FFI: Get frame IP at index */
TML_EXPORT void* ffi_backtrace_frame_ip(void* bt_handle, int32_t index);

/** FFI: Resolve all symbols */
TML_EXPORT void ffi_backtrace_resolve(void* bt_handle);

/** FFI: Get symbol name for frame (returns static buffer or NULL) */
TML_EXPORT const char* ffi_backtrace_frame_name(void* bt_handle, int32_t index);

/** FFI: Get filename for frame (returns static buffer or NULL) */
TML_EXPORT const char* ffi_backtrace_frame_filename(void* bt_handle, int32_t index);

/** FFI: Get line number for frame (0 if unknown) */
TML_EXPORT uint32_t ffi_backtrace_frame_lineno(void* bt_handle, int32_t index);

/** FFI: Format backtrace to string (caller must free) */
TML_EXPORT char* ffi_backtrace_to_string(void* bt_handle);

/** FFI: Free backtrace handle */
TML_EXPORT void ffi_backtrace_free(void* bt_handle);

/** FFI: Get column number for frame (0 if unknown) */
TML_EXPORT uint32_t ffi_backtrace_frame_colno(void* bt_handle, int32_t index);

/** FFI: Get symbol address for frame (start of function) */
TML_EXPORT void* ffi_backtrace_frame_symbol_address(void* bt_handle, int32_t index);

/** FFI: Get offset from symbol start for frame */
TML_EXPORT uint64_t ffi_backtrace_frame_offset(void* bt_handle, int32_t index);

/** FFI: Check if backtrace is fully resolved */
TML_EXPORT int32_t ffi_backtrace_is_resolved(void* bt_handle);

/** FFI: Clear symbol cache (re-initializes symbol handler) */
TML_EXPORT void ffi_backtrace_clear_cache(void);

#ifdef __cplusplus
}
#endif

#endif // TML_BACKTRACE_H
