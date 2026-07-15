// TML Standard Library - File I/O Runtime Implementation
// Implements: File operations (read, write, append)

#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <limits.h>
#include <windows.h>
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#define stat _stat
#define mkdir(path, mode) _mkdir(path)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#else
#include <dirent.h>
#include <unistd.h>
#ifndef MAX_PATH
#define MAX_PATH 4096
#endif
#endif

// ============================================================================
// File Operations
// ============================================================================

TmlFile* file_open(const char* path, int32_t mode) {
    if (!path)
        return NULL;

    const char* fmode;
    if (mode & TML_FILE_APPEND) {
        fmode = (mode & TML_FILE_READ) ? "a+b" : "ab";
    } else if (mode & TML_FILE_WRITE) {
        fmode = (mode & TML_FILE_READ) ? "w+b" : "wb";
    } else {
        fmode = "rb"; // Default to read
    }

    FILE* fp = fopen(path, fmode);
    if (!fp)
        return NULL;

    TmlFile* file = (TmlFile*)malloc(sizeof(TmlFile));
    if (!file) {
        fclose(fp);
        return NULL;
    }

    // Get file size
    fseek(fp, 0, SEEK_END);
    file->size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    file->handle = fp;
    file->position = 0;
    file->mode = mode;
    file->is_open = true;

    return file;
}

// Convenience wrappers for codegen (open with specific mode)
TmlFile* file_open_read(const char* path) {
    return file_open(path, TML_FILE_READ);
}

TmlFile* file_open_write(const char* path) {
    return file_open(path, TML_FILE_WRITE);
}

TmlFile* file_open_append(const char* path) {
    return file_open(path, TML_FILE_APPEND);
}

void file_close(TmlFile* file) {
    if (!file)
        return;
    if (file->is_open && file->handle) {
        fclose((FILE*)file->handle);
    }
    file->is_open = false;
    file->handle = NULL;
    free(file);
}

bool file_is_open(TmlFile* file) {
    return file && file->is_open;
}

int64_t file_read(TmlFile* file, uint8_t* buffer, int64_t size) {
    if (!file || !file->is_open || !buffer || size <= 0)
        return 0;
    size_t read = fread(buffer, 1, size, (FILE*)file->handle);
    file->position += read;
    return (int64_t)read;
}

char* file_read_all(const char* path) {
    if (!path)
        return NULL;

    FILE* fp = fopen(path, "rb");
    if (!fp)
        return NULL;

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }

    size_t read = fread(content, 1, size, fp);
    content[read] = '\0';
    fclose(fp);

    return content;
}

char* file_read_line(TmlFile* file) {
    if (!file || !file->is_open)
        return NULL;

    FILE* fp = (FILE*)file->handle;

    // Initial buffer
    size_t capacity = 256;
    size_t len = 0;
    char* line = (char*)malloc(capacity);
    if (!line)
        return NULL;

    int c;
    while ((c = fgetc(fp)) != EOF) {
        if (len + 1 >= capacity) {
            capacity *= 2;
            char* new_line = (char*)realloc(line, capacity);
            if (!new_line) {
                free(line);
                return NULL;
            }
            line = new_line;
        }

        if (c == '\n') {
            break;
        }
        if (c != '\r') { // Skip CR in CRLF
            line[len++] = (char)c;
        }
    }

    if (len == 0 && c == EOF) {
        free(line);
        return NULL;
    }

    line[len] = '\0';
    file->position = ftell(fp);
    return line;
}

int64_t file_write(TmlFile* file, const uint8_t* data, int64_t size) {
    if (!file || !file->is_open || !data || size <= 0)
        return 0;
    if (!(file->mode & (TML_FILE_WRITE | TML_FILE_APPEND)))
        return 0;

    size_t written = fwrite(data, 1, size, (FILE*)file->handle);
    file->position += written;
    if (file->position > file->size) {
        file->size = file->position;
    }
    return (int64_t)written;
}

bool file_write_str(TmlFile* file, const char* str) {
    if (!str)
        return false;
    size_t len = strlen(str);
    return file_write(file, (const uint8_t*)str, len) == (int64_t)len;
}

bool file_write_all(const char* path, const char* content) {
    if (!path || !content)
        return false;

    FILE* fp = fopen(path, "wb");
    if (!fp)
        return false;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    fclose(fp);

    return written == len;
}

bool file_append_all(const char* path, const char* content) {
    if (!path || !content)
        return false;

    FILE* fp = fopen(path, "ab");
    if (!fp)
        return false;

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    fclose(fp);

    return written == len;
}

