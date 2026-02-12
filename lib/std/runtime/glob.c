// TML Standard Library - Glob Runtime Implementation
// High-performance glob pattern matching and directory walking
//
// Pattern syntax:
//   *      - match any characters within a single path segment (no separators)
//   ?      - match exactly one character
//   **     - match zero or more directories (recursive)
//   [abc]  - match any character in the set
//   [a-z]  - match any character in the range
//   [!abc] - match any character NOT in the set
//   {a,b}  - match any of the comma-separated alternatives

#include "glob.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

// ============================================================================
// Internal: Dynamic path array
// ============================================================================

#define GLOB_INITIAL_CAPACITY 64

static TmlGlobResult* glob_result_new(void) {
    TmlGlobResult* r = (TmlGlobResult*)malloc(sizeof(TmlGlobResult));
    if (!r)
        return NULL;
    r->paths = (char**)malloc(GLOB_INITIAL_CAPACITY * sizeof(char*));
    if (!r->paths) {
        free(r);
        return NULL;
    }
    r->count = 0;
    r->capacity = GLOB_INITIAL_CAPACITY;
    r->cursor = 0;
    return r;
}

static bool glob_result_push(TmlGlobResult* r, const char* path) {
    if (r->count >= r->capacity) {
        int64_t new_cap = r->capacity * 2;
        char** new_paths = (char**)realloc(r->paths, (size_t)new_cap * sizeof(char*));
        if (!new_paths)
            return false;
        r->paths = new_paths;
        r->capacity = new_cap;
    }
    size_t len = strlen(path);
    char* copy = (char*)malloc(len + 1);
    if (!copy)
        return false;
    memcpy(copy, path, len + 1);
    r->paths[r->count++] = copy;
    return true;
}

// ============================================================================
// Internal: Path separator helpers
// ============================================================================

static bool is_sep(char c) {
    return c == '/' || c == '\\';
}

static void normalize_path(char* path) {
    for (char* p = path; *p; p++) {
        if (*p == '\\')
            *p = '/';
    }
}

// ============================================================================
// Internal: Segment-level pattern matching
// ============================================================================

// Match a single segment (no path separators) against a glob pattern segment.
// Supports: * ? [abc] [a-z] [!abc] {a,b}
static bool match_segment(const char* pattern, const char* text) {
    const char* p = pattern;
    const char* t = text;

    while (*p && *t) {
        if (*p == '*') {
            // * matches zero or more characters within segment
            p++;
            if (!*p)
                return true; // trailing * matches rest
            // Try matching the rest from every position
            while (*t) {
                if (match_segment(p, t))
                    return true;
                t++;
            }
            return match_segment(p, t); // try matching with empty remaining
        } else if (*p == '?') {
            // ? matches exactly one character
            p++;
            t++;
        } else if (*p == '[') {
            // Character class
            p++;
            bool negate = false;
            if (*p == '!' || *p == '^') {
                negate = true;
                p++;
            }
            bool matched = false;
            char prev = 0;
            while (*p && *p != ']') {
                if (*p == '-' && prev && *(p + 1) && *(p + 1) != ']') {
                    // Range: [a-z]
                    p++;
                    if (*t >= prev && *t <= *p)
                        matched = true;
                    prev = *p;
                    p++;
                } else {
                    if (*t == *p)
                        matched = true;
                    prev = *p;
                    p++;
                }
            }
            if (*p == ']')
                p++;
            if (negate)
                matched = !matched;
            if (!matched)
                return false;
            t++;
        } else if (*p == '{') {
            // Alternation: {a,b,c}
            p++;
            const char* start = p;
            int depth = 1;
            // Find the end of this group
            const char* end = p;
            while (*end && depth > 0) {
                if (*end == '{')
                    depth++;
                else if (*end == '}')
                    depth--;
                if (depth > 0)
                    end++;
            }
            // Try each alternative
            const char* alt_start = start;
            while (alt_start < end) {
                // Find next comma or end
                const char* alt_end = alt_start;
                int d = 0;
                while (alt_end < end) {
                    if (*alt_end == '{')
                        d++;
                    else if (*alt_end == '}')
                        d--;
                    else if (*alt_end == ',' && d == 0)
                        break;
                    alt_end++;
                }
                // Build sub-pattern: alternative + rest after '}'
                size_t alt_len = (size_t)(alt_end - alt_start);
                const char* rest = (*end == '}') ? end + 1 : end;
                size_t rest_len = strlen(rest);
                size_t sub_len = alt_len + rest_len;
                char* sub = (char*)malloc(sub_len + 1);
                if (sub) {
                    memcpy(sub, alt_start, alt_len);
                    memcpy(sub + alt_len, rest, rest_len);
                    sub[sub_len] = '\0';
                    bool ok = match_segment(sub, t);
                    free(sub);
                    if (ok)
                        return true;
                }
                alt_start = (alt_end < end) ? alt_end + 1 : end;
            }
            return false;
        } else {
            // Literal character match (case-sensitive on POSIX, insensitive on Windows for FS)
            if (*p != *t)
                return false;
            p++;
            t++;
        }
    }

    // Handle trailing *
    while (*p == '*')
        p++;

    return !*p && !*t;
}

