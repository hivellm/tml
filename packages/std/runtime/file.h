// TML Standard Library - File I/O Runtime
// Implements: File operations (read, write, append)

#ifndef STD_FILE_H
#define STD_FILE_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// File Handle
// ============================================================================

typedef struct {
    void* handle;       // FILE* pointer
    int64_t size;       // File size (cached on open)
    int64_t position;   // Current position
    int32_t mode;       // Open mode (read=1, write=2, append=4)
    bool is_open;
} TmlFile;

// File open modes
#define TML_FILE_READ   1
#define TML_FILE_WRITE  2
#define TML_FILE_APPEND 4

// ============================================================================
// File Operations
// ============================================================================

// Open/Close
TmlFile* file_open(const char* path, int32_t mode);
TmlFile* file_open_read(const char* path);   // Convenience wrapper
TmlFile* file_open_write(const char* path);  // Convenience wrapper
TmlFile* file_open_append(const char* path); // Convenience wrapper
void file_close(TmlFile* file);
bool file_is_open(TmlFile* file);

// Read operations
int64_t file_read(TmlFile* file, uint8_t* buffer, int64_t size);
char* file_read_all(const char* path);           // Returns malloc'd string
char* file_read_line(TmlFile* file);             // Returns malloc'd string

// Write operations
int64_t file_write(TmlFile* file, const uint8_t* data, int64_t size);
bool file_write_str(TmlFile* file, const char* str);
bool file_write_all(const char* path, const char* content);
bool file_append_all(const char* path, const char* content);

// Position/Size
int64_t file_size(TmlFile* file);
int64_t file_position(TmlFile* file);
bool file_seek(TmlFile* file, int64_t position);
bool file_seek_end(TmlFile* file);
void file_rewind(TmlFile* file);

// ============================================================================
// Path Operations
// ============================================================================

bool path_exists(const char* path);
bool path_is_file(const char* path);
bool path_is_dir(const char* path);
bool path_create_dir(const char* path);
bool path_create_dir_all(const char* path);
bool path_remove(const char* path);
bool path_remove_dir(const char* path);
bool path_rename(const char* from, const char* to);
bool path_copy(const char* from, const char* to);

// Path manipulation (returns malloc'd strings)
char* path_join(const char* base, const char* child);
char* path_parent(const char* path);
char* path_filename(const char* path);
char* path_extension(const char* path);
char* path_absolute(const char* path);

#endif // STD_FILE_H