bool file_flush(TmlFile* file) {
    if (!file || !file->is_open || !file->handle)
        return false;
    return fflush((FILE*)file->handle) == 0;
}

bool file_sync(TmlFile* file) {
    if (!file || !file->is_open || !file->handle)
        return false;
    // Flush CRT buffers first, then force OS to write to disk
    if (fflush((FILE*)file->handle) != 0)
        return false;
#ifdef _WIN32
    // _commit() is the Windows equivalent of fsync()
    return _commit(_fileno((FILE*)file->handle)) == 0;
#else
    return fsync(fileno((FILE*)file->handle)) == 0;
#endif
}

bool file_datasync(TmlFile* file) {
    if (!file || !file->is_open || !file->handle)
        return false;
    if (fflush((FILE*)file->handle) != 0)
        return false;
#ifdef _WIN32
    // Windows has no fdatasync; _commit() is equivalent to fsync()
    return _commit(_fileno((FILE*)file->handle)) == 0;
#else
#ifdef __linux__
    return fdatasync(fileno((FILE*)file->handle)) == 0;
#else
    // macOS/BSD: fall back to fsync
    return fsync(fileno((FILE*)file->handle)) == 0;
#endif
#endif
}

int64_t file_size(TmlFile* file) {
    return file ? file->size : 0;
}

int64_t file_position(TmlFile* file) {
    return file ? file->position : 0;
}

bool file_seek(TmlFile* file, int64_t position) {
    if (!file || !file->is_open)
        return false;
    if (fseek((FILE*)file->handle, (long)position, SEEK_SET) != 0)
        return false;
    file->position = position;
    return true;
}

bool file_seek_end(TmlFile* file) {
    if (!file || !file->is_open)
        return false;
    if (fseek((FILE*)file->handle, 0, SEEK_END) != 0)
        return false;
    file->position = file->size;
    return true;
}

void file_rewind(TmlFile* file) {
    if (file && file->is_open) {
        rewind((FILE*)file->handle);
        file->position = 0;
    }
}

// Seek with whence: 0=SEEK_SET, 1=SEEK_CUR, 2=SEEK_END
// Returns new absolute position, or -1 on error
int64_t file_seek_from(TmlFile* file, int64_t offset, int32_t whence) {
    if (!file || !file->is_open)
        return -1;
    int c_whence = SEEK_SET;
    if (whence == 1)
        c_whence = SEEK_CUR;
    else if (whence == 2)
        c_whence = SEEK_END;
    if (fseek((FILE*)file->handle, (long)offset, c_whence) != 0)
        return -1;
    long pos = ftell((FILE*)file->handle);
    if (pos < 0)
        return -1;
    file->position = (int64_t)pos;
    return file->position;
}

// ============================================================================
// Binary Read/Write Operations
// ============================================================================

// Read bytes from file into a buffer at the given pointer address
// Returns actual number of bytes read
int64_t file_read_bytes(TmlFile* file, int64_t buf_ptr, int64_t count) {
    if (!file || !file->is_open || !buf_ptr || count <= 0)
        return 0;
    size_t read = fread((void*)buf_ptr, 1, (size_t)count, (FILE*)file->handle);
    file->position += read;
    return (int64_t)read;
}

// Write bytes from a buffer at the given pointer address to file
// Returns actual number of bytes written
int64_t file_write_bytes(TmlFile* file, int64_t buf_ptr, int64_t count) {
    if (!file || !file->is_open || !buf_ptr || count <= 0)
        return 0;
    if (!(file->mode & (TML_FILE_WRITE | TML_FILE_APPEND)))
        return 0;
    size_t written = fwrite((void*)buf_ptr, 1, (size_t)count, (FILE*)file->handle);
    file->position += written;
    if (file->position > file->size) {
        file->size = file->position;
    }
    return (int64_t)written;
}

// Get size of a file by path (for pre-allocating buffer)
int64_t file_get_file_size(const char* path) {
    if (!path)
        return -1;
    struct stat st;
    if (stat(path, &st) != 0)
        return -1;
    return (int64_t)st.st_size;
}

// Read all bytes from a file into a buffer at the given pointer
// Caller must ensure buf_ptr has enough capacity (use file_get_file_size first)
// Returns actual bytes read
int64_t file_read_all_bytes(const char* path, int64_t buf_ptr, int64_t buf_capacity) {
    if (!path || !buf_ptr || buf_capacity <= 0)
        return 0;

    FILE* fp = fopen(path, "rb");
    if (!fp)
        return 0;

    size_t read = fread((void*)buf_ptr, 1, (size_t)buf_capacity, fp);
    fclose(fp);
    return (int64_t)read;
}

