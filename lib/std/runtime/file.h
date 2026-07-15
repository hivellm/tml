// TML Standard Library - File I/O Runtime
// Implements: File operations (read, write, append)

#ifndef STD_FILE_H
#define STD_FILE_H

#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// File Handle
// ============================================================================

typedef struct {
    void* handle;     // FILE* pointer
    int64_t size;     // File size (cached on open)
    int64_t position; // Current position
    int32_t mode;     // Open mode (read=1, write=2, append=4)
    bool is_open;
} TmlFile;

// File open modes
#define TML_FILE_READ 1
#define TML_FILE_WRITE 2
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
char* file_read_all(const char* path); // Returns malloc'd string
char* file_read_line(TmlFile* file);   // Returns malloc'd string

// Write operations
int64_t file_write(TmlFile* file, const uint8_t* data, int64_t size);
bool file_write_str(TmlFile* file, const char* str);
bool file_write_all(const char* path, const char* content);
bool file_append_all(const char* path, const char* content);

// Flush / Sync
bool file_flush(TmlFile* file);
bool file_sync(TmlFile* file);     // fsync() — flush to disk
bool file_datasync(TmlFile* file); // fdatasync() — flush data only (no metadata)

// Position/Size
int64_t file_size(TmlFile* file);
int64_t file_position(TmlFile* file);
bool file_seek(TmlFile* file, int64_t position);
bool file_seek_end(TmlFile* file);
void file_rewind(TmlFile* file);
int64_t file_seek_from(TmlFile* file, int64_t offset, int32_t whence);

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

// Binary read/write
int64_t file_read_bytes(TmlFile* file, int64_t buf_ptr, int64_t count);
int64_t file_write_bytes(TmlFile* file, int64_t buf_ptr, int64_t count);
int64_t file_read_all_bytes(const char* path, int64_t buf_ptr, int64_t buf_capacity);
int64_t file_get_file_size(const char* path);
bool file_write_all_bytes(const char* path, int64_t buf_ptr, int64_t count);

// Directory listing
typedef struct {
    char* name;
    char* path;
    bool is_file;
    bool is_dir;
} TmlDirEntry;

typedef struct {
    TmlDirEntry* entries;
    int64_t count;
    int64_t capacity;
} TmlDirList;

TmlDirList* dir_list(const char* path);
void dir_list_free(TmlDirList* list);
int64_t dir_list_count(TmlDirList* list);
const char* dir_entry_name(TmlDirList* list, int64_t index);
const char* dir_entry_path(TmlDirList* list, int64_t index);
bool dir_entry_is_file(TmlDirList* list, int64_t index);
bool dir_entry_is_dir(TmlDirList* list, int64_t index);
bool path_remove_dir_all(const char* path);

// File metadata
int64_t file_metadata_size(const char* path);
int64_t file_metadata_modified(const char* path);
bool file_metadata_is_readonly(const char* path);

// Path manipulation (returns malloc'd strings)
char* path_join(const char* base, const char* child);
char* path_parent(const char* path);
char* path_filename(const char* path);
char* path_extension(const char* path);
char* path_absolute(const char* path);

#endif // STD_FILE_H
