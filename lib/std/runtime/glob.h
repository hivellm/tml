// TML Standard Library - Glob Runtime
// High-performance glob pattern matching and directory walking

#ifndef STD_GLOB_H
#define STD_GLOB_H

#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#ifdef TML_GLOB_EXPORTS
#define TML_GLOB_API __declspec(dllexport)
#else
#define TML_GLOB_API
#endif
#else
#define TML_GLOB_API
#endif

// ============================================================================
// Glob Result Handle
// ============================================================================

typedef struct {
    char** paths;     // Array of matched path strings
    int64_t count;    // Number of matches
    int64_t capacity; // Allocated capacity
    int64_t cursor;   // Current iteration position
} TmlGlobResult;

// ============================================================================
// Glob API
// ============================================================================

// Find all files matching a glob pattern relative to base_dir.
// Pattern supports: * (any segment), ** (recursive), ? (char), [abc] (class)
// Returns a result handle that must be freed with glob_result_free().
TML_GLOB_API TmlGlobResult* glob_match(const char* base_dir, const char* pattern);

// Get the next matched path from results. Returns NULL when exhausted.
TML_GLOB_API const char* glob_result_next(TmlGlobResult* result);

// Get total count of matched paths.
TML_GLOB_API int64_t glob_result_count(TmlGlobResult* result);

// Free glob result and all contained strings.
TML_GLOB_API void glob_result_free(TmlGlobResult* result);

// Test if a text string matches a glob pattern (no directory walk).
// Only matches against the string itself, not the filesystem.
TML_GLOB_API bool glob_pattern_matches(const char* pattern, const char* text);

#endif // STD_GLOB_H