// Write all bytes from a buffer to a file (overwrites)
bool file_write_all_bytes(const char* path, int64_t buf_ptr, int64_t count) {
    if (!path || !buf_ptr || count <= 0)
        return false;

    FILE* fp = fopen(path, "wb");
    if (!fp)
        return false;

    size_t written = fwrite((void*)buf_ptr, 1, (size_t)count, fp);
    fclose(fp);
    return written == (size_t)count;
}

// ============================================================================
// Directory Listing Operations
// ============================================================================

TmlDirList* dir_list(const char* path) {
    if (!path)
        return NULL;

    TmlDirList* list = (TmlDirList*)malloc(sizeof(TmlDirList));
    if (!list)
        return NULL;

    list->count = 0;
    list->capacity = 32;
    list->entries = (TmlDirEntry*)malloc(sizeof(TmlDirEntry) * list->capacity);
    if (!list->entries) {
        free(list);
        return NULL;
    }

#ifdef _WIN32
    // Windows: FindFirstFile/FindNextFile
    size_t path_len = strlen(path);
    char* search_path = (char*)malloc(path_len + 3);
    if (!search_path) {
        free(list->entries);
        free(list);
        return NULL;
    }
    memcpy(search_path, path, path_len);
    // Append \* for search pattern
    if (path_len > 0 && (path[path_len - 1] == '/' || path[path_len - 1] == '\\')) {
        search_path[path_len] = '*';
        search_path[path_len + 1] = '\0';
    } else {
        search_path[path_len] = '\\';
        search_path[path_len + 1] = '*';
        search_path[path_len + 2] = '\0';
    }

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(search_path, &ffd);
    free(search_path);

    if (hFind == INVALID_HANDLE_VALUE)
        return list; // Return empty list, not NULL

    do {
        // Skip . and ..
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
            continue;

        // Grow if needed
        if (list->count >= list->capacity) {
            list->capacity *= 2;
            TmlDirEntry* new_entries =
                (TmlDirEntry*)realloc(list->entries, sizeof(TmlDirEntry) * list->capacity);
            if (!new_entries)
                break;
            list->entries = new_entries;
        }

        TmlDirEntry* entry = &list->entries[list->count];

        // Name
        size_t name_len = strlen(ffd.cFileName);
        entry->name = (char*)malloc(name_len + 1);
        if (!entry->name)
            break;
        memcpy(entry->name, ffd.cFileName, name_len + 1);

        // Full path
        size_t full_len = path_len + 1 + name_len;
        entry->path = (char*)malloc(full_len + 1);
        if (!entry->path) {
            free(entry->name);
            break;
        }
        memcpy(entry->path, path, path_len);
        entry->path[path_len] = '/';
        memcpy(entry->path + path_len + 1, ffd.cFileName, name_len + 1);

        entry->is_dir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        entry->is_file = !entry->is_dir;

        list->count++;
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);
#else
    // POSIX: opendir/readdir
    DIR* dir = opendir(path);
    if (!dir)
        return list;

    struct dirent* de;
    size_t path_len = strlen(path);

    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        if (list->count >= list->capacity) {
            list->capacity *= 2;
            TmlDirEntry* new_entries =
                (TmlDirEntry*)realloc(list->entries, sizeof(TmlDirEntry) * list->capacity);
            if (!new_entries)
                break;
            list->entries = new_entries;
        }

        TmlDirEntry* entry = &list->entries[list->count];

        size_t name_len = strlen(de->d_name);
        entry->name = (char*)malloc(name_len + 1);
        if (!entry->name)
            break;
        memcpy(entry->name, de->d_name, name_len + 1);

        size_t full_len = path_len + 1 + name_len;
        entry->path = (char*)malloc(full_len + 1);
        if (!entry->path) {
            free(entry->name);
            break;
        }
        memcpy(entry->path, path, path_len);
        entry->path[path_len] = '/';
        memcpy(entry->path + path_len + 1, de->d_name, name_len + 1);

        // Use stat for is_file/is_dir
        struct stat st;
        if (stat(entry->path, &st) == 0) {
            entry->is_file = S_ISREG(st.st_mode);
            entry->is_dir = S_ISDIR(st.st_mode);
        } else {
            entry->is_file = false;
            entry->is_dir = false;
        }

        list->count++;
    }

    closedir(dir);
#endif

    return list;
}