// ============================================================================
// Internal: Pattern segment splitting
// ============================================================================

typedef struct {
    char** segments;
    int count;
    int capacity;
} SegmentList;

static SegmentList* segments_new(void) {
    SegmentList* s = (SegmentList*)malloc(sizeof(SegmentList));
    if (!s)
        return NULL;
    s->segments = (char**)malloc(32 * sizeof(char*));
    if (!s->segments) {
        free(s);
        return NULL;
    }
    s->count = 0;
    s->capacity = 32;
    return s;
}

static bool segments_push(SegmentList* s, const char* seg, size_t len) {
    if (s->count >= s->capacity) {
        int new_cap = s->capacity * 2;
        char** new_segs = (char**)realloc(s->segments, (size_t)new_cap * sizeof(char*));
        if (!new_segs)
            return false;
        s->segments = new_segs;
        s->capacity = new_cap;
    }
    char* copy = (char*)malloc(len + 1);
    if (!copy)
        return false;
    memcpy(copy, seg, len);
    copy[len] = '\0';
    s->segments[s->count++] = copy;
    return true;
}

static void segments_free(SegmentList* s) {
    if (!s)
        return;
    for (int i = 0; i < s->count; i++) {
        free(s->segments[i]);
    }
    free(s->segments);
    free(s);
}

static SegmentList* split_pattern(const char* pattern) {
    SegmentList* segs = segments_new();
    if (!segs)
        return NULL;

    const char* start = pattern;
    const char* p = pattern;
    while (*p) {
        if (is_sep(*p)) {
            if (p > start) {
                segments_push(segs, start, (size_t)(p - start));
            }
            start = p + 1;
        }
        p++;
    }
    if (p > start) {
        segments_push(segs, start, (size_t)(p - start));
    }
    return segs;
}

// ============================================================================
// Internal: Directory listing
// ============================================================================

typedef struct {
    char** entries;
    bool* is_dir;
    int count;
    int capacity;
} DirEntries;

static DirEntries* dir_entries_new(void) {
    DirEntries* d = (DirEntries*)malloc(sizeof(DirEntries));
    if (!d)
        return NULL;
    d->entries = (char**)malloc(64 * sizeof(char*));
    d->is_dir = (bool*)malloc(64 * sizeof(bool));
    if (!d->entries || !d->is_dir) {
        free(d->entries);
        free(d->is_dir);
        free(d);
        return NULL;
    }
    d->count = 0;
    d->capacity = 64;
    return d;
}

static bool dir_entries_push(DirEntries* d, const char* name, bool isdir) {
    if (d->count >= d->capacity) {
        int new_cap = d->capacity * 2;
        char** ne = (char**)realloc(d->entries, (size_t)new_cap * sizeof(char*));
        bool* nd = (bool*)realloc(d->is_dir, (size_t)new_cap * sizeof(bool));
        if (!ne || !nd) {
            free(ne);
            return false;
        }
        d->entries = ne;
        d->is_dir = nd;
        d->capacity = new_cap;
    }
    size_t len = strlen(name);
    char* copy = (char*)malloc(len + 1);
    if (!copy)
        return false;
    memcpy(copy, name, len + 1);
    d->entries[d->count] = copy;
    d->is_dir[d->count] = isdir;
    d->count++;
    return true;
}

static void dir_entries_free(DirEntries* d) {
    if (!d)
        return;
    for (int i = 0; i < d->count; i++) {
        free(d->entries[i]);
    }
    free(d->entries);
    free(d->is_dir);
    free(d);
}

static DirEntries* list_directory(const char* dir_path) {
    DirEntries* entries = dir_entries_new();
    if (!entries)
        return NULL;

#ifdef _WIN32
    char search_path[MAX_PATH + 4];
    size_t dir_len = strlen(dir_path);
    if (dir_len > MAX_PATH) {
        dir_entries_free(entries);
        return NULL;
    }
    memcpy(search_path, dir_path, dir_len);
    // Append \* for FindFirstFile
    if (dir_len > 0 && !is_sep(dir_path[dir_len - 1])) {
        search_path[dir_len] = '\\';
        search_path[dir_len + 1] = '*';
        search_path[dir_len + 2] = '\0';
    } else {
        search_path[dir_len] = '*';
        search_path[dir_len + 1] = '\0';
    }

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        dir_entries_free(entries);
        return NULL;
    }

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;
        bool isdir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        dir_entries_push(entries, fd.cFileName, isdir);
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
#else
    DIR* d = opendir(dir_path);
    if (!d) {
        dir_entries_free(entries);
        return NULL;
    }

    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        bool isdir = false;
#ifdef _DIRENT_HAVE_D_TYPE
        if (ent->d_type == DT_DIR) {
            isdir = true;
        } else if (ent->d_type == DT_UNKNOWN) {
#endif
            // Fallback to stat
            size_t path_len = strlen(dir_path) + 1 + strlen(ent->d_name) + 1;
            char* full_path = (char*)malloc(path_len);
            if (full_path) {
                snprintf(full_path, path_len, "%s/%s", dir_path, ent->d_name);
                struct stat st;
                if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                    isdir = true;
                }
                free(full_path);
            }
#ifdef _DIRENT_HAVE_D_TYPE
        }
#endif
        dir_entries_push(entries, ent->d_name, isdir);
    }

    closedir(d);
#endif

    return entries;
}

// ============================================================================
// Internal: Recursive glob walker
// ============================================================================

static void glob_walk(const char* current_dir, SegmentList* segments, int seg_idx,
                      TmlGlobResult* result) {
    if (seg_idx >= segments->count) {
        // All segments consumed — current_dir is a match
        glob_result_push(result, current_dir);
        return;
    }

    const char* seg = segments->segments[seg_idx];

    if (strcmp(seg, "**") == 0) {
        // Globstar: match zero or more directories
        // Try matching with zero directories (skip **)
        glob_walk(current_dir, segments, seg_idx + 1, result);

        // Try matching by descending into each subdirectory
        DirEntries* entries = list_directory(current_dir);
        if (!entries)
            return;

        for (int i = 0; i < entries->count; i++) {
            if (!entries->is_dir[i])
                continue;

            // Build full path for subdirectory
            size_t dir_len = strlen(current_dir);
            size_t name_len = strlen(entries->entries[i]);
            size_t full_len = dir_len + 1 + name_len;
            char* child_path = (char*)malloc(full_len + 1);
            if (!child_path)
                continue;

            memcpy(child_path, current_dir, dir_len);
            child_path[dir_len] = '/';
            memcpy(child_path + dir_len + 1, entries->entries[i], name_len);
            child_path[full_len] = '\0';

            // Recurse with same ** segment (consuming more directories)
            glob_walk(child_path, segments, seg_idx, result);
            free(child_path);
        }
        dir_entries_free(entries);
        return;
    }

    // Regular segment (may contain *, ?, [...], {...})
    DirEntries* entries = list_directory(current_dir);
    if (!entries)
        return;

    bool is_last_segment = (seg_idx + 1 >= segments->count);

    for (int i = 0; i < entries->count; i++) {
        if (!match_segment(seg, entries->entries[i]))
            continue;

        // Build full path
        size_t dir_len = strlen(current_dir);
        size_t name_len = strlen(entries->entries[i]);
        size_t full_len = dir_len + 1 + name_len;
        char* child_path = (char*)malloc(full_len + 1);
        if (!child_path)
            continue;

        memcpy(child_path, current_dir, dir_len);
        child_path[dir_len] = '/';
        memcpy(child_path + dir_len + 1, entries->entries[i], name_len);
        child_path[full_len] = '\0';

        if (is_last_segment) {
            // Final segment — this entry matches
            glob_result_push(result, child_path);
        } else if (entries->is_dir[i]) {
            // More segments to match — recurse into directory
            glob_walk(child_path, segments, seg_idx + 1, result);
        }

        free(child_path);
    }

    dir_entries_free(entries);
}

// ============================================================================
// Internal: qsort comparison for sorting results
// ============================================================================