void dir_list_free(TmlDirList* list) {
    if (!list)
        return;
    for (int64_t i = 0; i < list->count; i++) {
        free(list->entries[i].name);
        free(list->entries[i].path);
    }
    free(list->entries);
    free(list);
}

int64_t dir_list_count(TmlDirList* list) {
    return list ? list->count : 0;
}

const char* dir_entry_name(TmlDirList* list, int64_t index) {
    if (!list || index < 0 || index >= list->count)
        return "";
    return list->entries[index].name;
}

const char* dir_entry_path(TmlDirList* list, int64_t index) {
    if (!list || index < 0 || index >= list->count)
        return "";
    return list->entries[index].path;
}

bool dir_entry_is_file(TmlDirList* list, int64_t index) {
    if (!list || index < 0 || index >= list->count)
        return false;
    return list->entries[index].is_file;
}

bool dir_entry_is_dir(TmlDirList* list, int64_t index) {
    if (!list || index < 0 || index >= list->count)
        return false;
    return list->entries[index].is_dir;
}

bool path_remove_dir_all(const char* path) {
    if (!path)
        return false;

#ifdef _WIN32
    size_t path_len = strlen(path);
    char* search_path = (char*)malloc(path_len + 3);
    if (!search_path)
        return false;
    memcpy(search_path, path, path_len);
    if (path_len > 0 && (path[path_len - 1] == '/' || path[path_len - 1] == '\\')) {
        search_path[path_len] = '*';
        search_path[path_len + 1] = '\0';
    } else {
        search_path[path_len] = '\\';
        search_path[path_len + 1] = '*';
        search_path[path_len + 2] = '\0';
    }

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(search_path, &ffd);
    free(search_path);

    if (hFind == INVALID_HANDLE_VALUE) {
        // Directory might already be empty or not exist
        return _rmdir(path) == 0;
    }

    bool success = true;
    do {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0)
            continue;

        size_t name_len = strlen(ffd.cFileName);
        char* child = (char*)malloc(path_len + 1 + name_len + 1);
        if (!child) {
            success = false;
            break;
        }
        memcpy(child, path, path_len);
        child[path_len] = '\\';
        memcpy(child + path_len + 1, ffd.cFileName, name_len + 1);

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!path_remove_dir_all(child))
                success = false;
        } else {
            if (remove(child) != 0)
                success = false;
        }
        free(child);
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);
    if (success)
        success = (_rmdir(path) == 0);
    return success;
#else
    DIR* dir = opendir(path);
    if (!dir)
        return false;

    size_t path_len = strlen(path);
    struct dirent* de;
    bool success = true;

    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        size_t name_len = strlen(de->d_name);
        char* child = (char*)malloc(path_len + 1 + name_len + 1);
        if (!child) {
            success = false;
            break;
        }
        memcpy(child, path, path_len);
        child[path_len] = '/';
        memcpy(child + path_len + 1, de->d_name, name_len + 1);

        struct stat st;
        if (stat(child, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (!path_remove_dir_all(child))
                success = false;
        } else {
            if (remove(child) != 0)
                success = false;
        }
        free(child);
    }

    closedir(dir);
    if (success)
        success = (rmdir(path) == 0);
    return success;
#endif
}

// ============================================================================
// File Metadata Operations
// ============================================================================

int64_t file_metadata_size(const char* path) {
    if (!path)
        return -1;
    struct stat st;
    if (stat(path, &st) != 0)
        return -1;
    return (int64_t)st.st_size;
}

int64_t file_metadata_modified(const char* path) {
    if (!path)
        return -1;
    struct stat st;
    if (stat(path, &st) != 0)
        return -1;
    return (int64_t)st.st_mtime;
}

bool file_metadata_is_readonly(const char* path) {
    if (!path)
        return false;
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path);
    if (attrs == INVALID_FILE_ATTRIBUTES)
        return false;
    return (attrs & FILE_ATTRIBUTE_READONLY) != 0;
#else
    struct stat st;
    if (stat(path, &st) != 0)
        return false;
    return (st.st_mode & S_IWUSR) == 0;
#endif
}

// ============================================================================
// Path Operations
// ============================================================================

bool path_exists(const char* path) {
    if (!path)
        return false;
    struct stat st;
    return stat(path, &st) == 0;
}

bool path_is_file(const char* path) {
    if (!path)
        return false;
    struct stat st;
    if (stat(path, &st) != 0)
        return false;
    return S_ISREG(st.st_mode);
}

bool path_is_dir(const char* path) {
    if (!path)
        return false;
    struct stat st;
    if (stat(path, &st) != 0)
        return false;
    return S_ISDIR(st.st_mode);
}