static int path_compare(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

// ============================================================================
// Public API
// ============================================================================

TmlGlobResult* glob_match(const char* base_dir, const char* pattern) {
    if (!base_dir || !pattern)
        return NULL;

    TmlGlobResult* result = glob_result_new();
    if (!result)
        return NULL;

    // Normalize pattern separators
    size_t pat_len = strlen(pattern);
    char* norm_pattern = (char*)malloc(pat_len + 1);
    if (!norm_pattern) {
        glob_result_free(result);
        return NULL;
    }
    memcpy(norm_pattern, pattern, pat_len + 1);
    normalize_path(norm_pattern);

    // Split pattern into segments
    SegmentList* segments = split_pattern(norm_pattern);
    free(norm_pattern);
    if (!segments) {
        glob_result_free(result);
        return NULL;
    }

    // Normalize base_dir
    size_t dir_len = strlen(base_dir);
    char* norm_dir = (char*)malloc(dir_len + 1);
    if (!norm_dir) {
        segments_free(segments);
        glob_result_free(result);
        return NULL;
    }
    memcpy(norm_dir, base_dir, dir_len + 1);
    // Remove trailing separator
    while (dir_len > 1 && is_sep(norm_dir[dir_len - 1])) {
        norm_dir[--dir_len] = '\0';
    }

    // Walk directory tree
    glob_walk(norm_dir, segments, 0, result);

    free(norm_dir);
    segments_free(segments);

    // Sort results for deterministic output
    if (result->count > 1) {
        qsort(result->paths, (size_t)result->count, sizeof(char*), path_compare);
    }

    // Normalize all result paths to forward slashes
    for (int64_t i = 0; i < result->count; i++) {
        normalize_path(result->paths[i]);
    }

    return result;
}

const char* glob_result_next(TmlGlobResult* result) {
    if (!result || result->cursor >= result->count)
        return "";
    return result->paths[result->cursor++];
}

int64_t glob_result_count(TmlGlobResult* result) {
    if (!result)
        return 0;
    return result->count;
}

void glob_result_free(TmlGlobResult* result) {
    if (!result)
        return;
    for (int64_t i = 0; i < result->count; i++) {
        free(result->paths[i]);
    }
    free(result->paths);
    free(result);
}

bool glob_pattern_matches(const char* pattern, const char* text) {
    if (!pattern || !text)
        return false;

    // For path-style patterns (containing /), split both and match segment by segment
    // For simple patterns, just match directly
    bool has_sep = false;
    for (const char* p = pattern; *p; p++) {
        if (is_sep(*p)) {
            has_sep = true;
            break;
        }
    }

    if (!has_sep) {
        return match_segment(pattern, text);
    }

    // Path-style matching: split both into segments and match
    // Normalize both
    size_t pat_len = strlen(pattern);
    char* norm_pat = (char*)malloc(pat_len + 1);
    if (!norm_pat)
        return false;
    memcpy(norm_pat, pattern, pat_len + 1);
    normalize_path(norm_pat);

    size_t text_len = strlen(text);
    char* norm_text = (char*)malloc(text_len + 1);
    if (!norm_text) {
        free(norm_pat);
        return false;
    }
    memcpy(norm_text, text, text_len + 1);
    normalize_path(norm_text);

    SegmentList* pat_segs = split_pattern(norm_pat);
    SegmentList* text_segs = split_pattern(norm_text);
    free(norm_pat);
    free(norm_text);

    if (!pat_segs || !text_segs) {
        segments_free(pat_segs);
        segments_free(text_segs);
        return false;
    }

    // Match segments with ** support
    bool matched = false;
    int pi = 0, ti = 0;
    int pc = pat_segs->count, tc = text_segs->count;

    // Simple recursive approach for ** matching
    // (pattern_matches doesn't need to be as fast as glob_walk)
    typedef struct {
        int pi;
        int ti;
    } State;
    State stack[256];
    int stack_top = 0;
    stack[stack_top].pi = 0;
    stack[stack_top].ti = 0;
    stack_top++;

    while (stack_top > 0 && !matched) {
        stack_top--;
        pi = stack[stack_top].pi;
        ti = stack[stack_top].ti;

        while (pi < pc && ti < tc) {
            if (strcmp(pat_segs->segments[pi], "**") == 0) {
                // ** can match 0 or more segments
                // Push: try skipping ** (match 0)
                if (stack_top < 256) {
                    stack[stack_top].pi = pi + 1;
                    stack[stack_top].ti = ti;
                    stack_top++;
                }
                // Continue: consume one text segment with **
                ti++;
                continue;
            }
            if (!match_segment(pat_segs->segments[pi], text_segs->segments[ti])) {
                break;
            }
            pi++;
            ti++;
        }

        // Skip trailing ** segments
        while (pi < pc && strcmp(pat_segs->segments[pi], "**") == 0) {
            pi++;
        }

        if (pi >= pc && ti >= tc) {
            matched = true;
        }
    }

    segments_free(pat_segs);
    segments_free(text_segs);
    return matched;
}