bool path_create_dir(const char* path) {
    if (!path)
        return false;
#ifdef _WIN32
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
#endif
}

bool path_create_dir_all(const char* path) {
    if (!path)
        return false;

    // Make a copy to modify
    size_t len = strlen(path);
    char* tmp = (char*)malloc(len + 1);
    if (!tmp)
        return false;
    strcpy(tmp, path);

    // Create each directory component
    for (size_t i = 1; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            tmp[i] = '\0';
            if (!path_exists(tmp)) {
                if (!path_create_dir(tmp)) {
                    free(tmp);
                    return false;
                }
            }
            tmp[i] = '/';
        }
    }

    // Create final directory
    bool result = path_exists(path) || path_create_dir(path);
    free(tmp);
    return result;
}

bool path_remove(const char* path) {
    if (!path)
        return false;
    return remove(path) == 0;
}

bool path_remove_dir(const char* path) {
    if (!path)
        return false;
#ifdef _WIN32
    return _rmdir(path) == 0;
#else
    return rmdir(path) == 0;
#endif
}

bool path_rename(const char* from, const char* to) {
    if (!from || !to)
        return false;
    return rename(from, to) == 0;
}

bool path_copy(const char* from, const char* to) {
    if (!from || !to)
        return false;

    FILE* src = fopen(from, "rb");
    if (!src)
        return false;

    FILE* dst = fopen(to, "wb");
    if (!dst) {
        fclose(src);
        return false;
    }

    char buffer[8192];
    size_t bytes;
    bool success = true;

    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            success = false;
            break;
        }
    }

    fclose(src);
    fclose(dst);
    return success;
}

char* path_join(const char* base, const char* child) {
    if (!base || !child)
        return NULL;

    size_t base_len = strlen(base);
    size_t child_len = strlen(child);

    // Remove trailing separator from base
    while (base_len > 0 && (base[base_len - 1] == '/' || base[base_len - 1] == '\\')) {
        base_len--;
    }

    // Remove leading separator from child
    while (*child == '/' || *child == '\\') {
        child++;
        child_len--;
    }

    char* result = (char*)malloc(base_len + 1 + child_len + 1);
    if (!result)
        return NULL;

    memcpy(result, base, base_len);
    result[base_len] = '/';
    memcpy(result + base_len + 1, child, child_len);
    result[base_len + 1 + child_len] = '\0';

    return result;
}

char* path_parent(const char* path) {
    if (!path)
        return NULL;

    size_t len = strlen(path);
    if (len == 0)
        return NULL;

    // Skip trailing separators
    while (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        len--;
    }

    // Find last separator
    size_t i = len;
    while (i > 0 && path[i - 1] != '/' && path[i - 1] != '\\') {
        i--;
    }

    if (i == 0)
        return NULL; // No parent

    // Skip the separator
    i--;

    // Handle root path
    if (i == 0)
        i = 1;

    char* result = (char*)malloc(i + 1);
    if (!result)
        return NULL;
    memcpy(result, path, i);
    result[i] = '\0';

    return result;
}

char* path_filename(const char* path) {
    if (!path)
        return NULL;

    size_t len = strlen(path);
    if (len == 0)
        return NULL;

    // Skip trailing separators
    while (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        len--;
    }

    // Find last separator
    size_t start = len;
    while (start > 0 && path[start - 1] != '/' && path[start - 1] != '\\') {
        start--;
    }

    size_t name_len = len - start;
    char* result = (char*)malloc(name_len + 1);
    if (!result)
        return NULL;
    memcpy(result, path + start, name_len);
    result[name_len] = '\0';

    return result;
}

char* path_extension(const char* path) {
    if (!path)
        return NULL;

    const char* filename = path;
    const char* sep = strrchr(path, '/');
    if (!sep)
        sep = strrchr(path, '\\');
    if (sep)
        filename = sep + 1;

    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename)
        return NULL;

    size_t len = strlen(dot);
    char* result = (char*)malloc(len + 1);
    if (!result)
        return NULL;
    strcpy(result, dot);

    return result;
}

char* path_absolute(const char* path) {
    if (!path)
        return NULL;

#ifdef _WIN32
    char* result = (char*)malloc(MAX_PATH);
    if (!result)
        return NULL;
    if (_fullpath(result, path, MAX_PATH) == NULL) {
        free(result);
        return NULL;
    }
    return result;
#else
    char* result = realpath(path, NULL);
    return result;
#endif
}
